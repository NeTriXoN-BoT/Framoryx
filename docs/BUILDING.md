# Building and testing

The 0.3 desktop uses C++20 and Qt Quick on Windows and Linux. Its Windows monitoring engine is built separately and runs privately behind the shared interface. The public stable release remains 0.2.3 until the 0.3 release checks pass.

## Shared desktop

Install CMake, a C++20 compiler, and Qt 6.8.3 with Core, Gui, Qml, Quick, QuickControls2, Network, Multimedia, Widgets and Concurrent. Set `CMAKE_PREFIX_PATH` to the matching Qt installation. Qt Test is needed only when building the optional automated input driver.

```sh
cmake -S . -B build/desktop -DFRAMORYX_BUILD_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop --config Release --parallel 3
ctest --test-dir build/desktop -C Release --output-on-failure
```

Model checks use an isolated preferences folder and an offscreen Qt platform. They do not record audio, change devices, register startup entries or open games. Live UI, game, audio and packaging checks run in the isolated GitHub Actions environments defined in [desktop.yml](../.github/workflows/desktop.yml).

On Linux, install the XCB Present development packages and graphics/audio development dependencies listed in that workflow. Build the pinned Vulkan/OpenGL runtime before packaging:

```sh
bash scripts/build-mangohud.sh
bash scripts/package-linux.sh
```

The runtime build verifies its upstream revision and dependency archives. [The integration sources](../runtime/mangohud) implement the local control stream and Framoryx HUD. The initial portable target is x86-64 with glibc 2.35 or newer. See [Linux support](LINUX.md) for display, architecture and sandbox limits.

## Windows monitoring engine

Use the MSVC 2022 x64 Qt kit for the shared desktop. Build the separate native engine and helpers with PowerShell:

```powershell
./scripts/bootstrap-tools.ps1
./source/build.ps1 -BuildDirectory ./work/engine-build -OutputDirectory ./work/qt-engine
```

`source/build.ps1` builds the monitoring engine, hooks and diagnostic scenes. Its legacy native window is an integration harness; the release interface comes from `desktop/`. Stage the engine as `FramoryxEngine.exe` beside the Qt executable, together with the hook DLLs and `tools` directory, as shown in the desktop workflow.

```powershell
./scripts/package-desktop.ps1
```

The package script includes the shared desktop, private engine, required Qt plugins and licensed runtime redistributables. The deployed package is tested with Qt build paths removed. The output is unsigned; no publisher certificate is generated or implied.

The engine build downloads a pinned, checksum-verified LLVM-MinGW toolchain when no `-ToolchainBin` is supplied. PresentMon is separately pinned and verified. No global compiler is installed or account/group membership changed. Toolchain header fixes are context and checksum checked; see [dependency patches](DEPENDENCY-PATCHES.md).

## Focused verification

Native suites cover frame statistics, settings validation, malformed inputs, audio configuration transactions, worker cancellation, dependency regressions, exports and verified plugin staging. Headless ImGui checks create CPU draw data and font atlases in x64 and x86 builds.

```powershell
# Explicit live checks against bundled diagnostic scenes
./source/validate.ps1 -ToolchainBin C:/llvm-mingw/bin -IncludeGraphicsTests

# Address/undefined-behavior sanitizers
./scripts/check-sanitizers.ps1 -ToolchainBin C:/llvm-mingw/bin
```

Live checks can create windows and load the optional hook into Framoryx's own scenes. They are separate from ordinary startup. Do not run them in an active gaming session. The older extended hook stress helper is a separate opt-in; interrupted or security-blocked runs are never counted as passes and security protections must remain enabled.

Shared desktop tests exercise cached page navigation, rapid slider gestures, Auto/manual scaling, maximization, backend capture, reversible window changes, live limits and audio devices in a private PipeWire session. Vulkan/OpenGL use real rendered scenes. These checks do not certify every game, GPU or microphone.

## Distribution and release checks

Packaging excludes preferences, captures, local logs, toolchains and development caches. Each download receives a SHA-256 file; clean source archives come from Git. See [runtime sources and licenses](RUNTIME-SOURCES.md).

The draft-release workflow requires successful desktop, core, secret and security checks for the exact source commit and no open CodeQL alerts. Published download checks use anonymous `releases/latest` routes and compare public asset bytes with their checksums. Old releases and tags are pruned only after the new published downloads have been verified.
