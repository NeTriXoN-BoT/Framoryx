#include "platform.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <shellapi.h>
#include <array>
#include <cmath>

namespace {
template <class T> struct Ref {
    T* p = nullptr;
    ~Ref() {
        if (p)
            p->Release();
    }
    T* operator->() const {
        return p;
    }
    void reset() {
        if (p)
            p->Release();
        p = nullptr;
    }
};
LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    if (message == WM_KEYDOWN && wparam == VK_ESCAPE) {
        DestroyWindow(window);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    double duration = 0;
    bool multiQueue = false;
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::wstring(argv[i]) == L"--seconds")
            duration = _wtof(argv[++i]);
    }
    for (int i = 1; i < argc; ++i)
        if (std::wstring(argv[i]) == L"--multi-queue")
            multiQueue = true;
    LocalFree(argv);

    Ref<IDXGIFactory4> factory;
    if (FAILED(
            CreateDXGIFactory2(0, __uuidof(IDXGIFactory4), reinterpret_cast<void**>(&factory.p))))
        return 1;
    Ref<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                 reinterpret_cast<void**>(&device.p)))) {
        Ref<IDXGIAdapter> warp;
        if (FAILED(factory->EnumWarpAdapter(__uuidof(IDXGIAdapter),
                                            reinterpret_cast<void**>(&warp.p))))
            return 2;
        if (FAILED(D3D12CreateDevice(warp.p, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                     reinterpret_cast<void**>(&device.p))))
            return 3;
    }
    Ref<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue),
                                          reinterpret_cast<void**>(&queue.p))))
        return 4;

    WNDCLASSW klass{};
    klass.hInstance = instance;
    klass.lpfnWndProc = windowProc;
    klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    klass.lpszClassName = L"Framoryx.TestScene12";
    RegisterClassW(&klass);
    HWND window =
        CreateWindowW(klass.lpszClassName, L"Framoryx test scene — DirectX 12 — Esc to close",
                      WS_OVERLAPPEDWINDOW, 120, 120, 800, 480, nullptr, nullptr, instance, nullptr);
    if (!window)
        return 5;
    BOOL tearing = FALSE;
    Ref<IDXGIFactory5> factory5;
    if (SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory5),
                                          reinterpret_cast<void**>(&factory5.p))))
        factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing,
                                      sizeof(tearing));
    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    swapDesc.Width = 800;
    swapDesc.Height = 480;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.Flags = tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    Ref<IDXGISwapChain1> swap1;
    if (FAILED(factory->CreateSwapChainForHwnd(queue.p, window, &swapDesc, nullptr, nullptr,
                                               &swap1.p)))
        return 6;
    Ref<IDXGISwapChain3> swap;
    if (FAILED(swap1->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swap.p))))
        return 7;
    factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);

    Ref<ID3D12DescriptorHeap> heap;
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = 2;
    if (FAILED(device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap),
                                            reinterpret_cast<void**>(&heap.p))))
        return 8;
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    std::array<Ref<ID3D12Resource>, 2> buffers;
    auto createBuffers = [&] {
        auto handle = heap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < buffers.size(); ++i) {
            if (FAILED(swap->GetBuffer(i, __uuidof(ID3D12Resource),
                                       reinterpret_cast<void**>(&buffers[i].p))))
                return false;
            device->CreateRenderTargetView(buffers[i].p, nullptr, handle);
            handle.ptr += descriptorSize;
        }
        return true;
    };
    if (!createBuffers())
        return 9;
    Ref<ID3D12CommandAllocator> allocator;
    Ref<ID3D12GraphicsCommandList> commands;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              __uuidof(ID3D12CommandAllocator),
                                              reinterpret_cast<void**>(&allocator.p))))
        return 10;
    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.p, nullptr,
                                         __uuidof(ID3D12GraphicsCommandList),
                                         reinterpret_cast<void**>(&commands.p))))
        return 11;
    commands->Close();
    Ref<ID3D12CommandQueue> extraQueue;
    Ref<ID3D12CommandAllocator> extraAllocator;
    Ref<ID3D12GraphicsCommandList> extraCommands;
    Ref<ID3D12Fence> extraFence;
    if (multiQueue) {
        if (FAILED(device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue),
                                              reinterpret_cast<void**>(&extraQueue.p))) ||
            FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                  __uuidof(ID3D12CommandAllocator),
                                                  reinterpret_cast<void**>(&extraAllocator.p))) ||
            FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, extraAllocator.p,
                                             nullptr, __uuidof(ID3D12GraphicsCommandList),
                                             reinterpret_cast<void**>(&extraCommands.p))) ||
            FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                       reinterpret_cast<void**>(&extraFence.p))))
            return 22;
        extraCommands->Close();
    }
    Ref<ID3D12Fence> fence;
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                   reinterpret_cast<void**>(&fence.p))))
        return 12;
    fg::Handle completed(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    if (!completed)
        return 13;
    UINT64 fenceValue = 0;
    auto waitForGpu = [&] {
        if (FAILED(queue->Signal(fence.p, ++fenceValue)))
            return false;
        if (fence->GetCompletedValue() >= fenceValue)
            return true;
        if (FAILED(fence->SetEventOnCompletion(fenceValue, completed.h)))
            return false;
        return WaitForSingleObject(completed.h, 2000) == WAIT_OBJECT_0;
    };
    fg::Handle timer(CreateWaitableTimerExW(nullptr, nullptr, 2, TIMER_ALL_ACCESS));
    ShowWindow(window, show);
    LARGE_INTEGER start{}, now{}, frequency{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    MSG message{};
    UINT width = swapDesc.Width, height = swapDesc.Height;
    while (message.message != WM_QUIT) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (message.message == WM_QUIT)
            break;
        QueryPerformanceCounter(&now);
        double seconds = double(now.QuadPart - start.QuadPart) / frequency.QuadPart;
        if (duration > 0 && seconds >= duration) {
            DestroyWindow(window);
            break;
        }
        if (IsIconic(window)) {
            Sleep(20);
            continue;
        }
        RECT client{};
        GetClientRect(window, &client);
        UINT newWidth = UINT(std::max(1L, client.right)),
             newHeight = UINT(std::max(1L, client.bottom));
        if (newWidth != width || newHeight != height) {
            if (!waitForGpu())
                return 14;
            allocator->Reset();
            commands->Reset(allocator.p, nullptr);
            commands->Close();
            for (auto& buffer : buffers)
                buffer.reset();
            if (FAILED(swap->ResizeBuffers(2, newWidth, newHeight, DXGI_FORMAT_UNKNOWN,
                                           swapDesc.Flags)))
                return 15;
            if (!createBuffers())
                return 16;
            width = newWidth;
            height = newHeight;
        }
        if (FAILED(allocator->Reset()) || FAILED(commands->Reset(allocator.p, nullptr)))
            return 17;
        UINT index = swap->GetCurrentBackBufferIndex();
        D3D12_RESOURCE_BARRIER transition{};
        transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transition.Transition.pResource = buffers[index].p;
        transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        transition.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        transition.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commands->ResourceBarrier(1, &transition);
        auto target = heap->GetCPUDescriptorHandleForHeapStart();
        target.ptr += index * descriptorSize;
        float color[]{0.04f, float(0.14 + 0.04 * std::sin(seconds)),
                      float(0.20 + 0.07 * std::cos(seconds)), 1};
        commands->ClearRenderTargetView(target, color, 0, nullptr);
        std::swap(transition.Transition.StateBefore, transition.Transition.StateAfter);
        commands->ResourceBarrier(1, &transition);
        if (FAILED(commands->Close()))
            return 18;
        ID3D12CommandList* submit[]{commands.p};
        if (multiQueue) {
            ID3D12CommandList* extra[]{extraCommands.p};
            extraQueue->ExecuteCommandLists(1, extra);
        }
        queue->ExecuteCommandLists(1, submit);
        if (FAILED(swap->Present(0, tearing ? DXGI_PRESENT_ALLOW_TEARING : 0)))
            return 19;
        if (!waitForGpu())
            return 20;
        LARGE_INTEGER due{};
        due.QuadPart = -10000;
        if (timer && SetWaitableTimer(timer.h, &due, 0, nullptr, nullptr, FALSE))
            WaitForSingleObject(timer.h, 100);
        else
            Sleep(1);
    }
    waitForGpu();
    if (multiQueue && SUCCEEDED(extraQueue->Signal(extraFence.p, 1)) &&
        extraFence->GetCompletedValue() < 1) {
        extraFence->SetEventOnCompletion(1, completed.h);
        WaitForSingleObject(completed.h, 2000);
    }
    return 0;
}
