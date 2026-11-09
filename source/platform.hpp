#pragma once
#include "core.hpp"
#include "shared.hpp"
#include <tlhelp32.h>
#include <limits>
#include <map>
#include <utility>

namespace fg {
inline std::wstring wide(const std::string& s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n);
    return out;
}
inline std::string utf8(const std::wstring& s) {
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}
inline std::wstring executableDir() {
    wchar_t path[32768]{};
    GetModuleFileNameW(nullptr, path, 32768);
    std::wstring s(path);
    return s.substr(0, s.find_last_of(L"\\/"));
}
inline std::wstring winError(DWORD code = GetLastError()) {
    wchar_t* message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::wstring s = message ? message : L"Windows error " + std::to_wstring(code);
    if (message)
        LocalFree(message);
    while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n'))
        s.pop_back();
    return s;
}
struct Handle {
    HANDLE h = nullptr;
    Handle() = default;
    explicit Handle(HANDLE value) : h(value) {}
    ~Handle() {
        reset();
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : h(std::exchange(other.h, nullptr)) {}
    Handle& operator=(Handle&& other) noexcept {
        reset(std::exchange(other.h, nullptr));
        return *this;
    }
    void reset(HANDLE value = nullptr) {
        if (h && h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
        h = value;
    }
    explicit operator bool() const {
        return h && h != INVALID_HANDLE_VALUE;
    }
};
inline std::map<DWORD, std::string> processes() {
    std::map<DWORD, std::string> list;
    Handle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    PROCESSENTRY32W e{};
    e.dwSize = sizeof(e);
    if (snap && Process32FirstW(snap.h, &e))
        do {
            list[e.th32ProcessID] = utf8(e.szExeFile);
        } while (Process32NextW(snap.h, &e));
    return list;
}
inline std::string processImageBasename(HANDLE process) {
    std::wstring path(32768, L'\0');
    DWORD length = DWORD(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &length))
        return {};
    path.resize(length);
    const auto separator = path.find_last_of(L"\\/");
    return utf8(path.substr(separator == std::wstring::npos ? 0 : separator + 1));
}
inline std::wstring antiCheatRunning() {
    Handle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap)
        return L"Cannot inspect running processes";
    PROCESSENTRY32W e{};
    e.dwSize = sizeof(e);
    if (!Process32FirstW(snap.h, &e))
        return L"Cannot inspect running processes";
    do {
        if (matches(antiCheatNames, utf8(e.szExeFile)))
            return e.szExeFile;
    } while (Process32NextW(snap.h, &e));
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm)
        return L"Cannot inspect active services";
    DWORD needed = 0, count = 0, resume = 0;
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32 | SERVICE_DRIVER, SERVICE_ACTIVE,
                          nullptr, 0, &needed, &count, &resume, nullptr);
    std::vector<BYTE> data(needed + 1024);
    resume = 0;
    BOOL ok = EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32 | SERVICE_DRIVER,
                                    SERVICE_ACTIVE, data.data(), DWORD(data.size()), &needed,
                                    &count, &resume, nullptr);
    CloseServiceHandle(scm);
    if (!ok)
        return L"Cannot inspect active services";
    auto services = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(data.data());
    const std::vector<std::string> patterns{
        "vgk",          "vgc",         "bedaisy",  "beservice*",  "easyanticheat*", "faceit*",
        "eaanticheat*", "eanticheat*", "ace-base", "ace-protect", "equ8*"};
    for (DWORD i = 0; i < count; ++i)
        if (matches(patterns, utf8(services[i].lpServiceName)))
            return services[i].lpServiceName;
    return {};
}
class HookClient {
    Handle mapping, target;
    HookControl* ipc = nullptr;
    DWORD pid_ = 0;
    Handle loaderThread;
    void* loaderPath = nullptr;
    ULONGLONG loaderStarted = 0;
    bool loaderIsBroker = false;
    std::string targetName;
    HudText sentHudText;

    static LONG hudFieldBits(const HudOptions& options) {
        return LONG(options.fields |
            (options.temperatureColors ? hudTemperatureColorsFlag : 0u) |
            (options.textGlow ? hudTextGlowFlag : 0u) |
            (options.valueColors ? hudValueColorsFlag : 0u));
    }

    void releaseExitedTarget() {
        if (!target || WaitForSingleObject(target.h, 0) != WAIT_OBJECT_0)
            return;
        // Process exit releases the remote allocation and stops the DLL without an IPC update.
        loaderThread.reset();
        loaderPath = nullptr;
        if (ipc) {
            UnmapViewOfFile(ipc);
            ipc = nullptr;
        }
        mapping.reset();
        target.reset();
        pid_ = 0;
        targetName.clear();
        sentHudText = {};
    }

  public:
    ~HookClient() {
        disable();
        if (ipc)
            UnmapViewOfFile(ipc);
    }
    DWORD pid() const {
        return pid_;
    }
    const std::string& name() const {
        return targetName;
    }
    void hudOptions(HudOptions options) {
        if (!ipc)
            return;
        options.validate();
        InterlockedExchange(&ipc->hudFields, hudFieldBits(options));
        InterlockedExchange(&ipc->hudScale, options.scale);
        InterlockedExchange(&ipc->hudOpacity, options.opacity);
        InterlockedExchange(&ipc->hudAccent, options.accent);
        InterlockedExchange(&ipc->hudStyle, options.style);
        InterlockedExchange(&ipc->hudOrderLo,LONG(options.order&0xffffffffu));
        InterlockedExchange(&ipc->hudOrderHi,LONG(options.order>>32));
        InterlockedExchange(&ipc->hudX,options.x);InterlockedExchange(&ipc->hudY,options.y);
        InterlockedExchange(&ipc->hudFontScale,options.fontScale);
    }
    void hudText(const HudText& text) {
        if (!ipc || text == sentHudText)
            return;
        std::array<LONG, hudTextWordCount> words{};
        auto* bytes = reinterpret_cast<char*>(words.data());
        for (size_t i = 0; i < text.labels.size(); ++i)
            text.labels[i].copy(bytes + i * (hudLabelBytes + 1), hudLabelBytes);
        text.fontFamily.copy(bytes + text.labels.size() * (hudLabelBytes + 1), hudFontFamilyBytes);
        // The generation stays odd while writing, so the render thread can skip partial updates.
        InterlockedIncrement(&ipc->hudTextGeneration);
        for (size_t i = 0; i < words.size(); ++i)
            InterlockedExchange(&ipc->hudTextWords[i], words[i]);
        InterlockedIncrement(&ipc->hudTextGeneration);
        sentHudText = text;
    }
    void sensors(const std::array<double,6>& values){if(ipc)for(size_t i=0;i<values.size();++i)InterlockedExchange(&ipc->sensors[i],std::isfinite(values[i])&&values[i]>=0?LONG(std::min(values[i],10000.)*100):-1);}
    void systemMetrics(double cpu, double ramGB) {
        if (ipc) {
            const auto cpuHundredths = std::isfinite(cpu) && cpu >= 0 ? LONG(std::min(cpu, 100.) * 100) : -1;
            const auto ramMiB = std::isfinite(ramGB) && ramGB >= 0
                ? LONG(std::min(ramGB * 1024, double(std::numeric_limits<LONG>::max()))) : -1;
            InterlockedExchange(&ipc->cpuHundredths, cpuHundredths);
            InterlockedExchange(&ipc->ramMiB, ramMiB);
        }
    }
    int status() const {
        return ipc ? read32(&ipc->status) : 2;
    }
    bool active() const {
        return ipc && status() == 1;
    }
    LONGLONG frameCount() const {
        return ipc ? std::max<LONGLONG>(0, read64(&ipc->frames)) : 0;
    }
    std::vector<Frame> drainFrames(LONGLONG& cursor) const {
        std::vector<Frame> result;
        if (!ipc)
            return result;
        const auto end = frameCount();
        if (cursor < 0 || cursor > end)
            cursor = 0;
        const auto count = std::min(end - cursor, LONGLONG(hookRingSize));
        if (!count)
            return result;
        LARGE_INTEGER now{}, frequency{};
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&frequency);
        for (auto remaining = count; remaining > 0; --remaining) {
            const auto n = end - remaining + 1;
            auto& entry = ipc->ring[n % hookRingSize];
            if (read64(&entry.sequence) != n)
                continue;
            auto time = read64(&entry.qpc), interval = read64(&entry.interval);
            if (read64(&entry.sequence) != n || time <= 0 || time > now.QuadPart ||
                interval <= 0 || interval > frequency.QuadPart * 60)
                continue;
            result.push_back({targetName, "Hook primary", pid_, double(time) / frequency.QuadPart,
                              1000.0 * interval / frequency.QuadPart});
        }
        cursor = end;
        return result;
    }
    void tick() {
        releaseExitedTarget();
        if (ipc)
            InterlockedExchange64(&ipc->heartbeat, GetTickCount64());
        if (loaderThread && WaitForSingleObject(loaderThread.h, 0) == WAIT_OBJECT_0) {
            DWORD result = 0;
            if (loaderIsBroker)
                GetExitCodeProcess(loaderThread.h, &result);
            else
                GetExitCodeThread(loaderThread.h, &result);
            if (loaderPath)
                VirtualFreeEx(target.h, loaderPath, 0, MEM_RELEASE);
            loaderPath = nullptr;
            loaderThread.reset();
            if ((loaderIsBroker ? result != 0 : result == 0) && ipc && status() == 0) {
                disable();
                InterlockedExchange(&ipc->status, -2);
            }
        }
        if (GetTickCount64() - loaderStarted > 8000 && ipc && status() == 0) {
            disable();
            InterlockedExchange(&ipc->status, -3);
        }
    }
    void cap(int fps) {
        if (ipc)
            InterlockedExchange(&ipc->cap, fps);
    }
    void overlay(bool show) {
        if (ipc)
            InterlockedExchange(&ipc->overlay, show ? 1 : 0);
    }
    void corner(int value) {
        if (ipc)
            InterlockedExchange(&ipc->corner, std::clamp(value, 0, 3));
    }
    int renderer() const {
        return ipc ? read32(&ipc->renderer) : 0;
    }
    bool hudFresh() const {
        if (!ipc)
            return false;
        auto last = read64(&ipc->lastHudQpc);
        LARGE_INTEGER now{}, frequency{};
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&frequency);
        return last > 0 && last <= now.QuadPart && now.QuadPart - last < frequency.QuadPart;
    }
    void disable() {
        if (ipc)
            InterlockedExchange(&ipc->request, 1);
    }
    bool targetAlive() const {
        return target && WaitForSingleObject(target.h, 0) == WAIT_TIMEOUT;
    }
    std::wstring attach(DWORD pid, const std::string& expectedName, const Rules& rules, int fps) {
        releaseExitedTarget();
        if (active() || loaderThread || (ipc && status() == 0 && targetAlive()))
            return L"A hook is active or still loading. Disable it and restart that game before "
                   L"attaching again.";
        if (!rules.hookAllowed(expectedName))
            return L"Add this executable to the hook allow list first. Block rules always win.";
        auto blocker = antiCheatRunning();
        if (!blocker.empty())
            return L"Hook blocked: " + blocker +
                   L". Use ETW monitoring and an in-game or driver limiter.";
        if (!pid || pid == GetCurrentProcessId())
            return L"The selected process has changed. Refresh the window list.";
        // Injection is confined to this explicit opt-in path. ETW and borderless mode never call
        // it.
        Handle process(OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                       PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ |
                                       SYNCHRONIZE,
                                   FALSE, pid));
        if (!process)
            return L"Cannot open the selected process: " + winError();
        const auto currentName = processImageBasename(process.h);
        if (currentName.empty() || lower(currentName) != lower(expectedName))
            return L"The selected process has changed. Refresh the window list.";
        USHORT machine = 0, native = 0;
        if (!IsWow64Process2(process.h, &machine, &native) ||
            (machine != IMAGE_FILE_MACHINE_UNKNOWN && machine != IMAGE_FILE_MACHINE_I386) ||
            native != IMAGE_FILE_MACHINE_AMD64)
            return L"Hook mode supports 32-bit and 64-bit processes on x64 Windows.";
        bool legacy = machine == IMAGE_FILE_MACHINE_I386;
        auto birth = creationTime(process.h);
        if (!birth)
            return L"Cannot verify the selected process identity.";
        auto mapName = mappingName(pid);
        Handle newMap(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                         sizeof(HookControl), mapName.c_str()));
        if (!newMap)
            return L"Cannot create local hook controls: " + winError();
        if (GetLastError() == ERROR_ALREADY_EXISTS)
            return L"The hook DLL was already loaded. Restart the game before attaching again.";
        auto* view = static_cast<HookControl*>(
            MapViewOfFile(newMap.h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HookControl)));
        if (!view)
            return L"Cannot map hook controls: " + winError();
        ZeroMemory(view, sizeof(*view));
        view->magic = ipcMagic;
        view->version = ipcVersion;
        sentHudText = {};
        view->ownerPid = GetCurrentProcessId();
        view->targetPid = pid;
        view->creationTime = birth;
        view->cap = fps;
        view->overlay = 1;
        view->cpuHundredths = -1;
        view->ramMiB = -1;
        view->hudFields = hudFieldBits(HudOptions{});
        view->hudOpacity = 90;
        view->hudOrderLo=LONG(HudOptions::defaultOrder&0xffffffffu);view->hudOrderHi=LONG(HudOptions::defaultOrder>>32);
        view->hudX=-1;view->hudY=-1;view->hudFontScale=100;
        for(auto& sensor:view->sensors)sensor=-1;
        InterlockedExchange64(&view->heartbeat, GetTickCount64());
        std::wstring dll =
            executableDir() + (legacy ? L"\\FramoryxHook32.dll" : L"\\FramoryxHook.dll");
        if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
            UnmapViewOfFile(view);
            return L"The matching hook module is missing. Keep the app folder intact.";
        }
        if (legacy) {
            auto helper = executableDir() + L"\\tools\\FramoryxInjector32.exe";
            auto command = L"\"" + helper + L"\" --pid " + std::to_wstring(pid);
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESHOWWINDOW;
            startup.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION info{};
            if (!CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, FALSE,
                                CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info)) {
                auto error = winError();
                UnmapViewOfFile(view);
                return L"Cannot start the 32-bit compatibility helper: " + error;
            }
            CloseHandle(info.hThread);
            if (ipc)
                UnmapViewOfFile(ipc);
            mapping = std::move(newMap);
            target = std::move(process);
            ipc = view;
            pid_ = pid;
            targetName = expectedName;
            loaderThread.reset(info.hProcess);
            loaderPath = nullptr;
            loaderIsBroker = true;
            loaderStarted = GetTickCount64();
            return {};
        }
        void* remote = VirtualAllocEx(process.h, nullptr, (dll.size() + 1) * sizeof(wchar_t),
                                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!remote) {
            UnmapViewOfFile(view);
            return L"DLL path allocation failed: " + winError();
        }
        SIZE_T written = 0;
        if (!WriteProcessMemory(process.h, remote, dll.c_str(), (dll.size() + 1) * sizeof(wchar_t),
                                &written)) {
            auto err = winError();
            VirtualFreeEx(process.h, remote, 0, MEM_RELEASE);
            UnmapViewOfFile(view);
            return L"DLL path write failed: " + err;
        }
        // Resolve LoadLibraryW in the remote module by RVA. Do not assume identical ASLR bases.
        auto loader = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
        HMODULE localModule = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(loader), &localModule);
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(localModule, modulePath, MAX_PATH);
        std::wstring moduleName = modulePath;
        moduleName = moduleName.substr(moduleName.find_last_of(L"\\/") + 1);
        uintptr_t remoteBase = 0;
        Handle modules(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        if (modules && Module32FirstW(modules.h, &me))
            do {
                if (_wcsicmp(me.szModule, moduleName.c_str()) == 0)
                    remoteBase = reinterpret_cast<uintptr_t>(me.modBaseAddr);
            } while (Module32NextW(modules.h, &me));
        if (!remoteBase) {
            VirtualFreeEx(process.h, remote, 0, MEM_RELEASE);
            UnmapViewOfFile(view);
            return L"Cannot resolve the Windows loader in the selected process.";
        }
        auto start = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            remoteBase + reinterpret_cast<uintptr_t>(loader) -
            reinterpret_cast<uintptr_t>(localModule));
        Handle thread(CreateRemoteThread(process.h, nullptr, 0, start, remote, 0, nullptr));
        if (!thread) {
            auto err = winError();
            VirtualFreeEx(process.h, remote, 0, MEM_RELEASE);
            UnmapViewOfFile(view);
            return L"DLL load was blocked: " + err;
        }
        if (ipc)
            UnmapViewOfFile(ipc);
        mapping = std::move(newMap);
        target = std::move(process);
        ipc = view;
        pid_ = pid;
        targetName = expectedName;
        loaderThread = std::move(thread);
        loaderPath = remote;
        loaderIsBroker = false;
        loaderStarted = GetTickCount64();
        return {};
    }
};

struct SavedWindow {
    HWND window;
    DWORD pid;
    LONG_PTR style, exstyle;
    WINDOWPLACEMENT placement;
    RECT rect;
    HANDLE marker;
};
class Borderless {
    std::vector<SavedWindow> saved;
    std::wstring markerName = L"Framoryx.WindowOwner." + std::to_wstring(GetCurrentProcessId());

  public:
    ~Borderless() {
        restoreAll();
    }
    bool contains(HWND w) const {
        for (auto& s : saved)
            if (s.window == w)
                return true;
        return false;
    }
    std::wstring apply(HWND w, const RECT& monitor) {
        if (!IsWindow(w))
            return L"This window is no longer available.";
        if (IsHungAppWindow(w))
            return L"This game window is not responding. Try again after it responds.";
        DWORD pid = 0;
        GetWindowThreadProcessId(w, &pid);
        if (!contains(w)) {
            SavedWindow s{};
            s.window = w;
            s.pid = pid;
            s.style = GetWindowLongPtrW(w, GWL_STYLE);
            s.exstyle = GetWindowLongPtrW(w, GWL_EXSTYLE);
            s.placement.length = sizeof(s.placement);
            if (!GetWindowPlacement(w, &s.placement) || !GetWindowRect(w, &s.rect))
                return L"Cannot read the window placement.";
            s.marker = reinterpret_cast<HANDLE>(uintptr_t(GetTickCount64() | 1));
            if (!SetPropW(w, markerName.c_str(), s.marker))
                return L"Cannot mark this window for safe restoration: " + winError();
            saved.push_back(s);
        }
        auto style = GetWindowLongPtrW(w, GWL_STYLE);
        SetLastError(0);
        auto result = SetWindowLongPtrW(
            w, GWL_STYLE,
            (style & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU)) |
                WS_POPUP);
        if (!result && GetLastError()) {
            restore(w);
            return L"Window style change failed: " + winError();
        }
        auto ex = GetWindowLongPtrW(w, GWL_EXSTYLE);
        SetLastError(0);
        auto changed = SetWindowLongPtrW(
            w, GWL_EXSTYLE,
            ex & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE));
        if (!changed && GetLastError()) {
            auto error = winError();
            restore(w);
            return L"Window frame change failed: " + error;
        }
        ShowWindowAsync(w, SW_RESTORE);
        if (!SetWindowPos(w, nullptr, monitor.left, monitor.top, monitor.right - monitor.left,
                          monitor.bottom - monitor.top,
                          SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS)) {
            auto error = winError();
            restore(w);
            return L"Window resize failed: " + error;
        }
        return {};
    }
    bool restore(HWND w) {
        for (auto i = saved.begin(); i != saved.end(); ++i)
            if (i->window == w) {
                DWORD pid = 0;
                GetWindowThreadProcessId(w, &pid);
                if (!IsWindow(w) || pid != i->pid || GetPropW(w, markerName.c_str()) != i->marker) {
                    saved.erase(i);
                    return false;
                }
                if (IsHungAppWindow(w))
                    return false;
                SetLastError(0);
                auto result = SetWindowLongPtrW(w, GWL_STYLE, i->style);
                if (!result && GetLastError())
                    return false;
                SetWindowLongPtrW(w, GWL_EXSTYLE, i->exstyle);
                SetWindowPlacement(w, &i->placement);
                SetWindowPos(w, nullptr, i->rect.left, i->rect.top, i->rect.right - i->rect.left,
                             i->rect.bottom - i->rect.top,
                             SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
                RemovePropW(w, markerName.c_str());
                saved.erase(i);
                return true;
            }
        return false;
    }
    void restoreAll() {
        while (!saved.empty()) {
            HWND w = saved.back().window;
            size_t count = saved.size();
            restore(w);
            if (saved.size() == count)
                saved.pop_back();
        }
    }
    void restoreBlocked(const Rules& rules) {
        auto current = processes();
        std::vector<HWND> pending;
        for (auto& entry : saved)
            if (!current.count(entry.pid) || rules.blocked(current[entry.pid]))
                pending.push_back(entry.window);
        for (auto window : pending)
            restore(window);
    }
};
} // namespace fg
