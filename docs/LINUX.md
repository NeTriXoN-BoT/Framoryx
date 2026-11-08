# Linux portability

**A native Linux edition is feasible. It is not implemented in this release, and no Linux application download is offered.**

The shared C++ statistics, parser, rule, layout and motion code can be reused. The present Windows desktop UI, ETW capture, Win32 window controls and Core Audio integration require platform-specific replacements.

## Why one universal build is not a credible promise

AppImage improves distribution portability but still requires a suitable runtime baseline and compatible system/graphics components. Its own documentation recommends building on an older supported baseline to run on newer systems. Architecture-specific builds remain necessary. See [AppImage concepts](https://docs.appimage.org/introduction/concepts.html).

Wayland clients do not know their global surface positions and cannot access other clients' surfaces. That prevents directly reproducing unrestricted Win32 window manipulation as a universal desktop feature. Compositor support and launch-time integration matter. See the [Wayland protocol model](https://wayland.freedesktop.org/docs/book/Protocol.html).

## Proposed scope

| Area | Linux direction | Status |
| --- | --- | --- |
| Shared core | Keep C++ data, profile, layout and motion logic portable | Core test target and Linux CI provided |
| Desktop UI | Cross-platform, GPU-accelerated presentation layer | Architecture work required |
| In-game telemetry | Explicit per-game Vulkan/OpenGL integration | Research; no Linux renderer shipped |
| Window/pacing controls | X11 implementation plus compositor-aware/launch-time Wayland integration | Research |
| Audio | PipeWire/WirePlumber integration with explicit device controls | Research |
| Distribution | Test a defined baseline, then evaluate AppImage/Flatpak/native packages | No package built |

[MangoHud](https://github.com/flightlessmango/MangoHud) is a useful primary reference for Vulkan/OpenGL overlays. [Valve's Gamescope](https://github.com/ValveSoftware/gamescope) demonstrates a per-game compositor approach on X11 and Wayland. These are references and potential integration candidates, not bundled Linux dependencies or implemented Framoryx features.

## Acceptance criteria before a Linux release

- Define an explicit distribution, architecture, GPU-driver and display-server support matrix.
- Validate native Vulkan/OpenGL games and relevant Proton/DXVK/VKD3D paths.
- Test scaling, multi-monitor behavior, frame timing, audio device changes and clean game exit.
- Package reproducible builds against the selected runtime baseline.
- Preserve game/security restrictions and explicit opt-in. Linux does not remove anti-cheat or game-policy constraints.

The initial target should be a clearly tested set of mainstream desktop configurations, followed by broader coverage based on evidence.
