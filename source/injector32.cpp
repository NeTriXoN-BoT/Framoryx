#include "platform.hpp"
#include <shellapi.h>

// Fixed-purpose architecture bridge. No arbitrary DLL path, elevation, persistence or stealth.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int count = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    DWORD pid = 0;
    if (args && count == 3 && std::wstring(args[1]) == L"--pid") {
        wchar_t* end = nullptr;
        auto parsed = wcstoul(args[2], &end, 10);
        if (end && !*end && parsed > 0)
            pid = parsed;
    }
    if (args)
        LocalFree(args);
    if (!pid || pid == GetCurrentProcessId())
        return 1;
    fg::Handle mapping(OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, fg::mappingName(pid).c_str()));
    if (!mapping)
        return 2;
    auto ipc = static_cast<fg::HookControl*>(
        MapViewOfFile(mapping.h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(fg::HookControl)));
    if (!ipc)
        return 3;
    struct View {
        fg::HookControl* p;
        ~View() {
            UnmapViewOfFile(p);
        }
    } view{ipc};
    auto fail = [&](int code) {
        InterlockedExchange(&ipc->request, 1);
        InterlockedExchange(&ipc->status, -code);
        return code;
    };
    if (ipc->magic != fg::ipcMagic || ipc->version != fg::ipcVersion || ipc->targetPid != pid ||
        !ipc->ownerPid || fg::read32(&ipc->request) || fg::read32(&ipc->status) != 0 ||
        GetTickCount64() - ULONGLONG(fg::read64(&ipc->heartbeat)) > 3500)
        return fail(4);
    fg::Handle owner(OpenProcess(SYNCHRONIZE, FALSE, ipc->ownerPid));
    if (!owner || WaitForSingleObject(owner.h, 0) != WAIT_TIMEOUT)
        return fail(5);
    auto processes = fg::processes();
    if (!processes.count(pid) || fg::Rules{}.blocked(processes[pid]) ||
        !fg::antiCheatRunning().empty())
        return fail(6);
    fg::Handle target(OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ |
                                      SYNCHRONIZE,
                                  FALSE, pid));
    USHORT machine = 0, native = 0;
    if (!target || fg::creationTime(target.h) != ipc->creationTime ||
        !IsWow64Process2(target.h, &machine, &native) || machine != IMAGE_FILE_MACHINE_I386 ||
        native != IMAGE_FILE_MACHINE_AMD64)
        return fail(7);
    auto tools = fg::executableDir();
    auto root = tools.substr(0, tools.find_last_of(L"\\/"));
    auto dll = root + L"\\FramoryxHook32.dll";
    if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES)
        return fail(8);
    auto loader = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HMODULE localModule = nullptr;
    if (!loader || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       reinterpret_cast<LPCWSTR>(loader), &localModule))
        return fail(9);
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(localModule, modulePath, MAX_PATH);
    std::wstring moduleName = modulePath;
    moduleName = moduleName.substr(moduleName.find_last_of(L"\\/") + 1);
    uintptr_t remoteBase = 0;
    fg::Handle modules(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid));
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (modules && Module32FirstW(modules.h, &entry))
        do {
            if (_wcsicmp(entry.szModule, moduleName.c_str()) == 0)
                remoteBase = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
        } while (Module32NextW(modules.h, &entry));
    if (!remoteBase || fg::read32(&ipc->request))
        return fail(10);
    SIZE_T bytes = (dll.size() + 1) * sizeof(wchar_t), written = 0;
    void* remote =
        VirtualAllocEx(target.h, nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!remote)
        return fail(11);
    if (!WriteProcessMemory(target.h, remote, dll.c_str(), bytes, &written) || written != bytes) {
        VirtualFreeEx(target.h, remote, 0, MEM_RELEASE);
        return fail(12);
    }
    auto start =
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteBase + reinterpret_cast<uintptr_t>(loader) -
                                                 reinterpret_cast<uintptr_t>(localModule));
    fg::Handle thread(CreateRemoteThread(target.h, nullptr, 0, start, remote, 0, nullptr));
    if (!thread) {
        VirtualFreeEx(target.h, remote, 0, MEM_RELEASE);
        return fail(13);
    }
    if (WaitForSingleObject(thread.h, 5000) != WAIT_OBJECT_0) {
        // The loader may still read the path: retain that allocation until the target exits.
        return fail(14);
    }
    DWORD loaded = 0;
    GetExitCodeThread(thread.h, &loaded);
    VirtualFreeEx(target.h, remote, 0, MEM_RELEASE);
    return loaded ? 0 : fail(15);
}
