#include "capture.hpp"
#include <psapi.h>
#include <iostream>

// This test exercises only ETW and ordinary window controls. It never attaches
// a hook or restores/retries any component blocked by Windows security.
int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    int duration = argc > 1 ? std::clamp(std::atoi(argv[1]), 30, 600) : 120;
    std::wstring executable = fg::executableDir() + L"\\tools\\FramoryxTest.exe";
    std::wstring command =
        L"\"" + executable + L"\" --verify-pipeline --seconds " + std::to_wstring(duration + 15);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNOACTIVATE;
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        nullptr, &startup, &info))
        return 1;
    fg::Handle process(info.hProcess), thread(info.hThread);
    fg::Capture capture;
    fg::Borderless borderless;
    fg::Rules rules;
    HWND window = nullptr;
    struct Find {
        DWORD pid;
        HWND* result;
    };
    Find find{info.dwProcessId, &window};
    for (int attempt = 0; attempt < 30 && !window; ++attempt) {
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
        if (!window)
            Sleep(100);
    }
    int failures = 0;
    auto error = capture.start(rules);
    if (!window || !error.empty()) {
        std::cout << "FAIL: passive scene/capture setup\n";
        ++failures;
    }
    auto start = GetTickCount64();
    int lastStep = -1, lastReport = -1, observed = 0, missing = 0, transitions = 0;
    bool full = false;
    SIZE_T baseline = 0, finalBytes = 0;
    DWORD baselineHandles = 0, finalHandles = 0;
    while (!failures && GetTickCount64() - start < ULONGLONG(duration) * 1000) {
        double elapsed = (GetTickCount64() - start) / 1000.;
        if (WaitForSingleObject(process.h, 0) != WAIT_TIMEOUT) {
            std::cout << "FAIL: passive scene exited\n";
            ++failures;
            break;
        }
        int step = int(elapsed / 18);
        if (step != lastStep && window) {
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
            lastStep = step;
            ++transitions;
        }
        Sleep(250);
        auto stats = capture.snapshot(info.dwProcessId);
        if (elapsed > 6 && std::fmod(elapsed, 18.) > 2) {
            ++observed;
            if (!stats.live || stats.fps < 10)
                ++missing;
        }
        PROCESS_MEMORY_COUNTERS_EX memory{};
        memory.cb = sizeof(memory);
        if (GetProcessMemoryInfo(GetCurrentProcess(),
                                 reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                                 sizeof(memory))) {
            finalBytes = memory.PrivateUsage;
            GetProcessHandleCount(GetCurrentProcess(), &finalHandles);
            if (elapsed >= 35 && !baseline) {
                baseline = finalBytes;
                baselineHandles = finalHandles;
            }
        }
        int report = int(elapsed / 15);
        if (report != lastReport) {
            std::cout << "Passive soak " << int(elapsed) << "s | " << stats.fps
                      << " FPS | collector private " << finalBytes / (1024 * 1024)
                      << " MiB | handles " << finalHandles << '\n';
            lastReport = report;
        }
    }
    if (!observed || missing > std::max(2, observed / 10))
        ++failures;
    if (baseline && finalBytes > baseline + 64ULL * 1024 * 1024)
        ++failures;
    if (baselineHandles && finalHandles > baselineHandles + 12)
        ++failures;
    std::cout << "Passive observations " << observed << ", missing telemetry " << missing
              << ", window transitions " << transitions << '\n';
    std::cout << "Post-warm-up collector private-byte delta "
              << (baseline ? static_cast<long long>(finalBytes) - static_cast<long long>(baseline)
                           : 0)
              << ", handle delta " << static_cast<long long>(finalHandles) - baselineHandles
              << '\n';
    borderless.restoreAll();
    capture.stop();
    if (window)
        PostMessageW(window, WM_CLOSE, 0, 0);
    if (WaitForSingleObject(process.h, 3000) == WAIT_TIMEOUT)
        TerminateProcess(process.h, 0);
    std::cout << (failures ? "FAIL" : "PASS") << ": " << duration
              << " second non-injecting ETW/borderless soak\n";
    return failures ? 1 : 0;
}
