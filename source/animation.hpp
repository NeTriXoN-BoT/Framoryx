#pragma once
#include "platform.hpp"
#include <atomic>
#include <thread>

namespace fg {
// A process-local high-resolution clock. No timeBeginPeriod/global timer changes.
// Only one paint request can be outstanding, so slow frames cannot build a queue.
class AnimationPump {
    Handle stopEvent, wakeEvent, timer;
    std::thread worker;
    std::atomic<bool> active{false}, pending{false};
    std::atomic<int> refresh{60};
    HWND window = nullptr;

  public:
    static constexpr UINT message = WM_APP + 23;
    ~AnimationPump() {
        stop();
    }
    bool start(HWND target) {
        window = target;
        stopEvent.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        wakeEvent.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
        timer.reset(CreateWaitableTimerExW(nullptr, nullptr, 2, TIMER_ALL_ACCESS));
        if (!timer)
            timer.reset(CreateWaitableTimerW(nullptr, FALSE, nullptr));
        if (!stopEvent || !wakeEvent || !timer)
            return false;
        try {
            worker = std::thread([this] {
                LARGE_INTEGER frequency{};
                QueryPerformanceFrequency(&frequency);
                LONGLONG deadline = 0;
                HANDLE events[]{stopEvent.h, wakeEvent.h, timer.h};
                for (;;) {
                    if (!active.load(std::memory_order_acquire)) {
                        auto result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
                        if (result == WAIT_OBJECT_0)
                            break;
                        deadline = 0;
                        continue;
                    }
                    LARGE_INTEGER now{};
                    QueryPerformanceCounter(&now);
                    LONGLONG step = frequency.QuadPart / refresh.load(std::memory_order_relaxed);
                    if (!deadline || deadline < now.QuadPart - step)
                        deadline = now.QuadPart + step;
                    LARGE_INTEGER due{};
                    due.QuadPart = -std::max<LONGLONG>(1, (deadline - now.QuadPart) * 10000000 /
                                                              frequency.QuadPart);
                    if (!SetWaitableTimer(timer.h, &due, 0, nullptr, nullptr, FALSE))
                        break;
                    auto result = WaitForMultipleObjects(3, events, FALSE, INFINITE);
                    if (result == WAIT_OBJECT_0)
                        break;
                    if (result == WAIT_OBJECT_0 + 1) {
                        deadline = 0;
                        continue;
                    }
                    if (result != WAIT_OBJECT_0 + 2)
                        break;
                    deadline += step;
                    if (active.load(std::memory_order_acquire) &&
                        !pending.exchange(true, std::memory_order_acq_rel))
                        if (!PostMessageW(window, message, 0, 0))
                            pending = false;
                }
            });
        } catch (...) {
            return false;
        }
        return true;
    }
    void configure(bool enabled, int rate) {
        if (!enabled)
            pending.store(false, std::memory_order_release);
        rate = std::clamp(rate, 30, 144);
        bool changed = active.exchange(enabled, std::memory_order_acq_rel) != enabled;
        changed |= refresh.exchange(rate, std::memory_order_relaxed) != rate;
        if (changed && wakeEvent)
            SetEvent(wakeEvent.h);
    }
    void painted() {
        pending.store(false, std::memory_order_release);
    }
    void stop() {
        if (stopEvent)
            SetEvent(stopEvent.h);
        if (worker.joinable())
            worker.join();
        active = false;
        pending = false;
    }
};
} // namespace fg
