#include "capture.hpp"
#include "audio.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <functional>
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--orphan-etw") {
        std::vector<BYTE> buffer(sizeof(EVENT_TRACE_PROPERTIES) + 1024);
        auto p = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.data());
        p->Wnode.BufferSize = ULONG(buffer.size());
        p->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        p->BufferSize = 64;
        p->MinimumBuffers = 2;
        p->MaximumBuffers = 2;
        p->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
        p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        TRACEHANDLE trace = 0;
        auto name = L"Framoryx_" + std::to_wstring(GetCurrentProcessId()) + L"_OrphanTest";
        return int(StartTraceW(&trace, name.c_str(), p));
    }
    std::cout << std::unitbuf;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    fg::Capture capture;
    fg::HookClient hook;
    fg::Borderless borderless;
    fg::Rules rules;
    auto folder = fg::executableDir();
    std::wstring scene = folder + L"\\tools\\FramoryxTest.exe",
                 command = L"\"" + scene + L"\" --seconds 55 --verify-pipeline";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNOACTIVATE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(scene.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                        &si, &pi)) {
        std::cerr << "FAIL: test scene launch\n";
        return 1;
    }
    fg::Handle process(pi.hProcess), thread(pi.hThread);
    auto wait = [&](int milliseconds) {
        auto end = GetTickCount64() + milliseconds;
        do {
            hook.tick();
            Sleep(50);
        } while (GetTickCount64() < end);
    };
    int failed = 0, passed = 0;
    auto check = [&](bool result, const char* description) {
        std::cout << (result ? "PASS: " : "FAIL: ") << description << '\n';
        result ? ++passed : ++failed;
    };
    auto styleChecks = [&](const char* description) {
        fg::HudOptions options{fg::HudAll, 200, 45, 3};
        hook.hudOptions(options);
        hook.systemMetrics(25, 24);
        wait(700);
        check(hook.hudFresh(), description);
        options.fields = 0;
        hook.hudOptions(options);
        wait(1300);
        check(!hook.hudFresh(), "hiding every readout stops injected HUD drawing");
        options = {fg::HudFps | fg::HudTime, 75, 100, 2, 1};
        hook.hudOptions(options);
        wait(500);
        check(hook.hudFresh(),
              "HUD rebuilds sharp fonts and resumes after live style/size changes");
        options = {fg::HudAll, 100, 90, 0, 2};
        hook.hudOptions(options);
        wait(600);
        check(hook.hudFresh(),
              "telemetry-bar layout renders all selected fields without losing the game");
        hook.hudOptions({});
        wait(300);
    };
    {
        wchar_t selfPath[32768]{};
        GetModuleFileNameW(nullptr, selfPath, 32768);
        std::wstring childCommand = L"\"" + std::wstring(selfPath) + L"\" --orphan-etw";
        PROCESS_INFORMATION child{};
        STARTUPINFOW hidden{};
        hidden.cb = sizeof(hidden);
        hidden.dwFlags = STARTF_USESHOWWINDOW;
        hidden.wShowWindow = SW_HIDE;
        if (CreateProcessW(selfPath, childCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                           nullptr, nullptr, &hidden, &child)) {
            fg::Handle childProcess(child.hProcess), childThread(child.hThread);
            WaitForSingleObject(childProcess.h, 3000);
            DWORD code = 1;
            GetExitCodeProcess(childProcess.h, &code);
            auto name = L"Framoryx_" + std::to_wstring(child.dwProcessId) + L"_OrphanTest";
            std::vector<BYTE> buffer(sizeof(EVENT_TRACE_PROPERTIES) + 1024);
            auto p = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.data());
            p->Wnode.BufferSize = ULONG(buffer.size());
            p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
            auto before = ControlTraceW(0, name.c_str(), p, EVENT_TRACE_CONTROL_QUERY);
            check(code == 0 && before == ERROR_SUCCESS,
                  "crash simulation leaves an identifiable abandoned ETW session");
            std::vector<BYTE> liveBuffer(sizeof(EVENT_TRACE_PROPERTIES) + 1024);
            auto liveProperties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(liveBuffer.data());
            liveProperties->Wnode.BufferSize = ULONG(liveBuffer.size());
            liveProperties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
            liveProperties->BufferSize = 64;
            liveProperties->MinimumBuffers = 2;
            liveProperties->MaximumBuffers = 2;
            liveProperties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
            liveProperties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
            auto liveName = L"Framoryx_" + std::to_wstring(GetCurrentProcessId()) + L"_LiveGuard";
            TRACEHANDLE liveTrace = 0;
            auto liveStarted = StartTraceW(&liveTrace, liveName.c_str(), liveProperties);
            auto stopped = fg::cleanupAbandonedCaptures();
            p->Wnode.BufferSize = ULONG(buffer.size());
            auto after = ControlTraceW(0, name.c_str(), p, EVENT_TRACE_CONTROL_QUERY);
            check(stopped >= 1 && after != ERROR_SUCCESS,
                  "abandoned Framoryx ETW session is cleaned up after its owner exits");
            liveProperties->Wnode.BufferSize = ULONG(liveBuffer.size());
            check(liveStarted == ERROR_SUCCESS &&
                      ControlTraceW(0, liveName.c_str(), liveProperties,
                                    EVENT_TRACE_CONTROL_QUERY) == ERROR_SUCCESS,
                  "cleanup preserves a live controller’s ETW session");
            if (liveStarted == ERROR_SUCCESS)
                ControlTraceW(0, liveName.c_str(), liveProperties, EVENT_TRACE_CONTROL_STOP);
            if (after == ERROR_SUCCESS)
                ControlTraceW(0, name.c_str(), p, EVENT_TRACE_CONTROL_STOP);
        } else
            check(false, "abandoned-session test helper starts");
    }
    wait(1000);
    HWND window = nullptr;
    struct Ctx {
        DWORD pid;
        HWND* result;
    };
    Ctx ctx{pi.dwProcessId, &window};
    EnumWindows(
        [](HWND w, LPARAM data) -> BOOL {
            auto c = reinterpret_cast<Ctx*>(data);
            DWORD pid = 0;
            GetWindowThreadProcessId(w, &pid);
            if (pid == c->pid && IsWindowVisible(w)) {
                *c->result = w;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&ctx));
    check(window != nullptr, "test scene has a visible native window");
    auto err = capture.start(rules);
    check(err.empty(), "ETW collector starts");
    wait(4000);
    auto snapshot = capture.snapshot(pi.dwProcessId);
    std::cout << "ETW observed FPS: " << snapshot.fps << " / frames " << snapshot.frames.size()
              << " / " << fg::utf8(capture.status()) << '\n';
    check(snapshot.live && snapshot.fps > 10 && !snapshot.frames.empty(),
          "real DirectX presentations received and parsed");
    if (window) {
        RECT original{};
        GetWindowRect(window, &original);
        auto style = GetWindowLongPtrW(window, GWL_STYLE);
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
        auto result = borderless.apply(window, monitor.rcMonitor);
        wait(500);
        RECT current{};
        GetWindowRect(window, &current);
        check(result.empty() && !(GetWindowLongPtrW(window, GWL_STYLE) & WS_CAPTION) &&
                  EqualRect(&current, &monitor.rcMonitor),
              "borderless fills the chosen display and removes caption");
        std::wstring marker = L"Framoryx.WindowOwner." + std::to_wstring(GetCurrentProcessId());
        check(GetPropW(window, marker.c_str()) != nullptr,
              "window identity marker guards restoration");
        check(borderless.restore(window), "borderless restores original state");
        wait(300);
        GetWindowRect(window, &current);
        check(GetPropW(window, marker.c_str()) == nullptr,
              "window identity marker is removed after restoration");
        check(GetWindowLongPtrW(window, GWL_STYLE) == style && EqualRect(&original, &current),
              "original styles and window rectangle restored exactly");
    }
    auto blocked = hook.attach(pi.dwProcessId, "FramoryxTest.exe", fg::Rules{{}, {}}, 30);
    check(!blocked.empty(), "hook refuses a process absent from the allow list");
    auto antiCheat = fg::antiCheatRunning();
    if (!antiCheat.empty()) {
        std::cout << "SKIP: hook tests blocked by active anti-cheat/inspection: "
                  << fg::utf8(antiCheat) << '\n';
    } else {
        err = hook.attach(pi.dwProcessId, "FramoryxTest.exe", rules, 30);
        check(err.empty(), "explicit allow-listed DX11 hook request");
        for (int i = 0; i < 60 && !hook.active(); ++i)
            wait(100);
        std::cout << "Hook status: " << hook.status() << " / renderer " << hook.renderer() << '\n';
        check(hook.active(), "DLL loaded and DXGI hooks initialized");
        auto rate = [&](int fps) {
            hook.cap(fps);
            wait(500);
            auto n = hook.frameCount();
            double begin = fg::clockSeconds();
            wait(3000);
            double rate = double(hook.frameCount() - n) / (fg::clockSeconds() - begin);
            std::cout << "Measured " << fps << " cap: " << rate << " FPS\n";
            return rate;
        };
        if (hook.active()) {
            double thirty = rate(30);
            check(thirty >= 27 && thirty <= 31.5, "30 FPS limiter pacing");
            LONGLONG cursor = 0;
            auto events = hook.drainFrames(cursor);
            fg::Store hookHistory;
            for (auto& frame : events)
                hookHistory.add(frame, fg::clockSeconds());
            auto fallback = hookHistory.snapshot(pi.dwProcessId, fg::clockSeconds());
            check(events.size() > 30 && fallback.live && fallback.fps > 27 && fallback.fps < 32,
                  "hook frame ring supports FPS, graph and low statistics without ETW");
            check(hook.renderer() == 11, "in-process DX11 overlay renders");
            check(hook.hudFresh(),
                  "HUD reports actual recent drawing rather than only renderer initialization");
            hook.overlay(false);
            wait(1300);
            check(!hook.hudFresh(),
                  "turning the injected HUD off allows automatic desktop fallback");
            hook.corner(3);
            hook.overlay(true);
            wait(400);
            check(hook.hudFresh(), "injected HUD can be re-enabled at another corner");
            styleChecks("DX11 customized readouts, opacity and enlarged font atlas render");
            double sixty = rate(60);
            check(sixty >= 54 && sixty <= 62, "60 FPS limit changes live");
            double uncapped = rate(0);
            check(uncapped > 80, "0 restores uncapped presentation");
            hook.disable();
            wait(500);
            check(hook.status() == 2, "disable restores hook entry points");
            auto count = hook.frameCount();
            wait(500);
            check(hook.frameCount() == count, "disabled hook is inert");
            check(!hook.attach(pi.dwProcessId, "FramoryxTest.exe", rules, 30).empty(),
                  "duplicate injection is refused until the game restarts");
        }
    }
    capture.stop();
    check(!capture.running(), "ETW shutdown completes");
    check(WaitForSingleObject(process.h, 0) == WAIT_TIMEOUT,
          "test scene preserves compute-shader state and survives capture, hooking, resizing and "
          "disable");
    if (window)
        PostMessageW(window, WM_CLOSE, 0, 0);
    if (WaitForSingleObject(process.h, 2000) == WAIT_TIMEOUT)
        TerminateProcess(process.h, 0);
    if (antiCheat.empty()) {
        std::wstring command2 = L"\"" + scene + L"\" --seconds 20";
        PROCESS_INFORMATION pi2{};
        if (CreateProcessW(scene.c_str(), command2.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &si, &pi2)) {
            fg::Handle process2(pi2.hProcess), thread2(pi2.hThread);
            wait(700);
            auto error = hook.attach(pi2.dwProcessId, "FramoryxTest.exe", rules, 30);
            for (int i = 0; i < 60 && !hook.active() && error.empty(); ++i)
                wait(100);
            check(error.empty() && hook.active(),
                  "new game instance can attach after the previous one exits");
            if (hook.active()) {
                Sleep(4500);
                check(hook.status() == 2, "heartbeat loss automatically disables the hook");
                check(WaitForSingleObject(process2.h, 0) == WAIT_TIMEOUT,
                      "game survives a lost controller heartbeat");
            }
            HWND secondWindow = nullptr;
            Ctx secondCtx{pi2.dwProcessId, &secondWindow};
            EnumWindows(
                [](HWND w, LPARAM data) -> BOOL {
                    auto c = reinterpret_cast<Ctx*>(data);
                    DWORD pid = 0;
                    GetWindowThreadProcessId(w, &pid);
                    if (pid == c->pid && IsWindowVisible(w)) {
                        *c->result = w;
                        return FALSE;
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&secondCtx));
            if (secondWindow)
                PostMessageW(secondWindow, WM_CLOSE, 0, 0);
            if (WaitForSingleObject(process2.h, 2000) == WAIT_TIMEOUT)
                TerminateProcess(process2.h, 0);
        } else
            check(false, "second scene launch for heartbeat test");
    }
    {
        std::wstring scene12 = folder + L"\\tools\\FramoryxTest12.exe",
                     command12 = L"\"" + scene12 + L"\" --seconds 35";
        PROCESS_INFORMATION pi12{};
        if (CreateProcessW(scene12.c_str(), command12.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &si, &pi12)) {
            fg::Handle process12(pi12.hProcess), thread12(pi12.hThread);
            wait(1000);
            auto error = capture.start(rules);
            wait(3500);
            auto stats12 = capture.snapshot(pi12.dwProcessId);
            double baseline = stats12.fps;
            std::cout << "DX12 baseline: " << baseline << " FPS\n";
            check(error.empty() && stats12.live && baseline > 10,
                  "real DX12 presentations captured through ETW");
            if (antiCheat.empty() && WaitForSingleObject(process12.h, 0) == WAIT_TIMEOUT) {
                rules.allow.push_back("FramoryxTest12.exe");
                error = hook.attach(pi12.dwProcessId, "FramoryxTest12.exe", rules, 30);
                for (int i = 0; i < 60 && !hook.active() && error.empty(); ++i)
                    wait(100);
                check(error.empty() && hook.active(), "explicit DX12 process hook initializes");
                if (hook.active()) {
                    auto rate12 = [&](int cap) {
                        hook.cap(cap);
                        wait(500);
                        auto initial = hook.frameCount();
                        double time = fg::clockSeconds();
                        wait(2500);
                        double rate =
                            double(hook.frameCount() - initial) / (fg::clockSeconds() - time);
                        std::cout << "DX12 " << cap << " cap: " << rate << " FPS\n";
                        return rate;
                    };
                    auto fps30 = rate12(30);
                    check(fps30 > 26 && fps30 < 32, "DX12 30 FPS limiter pacing");
                    check(hook.renderer() == 12 && hook.hudFresh(),
                          "DX12 in-process HUD draws through the game's observed direct queue");
                    hook.overlay(false);
                    wait(1300);
                    check(!hook.hudFresh(), "DX12 desktop fallback becomes available when its "
                                            "injected HUD is disabled");
                    hook.overlay(true);
                    wait(600);
                    check(hook.hudFresh(), "DX12 injected HUD can be enabled again");
                    styleChecks("DX12 customized readouts and enlarged font atlas render");
                    HWND resizeWindow = nullptr;
                    Ctx resizeCtx{pi12.dwProcessId, &resizeWindow};
                    EnumWindows(
                        [](HWND w, LPARAM p) -> BOOL {
                            auto c = reinterpret_cast<Ctx*>(p);
                            DWORD id = 0;
                            GetWindowThreadProcessId(w, &id);
                            if (id == c->pid && IsWindowVisible(w)) {
                                *c->result = w;
                                return FALSE;
                            }
                            return TRUE;
                        },
                        reinterpret_cast<LPARAM>(&resizeCtx));
                    if (resizeWindow) {
                        MONITORINFO display{};
                        display.cbSize = sizeof(display);
                        GetMonitorInfoW(MonitorFromWindow(resizeWindow, MONITOR_DEFAULTTONEAREST),
                                        &display);
                        auto resizeError = borderless.apply(resizeWindow, display.rcMonitor);
                        wait(1000);
                        check(resizeError.empty() && hook.hudFresh() &&
                                  WaitForSingleObject(process12.h, 0) == WAIT_TIMEOUT,
                              "DX12 HUD survives borderless resize without retaining back buffers");
                        borderless.restore(resizeWindow);
                        wait(700);
                        check(hook.hudFresh(),
                              "DX12 HUD survives restoration to its original window");
                    }
                    auto fps60 = rate12(60);
                    check(fps60 > 52 && fps60 < 63, "DX12 60 FPS limit updates live");
                    auto unlimited = rate12(0);
                    check(unlimited > std::min(80.0, baseline * 0.75),
                          "DX12 uncapped mode restores throughput");
                    hook.disable();
                    wait(500);
                    check(hook.status() == 2, "DX12 hook disables cleanly");
                }
            }
            capture.stop();
            check(WaitForSingleObject(process12.h, 0) == WAIT_TIMEOUT,
                  "DX12 scene survives capture and optional limiter");
            HWND window12 = nullptr;
            Ctx context12{pi12.dwProcessId, &window12};
            EnumWindows(
                [](HWND w, LPARAM data) -> BOOL {
                    auto c = reinterpret_cast<Ctx*>(data);
                    DWORD pid = 0;
                    GetWindowThreadProcessId(w, &pid);
                    if (pid == c->pid && IsWindowVisible(w)) {
                        *c->result = w;
                        return FALSE;
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&context12));
            if (window12)
                PostMessageW(window12, WM_CLOSE, 0, 0);
            if (WaitForSingleObject(process12.h, 2000) == WAIT_TIMEOUT)
                TerminateProcess(process12.h, 0);
        } else
            check(false, "DX12 test scene launch");
    }
    {
        auto scene32 = folder + L"\\tools\\FramoryxTest32.exe";
        auto command32 = L"\"" + scene32 + L"\" --seconds 30 --verify-pipeline";
        PROCESS_INFORMATION info32{};
        if (CreateProcessW(scene32.c_str(), command32.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &si, &info32)) {
            fg::Handle process32(info32.hProcess), thread32(info32.hThread);
            wait(800);
            auto error = capture.start(rules);
            wait(3000);
            auto baseline = capture.snapshot(info32.dwProcessId);
            check(error.empty() && baseline.live && baseline.fps > 10,
                  "32-bit DX11 presentations captured through ETW");
            if (antiCheat.empty()) {
                rules.allow.push_back("FramoryxTest32.exe");
                error = hook.attach(info32.dwProcessId, "FramoryxTest32.exe", rules, 30);
                for (int i = 0; i < 70 && error.empty() && !hook.active(); ++i)
                    wait(100);
                if (!error.empty())
                    std::cout << "32-bit attach: " << fg::utf8(error) << '\n';
                check(error.empty() && hook.active(),
                      "32-bit helper loads the matching DX11 hook automatically");
                if (hook.active()) {
                    auto rate32 = [&](int cap) {
                        hook.cap(cap);
                        wait(500);
                        auto count = hook.frameCount();
                        auto time = fg::clockSeconds();
                        wait(2500);
                        double rate =
                            double(hook.frameCount() - count) / (fg::clockSeconds() - time);
                        std::cout << "32-bit " << cap << " cap: " << rate << " FPS\n";
                        return rate;
                    };
                    auto fps30 = rate32(30);
                    check(fps30 > 26 && fps30 < 32, "32-bit 30 FPS limiter pacing");
                    check(hook.renderer() == 11 && hook.hudFresh(),
                          "32-bit in-process HUD reports actual drawing");
                    styleChecks("32-bit customized HUD fields and sharp font atlas render");
                    LONGLONG cursor = 0;
                    auto frames = hook.drainFrames(cursor);
                    check(frames.size() > 30 && frames.back().ms > 0,
                          "cross-architecture frame-ring records remain coherent");
                    auto fps60 = rate32(60);
                    check(fps60 > 52 && fps60 < 63, "32-bit 60 FPS limit updates live");
                    auto uncapped = rate32(0);
                    check(uncapped > 80, "32-bit uncapped mode restores throughput");
                    hook.disable();
                    wait(500);
                    check(hook.status() == 2, "32-bit hook disables cleanly");
                }
            }
            capture.stop();
            check(WaitForSingleObject(process32.h, 0) == WAIT_TIMEOUT,
                  "32-bit scene preserves pipeline state and survives the optional hook");
            HWND window32 = nullptr;
            Ctx context32{info32.dwProcessId, &window32};
            EnumWindows(
                [](HWND w, LPARAM data) -> BOOL {
                    auto c = reinterpret_cast<Ctx*>(data);
                    DWORD pid = 0;
                    GetWindowThreadProcessId(w, &pid);
                    if (pid == c->pid && IsWindowVisible(w)) {
                        *c->result = w;
                        return FALSE;
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&context32));
            if (window32)
                PostMessageW(window32, WM_CLOSE, 0, 0);
            if (WaitForSingleObject(process32.h, 2000) == WAIT_TIMEOUT)
                TerminateProcess(process32.h, 0);
        } else
            check(false, "32-bit test scene launch");
    }
    if (antiCheat.empty()) {
        auto executable = folder + L"\\tools\\FramoryxTest12.exe";
        auto command = L"\"" + executable + L"\" --seconds 20 --multi-queue";
        PROCESS_INFORMATION info{};
        if (CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                           nullptr, &si, &info)) {
            fg::Handle process(info.hProcess), thread(info.hThread);
            wait(800);
            rules.allow.push_back("FramoryxTest12.exe");
            auto error = hook.attach(info.dwProcessId, "FramoryxTest12.exe", rules, 30);
            for (int i = 0; i < 70 && !hook.active() && error.empty(); ++i)
                wait(100);
            wait(1400);
            check(error.empty() && hook.active(),
                  "multiple-direct-queue DX12 scene keeps the limiter available");
            check(!hook.hudFresh(),
                  "ambiguous DX12 direct queues automatically decline in-game drawing");
            auto initial = hook.frameCount();
            double start = fg::clockSeconds();
            wait(1500);
            double rate = double(hook.frameCount() - initial) / (fg::clockSeconds() - start);
            check(rate > 26 && rate < 33,
                  "conservative DX12 HUD fallback preserves the requested FPS cap");
            hook.disable();
            wait(400);
            check(WaitForSingleObject(process.h, 0) == WAIT_TIMEOUT,
                  "multiple-queue scene survives fallback and disable");
            HWND target = nullptr;
            Ctx context{info.dwProcessId, &target};
            EnumWindows(
                [](HWND w, LPARAM data) -> BOOL {
                    auto c = reinterpret_cast<Ctx*>(data);
                    DWORD pid = 0;
                    GetWindowThreadProcessId(w, &pid);
                    if (pid == c->pid && IsWindowVisible(w)) {
                        *c->result = w;
                        return FALSE;
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&context));
            if (target)
                PostMessageW(target, WM_CLOSE, 0, 0);
            if (WaitForSingleObject(process.h, 2000) == WAIT_TIMEOUT)
                TerminateProcess(process.h, 0);
        } else
            check(false, "multiple-queue DX12 test scene starts");
    }
    std::cout << passed << " passed, " << failed << " failed\n";
    CoUninitialize();
    return failed ? 1 : 0;
}
