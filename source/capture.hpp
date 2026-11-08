#pragma once
#include "platform.hpp"
#include <evntrace.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <array>

namespace fg {
inline double clockSeconds() {
    return GetTickCount64() / 1000.0;
}
inline unsigned cleanupAbandonedCaptures() {
    constexpr ULONG capacity = 128;
    std::vector<std::vector<BYTE>> buffers(
        capacity, std::vector<BYTE>(sizeof(EVENT_TRACE_PROPERTIES) + 4096));
    std::vector<EVENT_TRACE_PROPERTIES*> properties(capacity);
    for (ULONG i = 0; i < capacity; ++i) {
        auto p = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffers[i].data());
        p->Wnode.BufferSize = ULONG(buffers[i].size());
        p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        p->LogFileNameOffset = sizeof(EVENT_TRACE_PROPERTIES) + 2048;
        properties[i] = p;
    }
    ULONG count = 0;
    if (QueryAllTracesW(properties.data(), capacity, &count) != ERROR_SUCCESS)
        return 0;
    unsigned stopped = 0;
    for (ULONG i = 0; i < std::min(count, capacity); ++i) {
        auto p = properties[i];
        if (p->LoggerNameOffset + sizeof(wchar_t) > buffers[i].size())
            continue;
        auto begin = reinterpret_cast<const wchar_t*>(buffers[i].data() + p->LoggerNameOffset);
        size_t maximum = (buffers[i].size() - p->LoggerNameOffset) / sizeof(wchar_t);
        size_t length = 0;
        while (length < maximum && begin[length])
            ++length;
        if (length == maximum)
            continue;
        std::wstring name(begin, length);
        constexpr wchar_t prefix[] = L"Framoryx_";
        if (name.rfind(prefix, 0) != 0)
            continue;
        auto end = name.find(L'_', 9);
        if (end == std::wstring::npos || end == 9 || end - 9 > 10)
            continue;
        auto digits = name.substr(9, end - 9);
        if (std::any_of(digits.begin(), digits.end(),
                        [](wchar_t c) { return c < L'0' || c > L'9'; }))
            continue;
        auto value = number(utf8(digits));
        if (!value || *value < 1 || *value > UINT32_MAX || std::floor(*value) != *value)
            continue;
        DWORD owner = DWORD(*value);
        if (owner == GetCurrentProcessId())
            continue;
        SetLastError(ERROR_SUCCESS);
        Handle process(OpenProcess(SYNCHRONIZE, FALSE, owner));
        DWORD error = GetLastError();
        bool gone = process ? WaitForSingleObject(process.h, 0) == WAIT_OBJECT_0
                            : error == ERROR_INVALID_PARAMETER;
        // Never stop another live controller's session or infer death from access denied.
        if (gone && ControlTraceW(0, name.c_str(), p, EVENT_TRACE_CONTROL_STOP) == ERROR_SUCCESS)
            ++stopped;
    }
    return stopped;
}
class Capture {
    mutable std::mutex mutex;
    Store store;
    Rules rules;
    std::wstring message = L"Ready. Start capture, then focus a game or select its window.";
    std::wstring diagnosticOutput = L"No capture diagnostics yet.";
    std::thread reader;
    Handle process, job, pipe;
    std::wstring session;
    std::atomic<bool> running_{false}, stopping{false};
    void stopCollector() {
        // Session ownership survives collector exit; always clean up this unique session.
        if (!session.empty()) {
            std::array<BYTE, sizeof(EVENT_TRACE_PROPERTIES) + 1024> memory{};
            auto p = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(memory.data());
            p->Wnode.BufferSize = ULONG(memory.size());
            p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
            ControlTraceW(0, session.c_str(), p, EVENT_TRACE_CONTROL_STOP);
        }
        if (process && WaitForSingleObject(process.h, 1200) == WAIT_TIMEOUT)
            TerminateProcess(process.h, 0);
    }

  public:
    ~Capture() {
        stop();
    }
    bool running() const {
        return running_;
    }
    void setRules(const Rules& r) {
        std::lock_guard lock(mutex);
        rules = r;
        store.clear();
    }
    Stats snapshot(DWORD pid) const {
        std::lock_guard lock(mutex);
        auto s = store.snapshot(pid, clockSeconds());
        s.live = s.live && running_;
        if (!s.live) {
            s.fps = 0;
            s.ms = 0;
        }
        return s;
    }
    std::wstring status() const {
        std::lock_guard lock(mutex);
        return message;
    }
    std::wstring details() const {
        std::lock_guard lock(mutex);
        return diagnosticOutput;
    }
    void stop() {
        stopping = true;
        stopCollector();
        if (reader.joinable())
            reader.join();
        pipe.reset();
        process.reset();
        job.reset();
        session.clear();
        running_ = false;
    }
    std::wstring start(const Rules& r) {
        stop();
        cleanupAbandonedCaptures();
        stopping = false;
        rules = r;
        std::wstring exe = executableDir() + L"\\tools\\PresentMon.exe";
        if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES)
            return L"tools\\PresentMon.exe is missing. Extract the complete application folder.";
        SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
        HANDLE read = nullptr, write = nullptr;
        if (!CreatePipe(&read, &write, &security, 0))
            return L"Cannot open capture pipe: " + winError();
        pipe.reset(read);
        Handle writer(write);
        SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
        Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 &security, OPEN_EXISTING, 0, nullptr));
        session = L"Framoryx_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
                  std::to_wstring(GetTickCount64());
        std::wstring command =
            L"\"" + exe +
            L"\" --output_stdout --no_console_stats --v1_metrics --no_track_display --no_track_gpu "
            L"--no_track_input --session_name " +
            session;
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = input.h;
        si.hStdOutput = writer.h;
        si.hStdError = writer.h;
        PROCESS_INFORMATION pi{};
        job.reset(CreateJobObjectW(nullptr, nullptr));
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job || !SetInformationJobObject(job.h, JobObjectExtendedLimitInformation, &limits,
                                             sizeof(limits)))
            return L"Cannot create capture lifetime controls: " + winError();
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, executableDir().c_str(),
                            &si, &pi))
            return L"Cannot start PresentMon: " + winError();
        process.reset(pi.hProcess);
        Handle thread(pi.hThread);
        if (!AssignProcessToJobObject(job.h, process.h)) {
            TerminateProcess(process.h, 1);
            return L"Cannot manage the capture process: " + winError();
        }
        {
            std::lock_guard lock(mutex);
            store.clear();
            message =
                L"ETW capture active. Waiting for presentation events from the selected game.";
            diagnosticOutput = L"PresentMon capture started. Session: " + session;
        }
        running_ = true;
        ResumeThread(thread.h);
        writer.reset();
        try {
            reader = std::thread([this] {
                try {
                    Parser parser;
                    std::string pending, diagnostic;
                    char data[16384];
                    DWORD count = 0;
                    bool seenFrame = false;
                    while (ReadFile(pipe.h, data, sizeof(data), &count, nullptr) && count) {
                        pending.append(data, count);
                        size_t offset = 0, newline;
                        while ((newline = pending.find('\n', offset)) != std::string::npos) {
                            std::string line = pending.substr(offset, newline - offset);
                            offset = newline + 1;
                            auto frame = parser.parse(line);
                            if (frame) {
                                std::lock_guard lock(mutex);
                                if (!seenFrame) {
                                    seenFrame = true;
                                    message =
                                        L"ETW capture active. Select a game window or focus it to "
                                        L"view its frames.";
                                }
                                if (!rules.blocked(frame->app))
                                    store.add(*frame, clockSeconds());
                            } else if (!line.empty() && line.find(',') == std::string::npos &&
                                       diagnostic.size() < 3000)
                                diagnostic += line + " ";
                        }
                        pending.erase(0, offset);
                        if (pending.size() > 1024 * 1024)
                            pending.clear();
                    }
                    WaitForSingleObject(process.h, 1000);
                    DWORD code = 0;
                    GetExitCodeProcess(process.h, &code);
                    running_ = false;
                    std::lock_guard lock(mutex);
                    diagnosticOutput = L"PresentMon exit code: " + std::to_wstring(code) + L"\n\n" +
                                       wide(diagnostic.substr(0, 1800));
                    if (stopping)
                        message = L"Capture stopped. Rolling data is available for CSV export.";
                    else if (lower(diagnostic).find("access denied") != std::string::npos)
                        message = L"ETW access denied. Try Run as administrator or a Performance "
                                  L"Log Users "
                                  L"account. Anti-cheat may also restrict capture.";
                    else if (code || !diagnostic.empty())
                        message =
                            L"Capture stopped. Open Capture details for PresentMon’s output and "
                            L"troubleshooting information.";
                    else
                        message = L"Capture ended. Press Start capture to begin another session.";
                } catch (const std::exception&) {
                    stopCollector();
                    running_ = false;
                    std::lock_guard lock(mutex);
                    message = L"Capture reader stopped unexpectedly. Restart capture to retry.";
                    diagnosticOutput = L"A capture parsing or resource error occurred. No game "
                                       L"process was changed.";
                }
            });
        } catch (const std::exception&) {
            stop();
            return L"Windows could not start the capture reader. The collector was stopped.";
        }
        return {};
    }
};
} // namespace fg
