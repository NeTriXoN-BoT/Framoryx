#include "shared.hpp"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <atomic>
#include <algorithm>
#include <array>
#include <vector>
#include "MinHook.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "dx12_bridge.hpp"

namespace {
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT,
                                               const DXGI_PRESENT_PARAMETERS*);
PresentFn originalPresent = nullptr;
Present1Fn originalPresent1 = nullptr;
using ExecuteFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
ExecuteFn originalExecute = nullptr;
Dx12Bridge bridge;
bool guiIs12 = false;
ULONGLONG guiRetry = 0;
void STDMETHODCALLTYPE execute(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
fg::HookControl* control = nullptr;
std::atomic<void*> primary{nullptr};
std::atomic<bool> enabled{false};
std::atomic<LONGLONG> primaryLast{0};
LARGE_INTEGER frequency{};
thread_local bool inside = false;
LONGLONG lastStart = 0;
LONGLONG lastSuccessful = 0;
std::atomic<uint64_t> primaryPixels{0};
HANDLE timer = nullptr;
SRWLOCK paceLock = SRWLOCK_INIT;
SRWLOCK renderLock = SRWLOCK_INIT;
void STDMETHODCALLTYPE execute(ID3D12CommandQueue* queue, UINT count,
                               ID3D12CommandList* const* lists) {
    if (enabled.load(std::memory_order_relaxed) && control && fg::read32(&control->overlay))
        bridge.observe(queue);
    originalExecute(queue, count, lists);
}
ImGuiContext* gui = nullptr;
ID3D11Device* guiDevice = nullptr;
ID3D11DeviceContext* guiContext = nullptr;
void* guiChain = nullptr;
double smoothedFps = 0;
LONGLONG adaptiveWakeGuard=0;
LONGLONG previousDraw = 0;
LONGLONG hudSampleQpc = 0, hudSampleFrames = 0, hudLastGraphFrame = 0;
std::array<float, 120> hudTimes{};
size_t hudHead = 0, hudCount = 0;
float fontScale = 0;
int fontStyle = -1;
float requestedFontScale=0;ULONGLONG requestedFontAt=0;
ImFont* largeFont = nullptr;
double rollingAverage = 0, rollingLow = 0;
std::vector<double> rollingIntervals;
bool prepareHudFonts(float scale, int style) {
    auto& io = ImGui::GetIO();
    if (std::abs(fontScale - scale) < .015f && fontStyle == style){io.FontGlobalScale=scale/fontScale;return true;}
    if(fontScale>0&&fontStyle==style){
        if(std::abs(requestedFontScale-scale)>.015f){requestedFontScale=scale;requestedFontAt=GetTickCount64();}
        if(GetTickCount64()-requestedFontAt<180){io.FontGlobalScale=scale/fontScale;return true;}
    }
    ImGui_ImplDX11_InvalidateDeviceObjects();
    io.Fonts->Clear();
    wchar_t windows[32768]{};
    GetWindowsDirectoryW(windows, 32768);
    std::wstring folder = std::wstring(windows) + L"\\Fonts\\";
    auto fontPath = [&](const wchar_t* name) {
        auto path = folder + name;
        int n = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(n, 0);
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, utf8.data(), n, nullptr, nullptr);
        return utf8;
    };
    auto regular = fontPath(L"segoeui.ttf"), bold = fontPath(L"segoeuib.ttf");
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    io.FontDefault =
        io.Fonts->AddFontFromFileTTF(regular.c_str(), ((style == 1||style==3) ? 12 : 14) * scale, &config);
    if (!io.FontDefault) {
        config.SizePixels = ((style == 1||style==3) ? 12 : 14) * scale;
        io.FontDefault = io.Fonts->AddFontDefault(&config);
    }
    largeFont = io.Fonts->AddFontFromFileTTF(bold.c_str(), (style == 0 ? 34 : 28) * scale, &config);
    if (!largeFont)
        largeFont = io.FontDefault;
    io.FontGlobalScale = 1;
    bool ok = ImGui_ImplDX11_CreateDeviceObjects();
    fontScale = ok ? scale : 0;
    fontStyle = style;
    return ok;
}
struct PreserveExtraShaderStages {
    ID3D11DeviceContext* context;
    ID3D11HullShader* hull = nullptr;
    ID3D11DomainShader* domain = nullptr;
    ID3D11ComputeShader* compute = nullptr;
    ID3D11ClassInstance *hullInstances[256]{}, *domainInstances[256]{}, *computeInstances[256]{};
    UINT hullCount = 256, domainCount = 256, computeCount = 256;
    explicit PreserveExtraShaderStages(ID3D11DeviceContext* ctx) : context(ctx) {
        context->HSGetShader(&hull, hullInstances, &hullCount);
        context->DSGetShader(&domain, domainInstances, &domainCount);
        context->CSGetShader(&compute, computeInstances, &computeCount);
    }
    ~PreserveExtraShaderStages() {
        context->HSSetShader(hull, hullInstances, hullCount);
        context->DSSetShader(domain, domainInstances, domainCount);
        context->CSSetShader(compute, computeInstances, computeCount);
        if (hull)
            hull->Release();
        if (domain)
            domain->Release();
        if (compute)
            compute->Release();
        for (UINT i = 0; i < hullCount; ++i)
            if (hullInstances[i])
                hullInstances[i]->Release();
        for (UINT i = 0; i < domainCount; ++i)
            if (domainInstances[i])
                domainInstances[i]->Release();
        for (UINT i = 0; i < computeCount; ++i)
            if (computeInstances[i])
                computeInstances[i]->Release();
    }
};

bool drawHud(IDXGISwapChain* chain, UINT flags) {
    if (!enabled || !control || !fg::read32(&control->overlay) || primary.load() != chain ||
        (flags & DXGI_PRESENT_TEST) || !fg::read32(&control->hudFields))
        return false;
    AcquireSRWLockExclusive(&renderLock);
    bool drawn = false;
    auto previousContext = ImGui::GetCurrentContext();
    if (guiChain != chain || (!gui && GetTickCount64() >= guiRetry)) {
        if (gui) {
            ImGui::SetCurrentContext(gui);
            ImGui_ImplDX11_Shutdown();
            ImGui::DestroyContext(gui);
            gui = nullptr;
        }
        if (guiContext) {
            guiContext->Release();
            guiContext = nullptr;
        }
        if (guiDevice) {
            guiDevice->Release();
            guiDevice = nullptr;
        }
        bridge.reset();
        guiIs12 = false;
        guiRetry = GetTickCount64() + 2000;
        guiChain = chain;
        hudSampleQpc = 0;
        hudSampleFrames = 0;
        hudLastGraphFrame = 0;
        hudHead = 0;
        hudCount = 0;
        smoothedFps = 0;
        fontScale = 0;
        largeFont = nullptr;
        rollingAverage = rollingLow = 0;
        if (SUCCEEDED(
                chain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&guiDevice))))
            guiDevice->GetImmediateContext(&guiContext);
        else
            guiIs12 = bridge.initialize(chain, &guiDevice, &guiContext);
        if (guiDevice && guiContext) {
            gui = ImGui::CreateContext();
            ImGui::SetCurrentContext(gui);
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            io.FontGlobalScale = 1.35f;
            ImGui::StyleColorsDark();
            auto& style = ImGui::GetStyle();
            style.WindowRounding = 9;
            style.WindowPadding = ImVec2(14, 10);
            style.Colors[ImGuiCol_WindowBg] = ImVec4(0.045f, 0.07f, 0.09f, 0.95f);
            style.Colors[ImGuiCol_Border] = ImVec4(0.2f, 0.35f, 0.32f, 0.7f);
            style.Colors[ImGuiCol_PlotLines] = ImVec4(0.49f, 0.95f, 0.77f, 1);
            bool initialized = ImGui_ImplDX11_Init(guiDevice, guiContext);
            bool ready = initialized && ImGui_ImplDX11_CreateDeviceObjects();
            if (!ready) {
                if (initialized)
                    ImGui_ImplDX11_Shutdown();
                ImGui::DestroyContext(gui);
                gui = nullptr;
            }
            InterlockedExchange(&control->renderer, gui ? (guiIs12 ? 12 : 11) : -1);
        } else
            InterlockedExchange(&control->renderer,
                                12); // Waiting for an unambiguous DX12 direct queue.
    }
    if (gui && (!guiIs12 || bridge.validQueue())) {
        ImGui::SetCurrentContext(gui);
        ID3D11Texture2D* buffer = nullptr;
        ID3D11RenderTargetView* view = nullptr;
        if (guiIs12)
            buffer = bridge.acquire();
        else
            chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&buffer));
        if (buffer) {
            D3D11_TEXTURE2D_DESC desc{};
            buffer->GetDesc(&desc);
            fg::HudOptions presentation{
                uint32_t(fg::read32(&control->hudFields)), int(fg::read32(&control->hudScale)),
                int(fg::read32(&control->hudOpacity)), int(fg::read32(&control->hudAccent)),
                int(fg::read32(&control->hudStyle))};
            presentation.order=uint64_t(uint32_t(fg::read32(&control->hudOrderLo)))|(uint64_t(uint32_t(fg::read32(&control->hudOrderHi)))<<32);
            presentation.x=fg::read32(&control->hudX);presentation.y=fg::read32(&control->hudY);presentation.fontScale=fg::read32(&control->hudFontScale);
            presentation.validate();
            float scale = fg::overlayScale(presentation.scale, int(desc.Height))*presentation.fontScale/100.f;
            scale = std::min(scale, std::min((desc.Width - 32.f) / fg::hudWidth(presentation),
                                             (desc.Height - 32.f) / fg::hudHeight(presentation)));
            if (desc.Width >= 160 && desc.Height >= 120 &&
                prepareHudFonts(scale, presentation.style) &&
                SUCCEEDED(guiDevice->CreateRenderTargetView(buffer, nullptr, &view))) {
                LARGE_INTEGER now{};
                QueryPerformanceCounter(&now);
                double dt = previousDraw ? double(now.QuadPart - previousDraw) / frequency.QuadPart
                                         : 1.0 / 60;
                previousDraw = now.QuadPart;
                auto successfulFrames = fg::read64(&control->frames);
                if (!hudSampleQpc) {
                    hudSampleQpc = now.QuadPart;
                    hudSampleFrames = successfulFrames;
                }
                if (now.QuadPart - hudSampleQpc >= frequency.QuadPart / 4) {
                    smoothedFps = double(successfulFrames - hudSampleFrames) * frequency.QuadPart /
                                  (now.QuadPart - hudSampleQpc);
                    hudSampleQpc = now.QuadPart;
                    hudSampleFrames = successfulFrames;
                    rollingIntervals.clear();
                    double sum = 0;
                    for (auto n = successfulFrames;
                         n > 0 && successfulFrames - n < LONGLONG(fg::hookRingSize); --n) {
                        auto& sample = control->ring[n % fg::hookRingSize];
                        if (fg::read64(&sample.sequence) != n)
                            continue;
                        auto at = fg::read64(&sample.qpc), ticks = fg::read64(&sample.interval);
                        if (now.QuadPart - at > frequency.QuadPart * 30)
                            break;
                        if (ticks > 0 && fg::read64(&sample.sequence) == n) {
                            double ms = 1000. * ticks / frequency.QuadPart;
                            rollingIntervals.push_back(ms);
                            sum += ms;
                        }
                    }
                    rollingAverage = sum > 0 ? 1000. * rollingIntervals.size() / sum : 0;
                    rollingLow = 0;
                    if (rollingIntervals.size() >= 100) {
                        auto count = size_t(std::ceil(rollingIntervals.size() * .01));
                        std::nth_element(rollingIntervals.begin(), rollingIntervals.begin() + count,
                                         rollingIntervals.end(), std::greater<double>());
                        double slow = 0;
                        for (size_t i = 0; i < count; ++i)
                            slow += rollingIntervals[i];
                        if (slow > 0)
                            rollingLow = 1000. * count / slow;
                    }
                }
                if (successfulFrames > hudLastGraphFrame) {
                    auto& last = control->ring[successfulFrames % fg::hookRingSize];
                    auto interval = fg::read64(&last.interval);
                    if (fg::read64(&last.sequence) == successfulFrames && interval > 0) {
                        hudTimes[hudHead] = float(1000.0 * interval / frequency.QuadPart);
                        hudHead = (hudHead + 1) % hudTimes.size();
                        hudCount = std::min(hudTimes.size(), hudCount + 1);
                    }
                    hudLastGraphFrame = successfulFrames;
                }
                auto& io = ImGui::GetIO();
                io.DisplaySize = ImVec2(float(desc.Width), float(desc.Height));
                io.DeltaTime = float(std::max(0.00001, dt));
                ImGui_ImplDX11_NewFrame();
                ImGui::NewFrame();
                const auto layout=fg::hudLayout(presentation);
                const bool compact=presentation.style==1||presentation.style==3,minimal=presentation.style==3;
                const auto rgb=fg::accentColors[presentation.accent];
                const ImVec4 accent(float((rgb>>16)&255)/255,float((rgb>>8)&255)/255,float(rgb&255)/255,1);
                auto& style=ImGui::GetStyle();style.WindowPadding=ImVec2(0,0);style.WindowRounding=(compact?8:14)*scale;
                style.Colors[ImGuiCol_PlotLines]=accent;style.Colors[ImGuiCol_FrameBg]=ImVec4(0,0,0,0);
                const int corner=fg::read32(&control->corner);const float margin=22*scale;
                const float width=layout.width*scale,height=layout.height*scale;
                float x=corner%2?io.DisplaySize.x-margin-width:margin,y=corner>=2?io.DisplaySize.y-margin-height:margin;
                if(presentation.x>=0)x=std::max(0.f,io.DisplaySize.x-width)*presentation.x/10000.f;
                if(presentation.y>=0)y=std::max(0.f,io.DisplaySize.y-height)*presentation.y/10000.f;
                ImGui::SetNextWindowPos(ImVec2(x,y),ImGuiCond_Always);ImGui::SetNextWindowSize(ImVec2(width,height));
                ImGui::SetNextWindowBgAlpha(presentation.opacity/100.f);
                ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(accent.x,accent.y,accent.z,presentation.opacity>0&&!minimal?.25f:0.f));
                ImGui::Begin("Framoryx HUD",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing);
                const auto pos=ImGui::GetWindowPos();
                if(!minimal&&presentation.opacity>0)ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x+14*scale,pos.y+1),ImVec2(pos.x+width-14*scale,pos.y+1),ImGui::ColorConvertFloat4ToU32(accent),2*scale);
                auto reading=[&](uint32_t field,char* output,size_t length){
                    const char* title="";double number=0;
                    switch(field){
                    case fg::HudFps:number=smoothedFps;break;
                    case fg::HudAverage:title="Average";number=rollingAverage;break;
                    case fg::HudLow:title="1% low";number=rollingLow;break;
                    case fg::HudTime:title="Frame time";number=smoothedFps>0?1000./smoothedFps:0;break;
                    case fg::HudCpu:title="System CPU";number=fg::read32(&control->cpuHundredths)/100.;break;
                    case fg::HudRam:title="System RAM";number=fg::read32(&control->ramMiB)/1024.;break;
                    case fg::HudCap:title="FPS limit";number=fg::read32(&control->cap);break;
                    case fg::HudApi:snprintf(output,length,"%s",guiIs12?"DX12":"DX11");return "Renderer";
                    default:
                        if(field>=fg::HudGpu){int sensor=0;while(sensor<6&&field!=(fg::HudGpu<<sensor))++sensor;static const char* titles[]{"GPU usage","VRAM usage","GPU temp","CPU temp","GPU power","GPU clock"};
                            if(sensor<6){const auto value=fg::read32(&control->sensors[sensor]);if(value<0)snprintf(output,length,"--");else if(sensor<2)snprintf(output,length,"%.0f%%",value/100.);else if(sensor<4)snprintf(output,length,"%.0f C",value/100.);else if(sensor==4)snprintf(output,length,"%.1f W",value/100.);else snprintf(output,length,"%.0f MHz",value/100.);return titles[sensor];}}
                        snprintf(output,length,"FRAMORYX");return "";
                    }
                    if(field==fg::HudCap&&!number)snprintf(output,length,"Uncapped");
                    else if(number>0||field==fg::HudCpu||field==fg::HudRam){
                        if(field==fg::HudTime)snprintf(output,length,"%.2f ms",number);
                        else if(field==fg::HudCpu)snprintf(output,length,"%.0f%%",number);
                        else if(field==fg::HudRam)snprintf(output,length,"%.1f GB",number);
                        else if(field==fg::HudCap||field==fg::HudFps)snprintf(output,length,"%.0f",number);
                        else snprintf(output,length,"%.0f FPS",number);
                    }
                    else snprintf(output,length,"--");return title;
                };
                for(size_t n=0;n<layout.count;++n){const auto& cell=layout.cells[n];
                    ImGui::SetCursorPos(ImVec2(cell.x*scale,cell.y*scale));
                    if(cell.field==fg::HudGraph){
                        if(hudCount){float maximum=33.4f;for(size_t i=0;i<hudCount;++i)maximum=std::max(maximum,std::min(100.f,hudTimes[i]));ImGui::PlotLines("##FrameTime",hudTimes.data(),int(hudCount),hudCount==hudTimes.size()?int(hudHead):0,nullptr,0,maximum,ImVec2(cell.width*scale,(cell.height-10)*scale));}continue;
                    }
                    char number[80]{};const char* title=reading(cell.field,number,sizeof(number));
                    if(cell.field==fg::HudFps){ImGui::PushFont(largeFont);ImGui::TextColored(accent,"%s FPS",number);ImGui::PopFont();continue;}
                    if(cell.field==fg::HudBrand){ImGui::TextDisabled("FRAMORYX");continue;}
                    ImGui::TextDisabled("%s",title);
                    const float numberX=cell.column?cell.x:cell.x+cell.width-105;
                    ImGui::SetCursorPos(ImVec2(numberX*scale,(cell.y+(cell.column?19:0))*scale));ImGui::TextUnformatted(number);
                }
                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::Render();
                ID3D11RenderTargetView* old[8]{};
                ID3D11DepthStencilView* depth = nullptr;
                guiContext->OMGetRenderTargets(8, old, &depth);
                guiContext->OMSetRenderTargets(1, &view, nullptr);
                {
                    PreserveExtraShaderStages preserve(guiContext);
                    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                }
                drawn = ImGui::GetDrawData()->TotalVtxCount > 0 &&
                        guiDevice->GetDeviceRemovedReason() == S_OK;
                guiContext->OMSetRenderTargets(8, old, depth);
                for (auto* item : old)
                    if (item)
                        item->Release();
                if (depth)
                    depth->Release();
                view->Release();
            }
            buffer->Release();
        }
        if (guiIs12)
            bridge.finish(guiContext);
        if (!drawn && GetTickCount64() >= guiRetry) {
            guiChain = nullptr;
            guiRetry = GetTickCount64() + 2000;
        }
    }
    ImGui::SetCurrentContext(previousContext);
    ReleaseSRWLockExclusive(&renderLock);
    return drawn;
}

LONGLONG pace(IDXGISwapChain* chain, UINT flags) {
    if (!enabled.load(std::memory_order_relaxed) || !control || (flags & DXGI_PRESENT_TEST))
        return 0;
    // A secondary UI/video swapchain must not wait for the primary game's cap.
    if(primary.load()&&primary.load()!=chain){
        LARGE_INTEGER at{};QueryPerformanceCounter(&at);
        if(at.QuadPart-primaryLast.load()<frequency.QuadPart*2){
            DXGI_SWAP_CHAIN_DESC surface{};uint64_t pixels=0;
            if(SUCCEEDED(chain->GetDesc(&surface))){UINT width=surface.BufferDesc.Width,height=surface.BufferDesc.Height;if(!width||!height){RECT rect{};GetClientRect(surface.OutputWindow,&rect);width=UINT(std::max(0L,rect.right));height=UINT(std::max(0L,rect.bottom));}pixels=uint64_t(width)*height;}
            if(pixels<640*360||pixels<=primaryPixels.load()*3/2)return 0;
        }
    }
    AcquireSRWLockExclusive(&paceLock);
    struct Unlock {
        ~Unlock() {
            ReleaseSRWLockExclusive(&paceLock);
        }
    } unlock;
    if (!enabled.load(std::memory_order_relaxed))
        return 0;
    // Lock onto the first presenting swapchain. Additional UI/video swapchains are not throttled or
    // counted.
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    if (primary.load() != chain) {
        DXGI_SWAP_CHAIN_DESC surface{};
        uint64_t pixels = 0;
        if (SUCCEEDED(chain->GetDesc(&surface))) {
            UINT width = surface.BufferDesc.Width, height = surface.BufferDesc.Height;
            if (!width || !height) {
                RECT rect{};
                GetClientRect(surface.OutputWindow, &rect);
                width = UINT(std::max(0L, rect.right));
                height = UINT(std::max(0L, rect.bottom));
            }
            if (width < 160 || height < 120)
                return 0;
            pixels = uint64_t(width) * height;
        }
        if (!primary.load() || now.QuadPart - primaryLast.load() > frequency.QuadPart * 2 ||
            (pixels >= 640 * 360 && pixels > primaryPixels * 3 / 2)) {
            primary = chain;
            primaryPixels = pixels;
            lastStart = 0;
            lastSuccessful = 0;
        } else
            return 0;
    }
    primaryLast = now.QuadPart;
    LONG cap = fg::read32(&control->cap);
    if (cap >= 10 && cap <= 1000 && lastStart) {
        LONGLONG step = frequency.QuadPart / cap;
        LONGLONG deadline = lastStart + step;
        // No backlog: a slow frame resets the schedule instead of causing a burst of catch-up
        // frames.
        if (now.QuadPart < deadline) {
            if (!timer)
                timer = CreateWaitableTimerExW(nullptr, nullptr, 0x2 /* high resolution */,
                                               TIMER_ALL_ACCESS);
            if (!timer)
                timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
            auto remaining = deadline - now.QuadPart;
            const LONGLONG minimumGuard=std::max<LONGLONG>(1,frequency.QuadPart/4000);
            const LONGLONG maximumGuard=std::max(minimumGuard,std::min(frequency.QuadPart/200,step/3));
            LONGLONG guard=std::clamp(adaptiveWakeGuard?adaptiveWakeGuard:minimumGuard,minimumGuard,maximumGuard);
            if (timer && remaining > guard * 2) {
                LARGE_INTEGER due{};
                due.QuadPart = -((remaining - guard) * 10000000LL / frequency.QuadPart);
                if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)){
                    WaitForSingleObject(timer, 200);
                    LARGE_INTEGER woke{};QueryPerformanceCounter(&woke);
                    const auto oversleep=std::max<LONGLONG>(0,woke.QuadPart-(deadline-guard));
                    // Learn this process's timer wakeup error rather than imposing a
                    // global timer resolution or carrying each oversleep into every frame.
                    adaptiveWakeGuard=std::clamp(std::max(oversleep+minimumGuard,guard*4/5),minimumGuard,maximumGuard);
                }
            }
            do {
                YieldProcessor();
                QueryPerformanceCounter(&now);
            } while (now.QuadPart < deadline && enabled.load(std::memory_order_relaxed));
        }
    }
    lastStart = now.QuadPart;
    return now.QuadPart;
}
void publishFrame(IDXGISwapChain* chain, LONGLONG at) {
    if (!at || !control || !enabled.load(std::memory_order_relaxed))
        return;
    AcquireSRWLockExclusive(&paceLock);
    struct Unlock {
        ~Unlock() {
            ReleaseSRWLockExclusive(&paceLock);
        }
    } unlock;
    if (!enabled.load(std::memory_order_relaxed) || primary.load() != chain || at <= lastSuccessful)
        return;
    LONGLONG interval = lastSuccessful ? at - lastSuccessful : 0;
    lastSuccessful = at;
    auto sequence = fg::read64(&control->frames) + 1;
    auto& entry = control->ring[sequence % fg::hookRingSize];
    InterlockedExchange64(&entry.sequence, 0);
    InterlockedExchange64(&entry.qpc, at);
    InterlockedExchange64(&entry.interval, interval);
    InterlockedExchange64(&entry.sequence, sequence);
    InterlockedExchange64(&control->lastPresentQpc, at);
    InterlockedExchange64(&control->frames, sequence);
}
HRESULT STDMETHODCALLTYPE present(IDXGISwapChain* chain, UINT interval, UINT flags) {
    if (inside)
        return originalPresent(chain, interval, flags);
    inside = true;
    auto at = pace(chain, flags);
    bool drawn = drawHud(chain, flags);
    HRESULT hr = originalPresent(chain, interval, flags);
    if (hr == S_OK) {
        publishFrame(chain, at);
        if (drawn)
            InterlockedExchange64(&control->lastHudQpc, at);
    }
    inside = false;
    return hr;
}
HRESULT STDMETHODCALLTYPE present1(IDXGISwapChain1* chain, UINT interval, UINT flags,
                                   const DXGI_PRESENT_PARAMETERS* params) {
    if (inside)
        return originalPresent1(chain, interval, flags, params);
    inside = true;
    auto at = pace(chain, flags);
    bool drawn = drawHud(chain, flags);
    HRESULT hr = originalPresent1(chain, interval, flags, params);
    if (hr == S_OK) {
        publishFrame(chain, at);
        if (drawn)
            InterlockedExchange64(&control->lastHudQpc, at);
    }
    inside = false;
    return hr;
}
DWORD WINAPI worker(void*) {
    auto name = fg::mappingName(GetCurrentProcessId());
    HANDLE map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
    if (!map)
        return 0;
    control = static_cast<fg::HookControl*>(
        MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(fg::HookControl)));
    if (!control || control->magic != fg::ipcMagic || control->version != fg::ipcVersion ||
        control->targetPid != GetCurrentProcessId() ||
        control->creationTime != fg::creationTime(GetCurrentProcess())) {
        if (control)
            UnmapViewOfFile(control);
        control = nullptr;
        CloseHandle(map);
        return 0;
    }
    QueryPerformanceFrequency(&frequency);
    if (fg::read32(&control->request) ||
        GetTickCount64() - ULONGLONG(fg::read64(&control->heartbeat)) > 3500) {
        InterlockedExchange(&control->status, 2);
        return 0;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"Framoryx.DXGI.Probe";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"", WS_POPUP, 0, 0, 8, 8, nullptr, nullptr,
                                wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* chain = nullptr;
    HRESULT hr =
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                      D3D11_SDK_VERSION, &desc, &chain, &device, nullptr, &context);
    if (FAILED(hr))
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                           D3D11_SDK_VERSION, &desc, &chain, &device, nullptr,
                                           &context);
    bool ok = false;
    if (SUCCEEDED(hr) && chain && MH_Initialize() == MH_OK) {
        auto v = *reinterpret_cast<void***>(chain);
        ok = MH_CreateHook(v[8], reinterpret_cast<void*>(present),
                           reinterpret_cast<void**>(&originalPresent)) == MH_OK;
        IDXGISwapChain1* chain1 = nullptr;
        if (SUCCEEDED(chain->QueryInterface(__uuidof(IDXGISwapChain1),
                                            reinterpret_cast<void**>(&chain1)))) {
            auto v1 = *reinterpret_cast<void***>(chain1);
            auto result = MH_CreateHook(v1[22], reinterpret_cast<void*>(present1),
                                        reinterpret_cast<void**>(&originalPresent1));
            ok = ok && result == MH_OK;
            chain1->Release();
        }
        if (ok && (fg::read32(&control->request) ||
                   GetTickCount64() - ULONGLONG(fg::read64(&control->heartbeat)) > 3500))
            ok = false;
        if (ok) {
            ID3D12Device* probeDevice = nullptr;
            ID3D12CommandQueue* probeQueue = nullptr;
            D3D12_COMMAND_QUEUE_DESC queueDesc{};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                            reinterpret_cast<void**>(&probeDevice)))) {
                if (SUCCEEDED(
                        probeDevice->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue),
                                                        reinterpret_cast<void**>(&probeQueue)))) {
                    auto methods = *reinterpret_cast<void***>(probeQueue);
                    MH_CreateHook(methods[10], reinterpret_cast<void*>(execute),
                                  reinterpret_cast<void**>(&originalExecute));
                    probeQueue->Release();
                }
                probeDevice->Release();
            }
        }
        if (ok) {
            enabled = true;
            ok = MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
        }
    }
    if (context)
        context->Release();
    if (device)
        device->Release();
    if (chain)
        chain->Release();
    DestroyWindow(window);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if (!ok) {
        enabled = false;
        MH_DisableHook(MH_ALL_HOOKS);
        InterlockedExchange(&control->status, -1);
        return 0;
    }
    InterlockedExchange(&control->status, 1);
    for (;;) {
        Sleep(200);
        // Heartbeat loss disables pacing and restores hook entry points. No automatic retry.
        if (fg::read32(&control->request) ||
            GetTickCount64() - ULONGLONG(fg::read64(&control->heartbeat)) > 3500)
            break;
    }
    enabled = false;
    MH_DisableHook(MH_ALL_HOOKS);
    InterlockedExchange(&control->status, 2);
    // Intentionally retain the DLL, mapping and trampolines until process exit. A render thread may
    // still be returning from a callback. Unloading code here would create a use-after-free race.
    return 0;
}
} // namespace
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
