# Linux game runtime

The bundled x86-64 runtime builds MangoHud v0.8.4 at commit `992103e4fb744897826de04ea00a2f71e7018214`. Upstream and dependency licenses remain in the runtime package.

`framoryx.patch` adds bounded frame-time retrieval, collection control, HUD/sensor updates and typed cap/font changes to the local control connection. Both endpoints check the peer's user ID; the desktop also checks the socket's process ID. Recording uses a fixed 1,024-sample buffer, never allocates or waits on the render thread, and reports overwritten samples. A disconnected controller stops collection, hides its HUD and releases its cap. Normal monitoring does not write per-frame CSV files.

The patch reuses the control listener and retains the upstream Vulkan/OpenGL integration and frame limiter. Cap changes update pacing directly. The controller supplies a supported installed font at game launch, and size changes rebuild the atlas only when needed. Framoryx supplies its own HUD drawing with shared layout geometry, including five layouts, sixteen individually selectable readouts, custom labels, ordering, ten accents, scaling and transparent backgrounds. Hardware values come from the desktop's worker, avoiding duplicate polling in the game. Packet parsing is bounded and tested separately.

The runtime is enabled only for explicitly selected game-layer launches. The default external monitoring path loads no game layer. A missing controller leaves unavailable statistics blank instead of retaining stale sensor values.

Its ImGui dependency uses the same pinned 1.91.9b source and maintained bounds/format hardening as the Windows HUD. The upstream archive is checksum verified; the reviewed source files and `scalar_formats.hpp` are copied by the build script. See [dependency patches](../../docs/DEPENDENCY-PATCHES.md).

This is a Framoryx runtime profile, not a system MangoHud replacement. The controller owns profiles and supplies launch parameters and typed updates. Generic configuration/preset file loading, arbitrary-library overrides, shell-command readouts, shell-based system probes, online log uploads and game-process file logging are removed. Reviewed parsing, font-size arithmetic, time conversion and dependency format fixes are included in the public patch/build scripts.

Build with `bash scripts/build-mangohud.sh` after installing the dependencies listed in the shared desktop workflow. The launcher relocates with the package and preserves unrelated configured layers. Its uniquely named explicit Vulkan layer does not also enable a host MangoHud implicit layer. Undocumented upstream hotkeys are disabled in the Framoryx profile so game shortcuts cannot accidentally start logging or change its configuration.

This package supports 64-bit games. A 32-bit Framoryx game layer is not bundled; use external monitoring for those games. An unmodified host MangoHud is not a replacement for Framoryx's custom control protocol. Steam Flatpak and other sandboxes need access to a compatible layer inside their runtime.
