#pragma once
#include "platform.hpp"

namespace fg {
struct Compatibility {
    DWORD pid = 0;
    std::wstring architecture = L"Unknown", runtime = L"Waiting for a game",
                 window = L"No window selected";
    std::wstring note = L"Select a game to inspect its display and available overlay paths.";
    std::wstring otherOverlays;
    bool dxgi = false, vulkan = false, opengl = false, dx9 = false, inspected = false;
};
inline Compatibility inspectCompatibility(HWND window, const std::string& exe) {
    Compatibility result;
    if (!window || !IsWindow(window))
        return result;
    GetWindowThreadProcessId(window, &result.pid);
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, result.pid));
    USHORT machine = 0, native = 0;
    if (process && IsWow64Process2(process.h, &machine, &native))
        result.architecture = machine == IMAGE_FILE_MACHINE_I386   ? L"32-bit"
                              : native == IMAGE_FILE_MACHINE_AMD64 ? L"64-bit"
                                                                   : L"Other architecture";
    RECT rect{};
    GetWindowRect(window, &rect);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
    result.window = IsIconic(window)                       ? L"Minimized"
                    : EqualRect(&rect, &monitor.rcMonitor) ? L"Fills display"
                                                           : L"Windowed";
    Handle modules(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, result.pid));
    MODULEENTRY32W module{};
    module.dwSize = sizeof(module);
    bool dx12 = false, dx11 = false;
    if (modules && Module32FirstW(modules.h, &module)) {
        result.inspected = true;
        do {
            auto name = lower(utf8(module.szModule));
            dx12 |= name == "d3d12.dll";
            dx11 |= name == "d3d11.dll";
            result.dxgi |= name == "dxgi.dll";
            result.vulkan |= name == "vulkan-1.dll";
            result.opengl |= name == "opengl32.dll";
            result.dx9 |= name == "d3d9.dll";
            if (matches({"rtsshooks*.dll", "discordhook*.dll", "gameoverlayrenderer*.dll",
                         "nvspcap*.dll", "specialk*.dll", "owclient*.dll"},
                        name)) {
                if (!result.otherOverlays.empty())
                    result.otherOverlays += L", ";
                result.otherOverlays += module.szModule;
            }
        } while (Module32NextW(modules.h, &module));
    }
    result.runtime = dx12            ? L"DX12 runtime present"
                     : dx11          ? L"DX11 runtime present"
                     : result.vulkan ? L"Vulkan runtime present"
                     : result.opengl ? L"OpenGL runtime present"
                     : result.dx9    ? L"DX9 runtime present"
                                     : L"Runtime not identified";
    if (result.vulkan)
        result.note = L"Vulkan: use the desktop overlay with the game's own borderless setting. "
                      L"Framoryx does not inject a Vulkan renderer.";
    else if (dx12 || dx11)
        result.note = L"DXGI: automatic desktop fallback is available. The optional in-game HUD "
                      L"needs explicit hook opt-in; it never attaches automatically.";
    else if (result.dx9 || result.opengl)
        result.note = L"Use the desktop overlay and borderless mode. Use the game's own FPS limit "
                      L"for this rendering API.";
    else
        result.note = L"Start capture and focus the game. Runtime modules are hints; the frame "
                      L"counter shows whether real presentation events are arriving.";
    if (!result.inspected)
        result.note = L"Windows restricted runtime inspection. ETW monitoring can still work; use "
                      L"Capture details if no frames arrive.";
    if (exe.empty())
        result.note = L"Refresh the game list to verify this window's process.";
    return result;
}
} // namespace fg
