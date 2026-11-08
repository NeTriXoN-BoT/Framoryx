#include "platform.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <shellapi.h>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
LRESULT CALLBACK sceneProc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (m == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    if (m == WM_KEYDOWN && a == VK_ESCAPE) {
        DestroyWindow(w);
        return 0;
    }
    return DefWindowProcW(w, m, a, b);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &argc);
    double duration = 0;
    std::wstring report;
    bool verifyPipeline = false;
    for (int i = 1; i < argc; ++i) {
        if (std::wstring(args[i]) == L"--seconds" && i + 1 < argc)
            duration = _wtof(args[++i]);
        else if (std::wstring(args[i]) == L"--report" && i + 1 < argc)
            report = args[++i];
        else if (std::wstring(args[i]) == L"--verify-pipeline")
            verifyPipeline = true;
    }
    LocalFree(args);
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = sceneProc;
    wc.lpszClassName = L"Framoryx.TestScene";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    HWND window =
        CreateWindowW(wc.lpszClassName, L"Framoryx test scene — DirectX 11 — Esc to close",
                      WS_OVERLAPPEDWINDOW, 90, 90, 800, 480, nullptr, nullptr, instance, nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    IDXGISwapChain* chain{};
    auto result =
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                      D3D11_SDK_VERSION, &desc, &chain, &device, nullptr, &context);
    if (FAILED(result))
        result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr,
                                               0, D3D11_SDK_VERSION, &desc, &chain, &device,
                                               nullptr, &context);
    if (FAILED(result)) {
        MessageBoxW(window, L"DirectX 11 initialization failed.", L"Framoryx test", MB_OK);
        return 1;
    }
    ID3D11ComputeShader* expectedCompute = nullptr;
    if (verifyPipeline) {
        const char shader[] = "[numthreads(1,1,1)] void main(uint3 id : SV_DispatchThreadID) {}";
        ID3DBlob* bytecode = nullptr;
        if (FAILED(D3DCompile(shader, sizeof(shader) - 1, nullptr, nullptr, nullptr, "main",
                              "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &bytecode, nullptr)))
            return 4;
        auto compiled = device->CreateComputeShader(
            bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &expectedCompute);
        bytecode->Release();
        if (FAILED(compiled))
            return 5;
        context->CSSetShader(expectedCompute, nullptr, 0);
    }
    ShowWindow(window, show);
    LARGE_INTEGER start{}, now{}, frequency{}, previous{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    previous = start;
    std::ofstream log;
    if (!report.empty()) {
        log.open(std::filesystem::path(report));
        log << "Seconds,FrameTimeMs\n" << std::fixed << std::setprecision(6);
    }
    MSG message{};
    UINT width = 0, height = 0;
    ID3D11RenderTargetView* view{};
    fg::Handle frameTimer(CreateWaitableTimerExW(nullptr, nullptr, 2, TIMER_ALL_ACCESS));
    while (message.message != WM_QUIT) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (message.message == WM_QUIT)
            break;
        RECT rect{};
        GetClientRect(window, &rect);
        UINT w = std::max(1L, rect.right), h = std::max(1L, rect.bottom);
        if (w != width || h != height) {
            context->OMSetRenderTargets(0, nullptr, nullptr);
            if (view) {
                view->Release();
                view = nullptr;
            }
            if (SUCCEEDED(chain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0))) {
                width = w;
                height = h;
            }
            ID3D11Texture2D* buffer{};
            if (SUCCEEDED(chain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                           reinterpret_cast<void**>(&buffer)))) {
                device->CreateRenderTargetView(buffer, nullptr, &view);
                buffer->Release();
            }
        }
        QueryPerformanceCounter(&now);
        double seconds = double(now.QuadPart - start.QuadPart) / frequency.QuadPart;
        if (duration > 0 && seconds > duration) {
            DestroyWindow(window);
            break;
        }
        float color[]{float(0.04 + 0.03 * std::sin(seconds)),
                      float(0.17 + 0.08 * std::sin(seconds * 0.7)),
                      float(0.19 + 0.08 * std::cos(seconds * 0.9)), 1};
        if (view) {
            context->OMSetRenderTargets(1, &view, nullptr);
            context->ClearRenderTargetView(view, color);
        }
        if (verifyPipeline)
            chain->Present(0, DXGI_PRESENT_TEST);
        chain->Present(0, 0);
        QueryPerformanceCounter(&now);
        if (verifyPipeline) {
            ID3D11ComputeShader* actual = nullptr;
            context->CSGetShader(&actual, nullptr, nullptr);
            bool preserved = actual == expectedCompute;
            if (actual)
                actual->Release();
            if (!preserved)
                return 6;
        }
        if (log)
            log << double(now.QuadPart - start.QuadPart) / frequency.QuadPart << ','
                << 1000.0 * (now.QuadPart - previous.QuadPart) / frequency.QuadPart << '\n';
        previous = now;
        LARGE_INTEGER due{};
        due.QuadPart = -10000;
        if (frameTimer && SetWaitableTimer(frameTimer.h, &due, 0, nullptr, nullptr, FALSE))
            WaitForSingleObject(frameTimer.h, 100);
        else
            Sleep(1);
    }
    if (view)
        view->Release();
    if (expectedCompute)
        expectedCompute->Release();
    chain->Release();
    context->Release();
    device->Release();
    return 0;
}
