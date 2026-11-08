#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <cstddef>
#include "presentation.hpp"

namespace fg {
constexpr LONG ipcMagic = 0x46475032;
constexpr size_t hookRingSize = 8192;
struct alignas(8) HookFrame {
    volatile LONG64 sequence, qpc, interval;
};
// All cross-process fields use Win32 Interlocked operations, not C++ object synchronization.
struct alignas(8) HookControl {
    LONG magic, version;
    DWORD ownerPid, targetPid;
    ULONGLONG creationTime;
    volatile LONG cap, request, status; // request: 0 enabled, 1 disable; status: 0 init, 1 active,
                                        // 2 disabled, negative failure
    volatile LONG overlay, renderer, corner;
    volatile LONG64 heartbeat, frames, lastPresentQpc;
    volatile LONG64 lastHudQpc;
    HookFrame ring[hookRingSize];
    volatile LONG hudFields, hudScale, hudOpacity, hudAccent, cpuHundredths, ramMiB, hudStyle;
    volatile LONG hudOrderLo, hudOrderHi, hudX, hudY, hudFontScale;
};
static_assert(sizeof(HookFrame) == 24);
static_assert(offsetof(HookControl, ring) == 80);
static_assert(sizeof(HookControl) == 196736);
inline std::wstring mappingName(DWORD pid) {
    return L"Local\\Framoryx.Control." + std::to_wstring(pid);
}
inline LONGLONG read64(volatile LONG64* p) {
    return InterlockedCompareExchange64(p, 0, 0);
}
inline LONG read32(volatile LONG* p) {
    return InterlockedCompareExchange(p, 0, 0);
}
inline ULONGLONG creationTime(HANDLE process) {
    FILETIME c{}, e{}, k{}, u{};
    if (!GetProcessTimes(process, &c, &e, &k, &u))
        return 0;
    return (ULONGLONG(c.dwHighDateTime) << 32) | c.dwLowDateTime;
}
} // namespace fg
