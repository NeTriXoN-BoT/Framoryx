#include "capture.hpp"
#include <psapi.h>
#include <iostream>

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    int duration = argc > 1 ? std::clamp(std::atoi(argv[1]), 30, 600) : 120;
    auto blocker = fg::antiCheatRunning();
    if (!blocker.empty()) {
        std::cout << "SKIP: optional hook soak blocked by " << fg::utf8(blocker) << '\n';
        return 2;
    }
    fg::Capture capture;
    fg::HookClient hook;
    fg::Borderless borderless;
    fg::Rules rules;
    std::wstring executable = fg::executableDir() + L"\\tools\\FramoryxTest.exe";
    std::wstring command =
        L"\"" + executable + L"\" --verify-pipeline --seconds " + std::to_wstring(duration + 20);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNOACTIVATE;
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        nullptr, &startup, &info))
        return 1;
    fg::Handle process(info.hProcess), thread(info.hThread);
    auto wait = [&](int milliseconds) {
        auto end = GetTickCount64() + milliseconds;
        while (GetTickCount64() < end) {
            hook.tick();
            Sleep(50);
        }
    };
    wait(700);
    HWND window = nullptr;
    struct Find {
        DWORD pid;
        HWND* result;
    } find{info.dwProcessId, &window};
    EnumWindows(
        [](HWND current, LPARAM data) -> BOOL {
            auto find = reinterpret_cast<Find*>(data);
            DWORD pid = 0;
            GetWindowThreadProcessId(current, &pid);
            if (pid == find->pid && IsWindowVisible(current)) {
                *find->result = current;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&find));
    int failures = 0;
    auto error = capture.start(rules);
    if (!window || !error.empty()) {
        std::cout << "FAIL: capture/window setup\n";
        ++failures;
    }
    error = hook.attach(info.dwProcessId, "FramoryxTest.exe", rules, 30);
    for (int i = 0; i < 80 && error.empty() && !hook.active(); ++i)
        wait(100);
    if (!error.empty() || !hook.active()) {
        std::cout << "FAIL: hook setup\n";
        ++failures;
    }

    fg::Store history;
    LONGLONG cursor = 0;
    auto start = GetTickCount64();
    int lastSegment = -1, lastWindowStep = -1, lastReport = -1;
    bool full = false;
    int observed = 0, badPacing = 0, resizeChanges = 0;
    SIZE_T baselinePrivate = 0, finalPrivate = 0;
    DWORD baselineHandles = 0, finalHandles = 0;
    const int caps[]{30, 60, 120, 0};
    while (!failures && GetTickCount64() - start < ULONGLONG(duration) * 1000) {
        if (WaitForSingleObject(process.h, 0) != WAIT_TIMEOUT) {
            std::cout << "FAIL: graphics scene exited during soak\n";
            ++failures;
            break;
        }
        double elapsed = (GetTickCount64() - start) / 1000.0;
        int segment = int(elapsed / 12);
        int cap = caps[segment % 4];
        if (segment != lastSegment) {
            hook.cap(cap);
            lastSegment = segment;
        }
        int step = int(elapsed / 18);
        if (step != lastWindowStep && window) {
            if (full) {
                if (!borderless.restore(window))
                    ++failures;
                full = false;
            } else {
                MONITORINFO monitor{};
                monitor.cbSize = sizeof(monitor);
                GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
                if (!borderless.apply(window, monitor.rcMonitor).empty())
                    ++failures;
                full = true;
            }
            lastWindowStep = step;
            ++resizeChanges;
        }
        wait(250);
        for (auto& frame : hook.drainFrames(cursor))
            history.add(frame, fg::clockSeconds());
        auto stats = history.snapshot(info.dwProcessId, fg::clockSeconds());
        if (std::fmod(elapsed, 12.0) > 3 && elapsed > 4 && std::fmod(elapsed, 18.0) > 2) {
            ++observed;
            if (!stats.live || (cap && (stats.fps < cap * 0.70 || stats.fps > cap * 1.06)))
                ++badPacing;
        }
        PROCESS_MEMORY_COUNTERS_EX memory{};
        memory.cb = sizeof(memory);
        if (GetProcessMemoryInfo(process.h, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                                 sizeof(memory))) {
            finalPrivate = memory.PrivateUsage;
            GetProcessHandleCount(process.h, &finalHandles);
            if (elapsed >= 35 && !baselinePrivate) {
                baselinePrivate = finalPrivate;
                baselineHandles = finalHandles;
            }
        }
        int report = int(elapsed / 15);
        if (report != lastReport) {
            std::cout << "Soak " << int(elapsed) << "s | cap " << cap << " | " << stats.fps
                      << " FPS | scene private " << finalPrivate / (1024 * 1024)
                      << " MiB | handles " << finalHandles << '\n';
            lastReport = report;
        }
    }
    if (!observed || badPacing > std::max(2, observed / 5)) {
        std::cout << "FAIL: unstable telemetry/pacing during soak\n";
        ++failures;
    }
    if (baselinePrivate && finalPrivate > baselinePrivate + 128ULL * 1024 * 1024) {
        std::cout << "FAIL: excessive private-memory growth after warm-up\n";
        ++failures;
    }
    if (baselineHandles && finalHandles > baselineHandles + 20) {
        std::cout << "FAIL: handle growth after warm-up\n";
        ++failures;
    }
    std::cout << "Soak observations " << observed << ", pacing outliers " << badPacing
              << ", window transitions " << resizeChanges << '\n';
    std::cout << "Post-warm-up private-byte delta "
              << (baselinePrivate ? static_cast<long long>(finalPrivate) -
                                        static_cast<long long>(baselinePrivate)
                                  : 0)
              << ", handle delta " << static_cast<long long>(finalHandles) - baselineHandles
              << '\n';
    hook.disable();
    wait(400);
    borderless.restoreAll();
    capture.stop();
    if (window)
        PostMessageW(window, WM_CLOSE, 0, 0);
    if (WaitForSingleObject(process.h, 3000) == WAIT_TIMEOUT)
        TerminateProcess(process.h, 0);
    std::cout << (failures ? "FAIL" : "PASS") << ": " << duration
              << " second DX11 telemetry/limiter/resize soak\n";
    return failures ? 1 : 0;
}
