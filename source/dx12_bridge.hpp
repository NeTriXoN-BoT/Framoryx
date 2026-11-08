#pragma once
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <array>
#include <atomic>

// A small 11-on-12 renderer bridge, used only after explicit hook opt-in.
// Use the game's observed queue to preserve submission order. Multiple direct
// queues on the same device are ambiguous: keep the limiter and desktop HUD.
class Dx12Bridge {
    struct Queue {
        ID3D12CommandQueue* queue = nullptr;
        ID3D12Device* device = nullptr;
        bool ambiguous = false;
    };
    std::array<Queue, 8> queues{};
    std::array<std::atomic<ID3D12CommandQueue*>, 8> seen{};
    SRWLOCK queueLock = SRWLOCK_INIT;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    ID3D11On12Device* interop = nullptr;
    IDXGISwapChain3* swap = nullptr;
    ID3D11Texture2D* wrapped = nullptr;
    ID3D12Resource* nativeBuffer = nullptr;

  public:
    void observe(ID3D12CommandQueue* submitted) {
        for (auto& known : seen)
            if (known.load(std::memory_order_relaxed) == submitted)
                return;
        if (submitted->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
            return;
        ID3D12Device* owner = nullptr;
        if (FAILED(submitted->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&owner))))
            return;
        AcquireSRWLockExclusive(&queueLock);
        bool already = false;
        size_t empty = queues.size();
        for (size_t i = 0; i < queues.size(); ++i) {
            if (queues[i].queue == submitted)
                already = true;
            if (!queues[i].queue && empty == queues.size())
                empty = i;
        }
        if (!already) {
            bool ambiguous = false;
            for (auto& existing : queues)
                if (existing.device == owner && existing.queue != submitted) {
                    existing.ambiguous = true;
                    ambiguous = true;
                }
            if (empty < queues.size()) {
                submitted->AddRef();
                owner->AddRef();
                queues[empty] = {submitted, owner, ambiguous};
                seen[empty].store(submitted, std::memory_order_release);
            }
        }
        ReleaseSRWLockExclusive(&queueLock);
        owner->Release();
    }
    bool validQueue() {
        bool valid = false;
        AcquireSRWLockShared(&queueLock);
        for (auto& found : queues)
            if (found.queue == queue && !found.ambiguous)
                valid = true;
        ReleaseSRWLockShared(&queueLock);
        return valid;
    }
    bool initialize(IDXGISwapChain* chain, ID3D11Device** output, ID3D11DeviceContext** context) {
        reset();
        if (FAILED(chain->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&device))))
            return false;
        AcquireSRWLockShared(&queueLock);
        for (auto& candidate : queues)
            if (candidate.device == device && !candidate.ambiguous) {
                queue = candidate.queue;
                queue->AddRef();
                break;
            }
        ReleaseSRWLockShared(&queueLock);
        if (!queue || FAILED(chain->QueryInterface(__uuidof(IDXGISwapChain3),
                                                   reinterpret_cast<void**>(&swap)))) {
            reset();
            return false;
        }
        IUnknown* submissions[]{queue};
        auto result = D3D11On12CreateDevice(device, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                            submissions, 1, 0, output, context, nullptr);
        if (FAILED(result) ||
            FAILED((*output)->QueryInterface(__uuidof(ID3D11On12Device),
                                             reinterpret_cast<void**>(&interop)))) {
            if (*context) {
                (*context)->Release();
                *context = nullptr;
            }
            if (*output) {
                (*output)->Release();
                *output = nullptr;
            }
            reset();
            return false;
        }
        return true;
    }
    ID3D11Texture2D* acquire() {
        if (!interop || !swap || !validQueue())
            return nullptr;
        auto index = swap->GetCurrentBackBufferIndex();
        if (FAILED(swap->GetBuffer(index, __uuidof(ID3D12Resource),
                                   reinterpret_cast<void**>(&nativeBuffer))))
            return nullptr;
        D3D11_RESOURCE_FLAGS flags{};
        flags.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(interop->CreateWrappedResource(
                nativeBuffer, &flags, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT,
                __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&wrapped)))) {
            nativeBuffer->Release();
            nativeBuffer = nullptr;
            return nullptr;
        }
        ID3D11Resource* resources[]{wrapped};
        interop->AcquireWrappedResources(resources, 1);
        wrapped->AddRef();
        return wrapped;
    }
    void finish(ID3D11DeviceContext* context) {
        if (wrapped) {
            ID3D11Resource* resources[]{wrapped};
            interop->ReleaseWrappedResources(resources, 1);
            context->ClearState();
            wrapped->Release();
            wrapped = nullptr;
            // Flush after releasing all wrapped views/references: no back-buffer
            // references are retained between presents, including during resize.
            context->Flush();
        }
        if (nativeBuffer) {
            nativeBuffer->Release();
            nativeBuffer = nullptr;
        }
    }
    void reset() {
        if (interop) {
            interop->Release();
            interop = nullptr;
        }
        if (swap) {
            swap->Release();
            swap = nullptr;
        }
        if (queue) {
            queue->Release();
            queue = nullptr;
        }
        if (device) {
            device->Release();
            device = nullptr;
        }
    }
};
