# Framoryx on Linux

The 0.3.0 development branch has a native Linux desktop with the same Qt interface, artwork, motion, scaling controls, profiles and release history as Windows. Downloads will be offered after live integration and portable-package checks pass. The public 0.2.3 release is Windows only.

## Implemented paths

| Feature | Linux implementation |
| --- | --- |
| Default capture | Read X Present events from an explicitly selected X11 game window; no game module is loaded |
| External overlay | The same customizable five-layout HUD used by the shared desktop preview; input passes through to the game |
| Vulkan/OpenGL in-game overlay and limiter | Explicitly enabled MangoHud layer, pinned and bundled for x86-64 |
| Live game-layer telemetry | Bounded local memory stream; normal monitoring does not continuously write CSV logs |
| Borderless | X11 fullscreen requests with saved process/window identity; optional Gamescope at launch |
| Output audio | PipeWire volume up to 200%, compensated bass and a selectable virtual output |
| Microphone | PipeWire three-band EQ, Bassy Voice, verified RNNoise and a selectable virtual microphone |
| Hardware | Read-only DRM/hwmon and driver metrics; missing sensors stay unavailable |
| Convenience | Tray behavior, sign-in launch, quiet startup, auto capture, game library and process rules |

## Runtime and desktop support

The initial package target is x86-64 desktop Linux built on Ubuntu 22.04, with glibc 2.35 or newer and compatible graphics drivers. AppImage and a portable tar archive carry the Qt interface libraries. A working display session, GPU driver and system audio services remain required.

X11 provides window selection and positioning. A compositor enables transparent external backgrounds; without compositing, Framoryx uses an opaque HUD surface. Some rendering paths do not expose usable X Present events. Unavailable timing is reported instead of inventing FPS or silently loading a layer.

Native Wayland restricts external positioning and monitoring of other applications. Vulkan/OpenGL layers and compositor-supported launches provide the in-game path there. Gamescope remains optional. Its MangoApp integration may be required for a Gamescope session; a normal MangoHud layer is not a universal Gamescope overlay.

32-bit games need matching host MangoHud libraries. Proton, Steam Flatpak, containers and other runtimes may need a compatible layer inside their sandbox. An x86-64 AppImage cannot support every architecture, distribution, driver or compositor universally.

PipeWire and WirePlumber must be available for audio effects. Choose **Framoryx Microphone** in voice settings or **Framoryx Output** as playback output after applying the corresponding effects. Apply does not silently change the default device.

## Verification

The isolated runner exercises X Present capture, the external HUD with the main window hidden, reversible fullscreen, private PipeWire audio, and real OpenGL/Vulkan scenes. Renderer checks verify a 30 FPS target and live limit changes; package checks run without the Qt build environment. These tests do not certify every commercial game or physical microphone/GPU.

In-game layers remain subject to each game's anti-cheat rules. Use at your own risk; a clear indicator scan is not permission or a ban-free guarantee. See [compatibility](COMPATIBILITY.md), [building](BUILDING.md), [MangoHud](https://github.com/flightlessmango/MangoHud) and [Gamescope](https://github.com/ValveSoftware/gamescope).
