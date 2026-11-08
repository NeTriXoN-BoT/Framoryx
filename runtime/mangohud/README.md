# Linux game runtime

The bundled x86-64 runtime builds MangoHud v0.8.4 at commit `992103e4fb744897826de04ea00a2f71e7018214`. Upstream and dependency licenses remain in the runtime package.

`framoryx.patch` adds three commands to its existing local control connection: bounded frame-time retrieval, stopping that stream, and reloading the saved overlay configuration. Both endpoints check the peer's user ID; the desktop also checks the socket's process ID. Recording uses a fixed 1,024-sample buffer, never allocates or waits on the render thread, and reports overwritten samples. A disconnected desktop stops collection. Normal monitoring does not write per-frame CSV files.

The patch reuses the control listener during configuration reloads and leaves the upstream Vulkan/OpenGL integration and frame limiter in place. Framoryx supplies its own HUD drawing with the shared layout geometry: all five layouts, sixteen independently selectable readouts, ordering, ten accents, font/percentage scaling and transparent backgrounds. Hardware values come from the desktop's sensor worker; the runtime avoids starting a duplicate hardware-polling thread in this mode. Packet parsing is bounded and tested separately.

The runtime is enabled only for explicitly selected game-layer launches. The default external monitoring path loads no game layer. A missing controller leaves unavailable statistics blank instead of retaining stale sensor values.

Its ImGui dependency uses the same pinned 1.91.9b source and maintained bounds/format hardening as the Windows HUD. The upstream archive is checksum verified; the reviewed source files and `scalar_formats.hpp` are copied by the build script. See [dependency patches](../../docs/DEPENDENCY-PATCHES.md).

Build with `bash scripts/build-mangohud.sh` after installing the dependencies listed in the shared desktop workflow. The launcher relocates with the package and preserves other configured layers. This package is x86-64; 32-bit games need matching host MangoHud libraries. Steam Flatpak and other sandboxes need access to a compatible layer inside their runtime.
