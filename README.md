<div align="center">
  <img src="docs/images/hero.png" alt="Framoryx — Every frame. Under control." width="100%">
  <p>A native Windows toolkit for performance overlays, frame pacing, borderless play, and audio control.</p>
  <a href="https://github.com/NeTriXoN-BoT/Framoryx/releases/latest/download/Framoryx-Windows-x64.zip"><img src="docs/badges/download-windows.svg" alt="Download Framoryx for Windows x64" height="94"></a>
  &nbsp;
  <a href="https://github.com/NeTriXoN-BoT/Framoryx/releases/latest"><img src="docs/badges/releases.svg" alt="Latest release notes and checksums" height="94"></a>
  &nbsp;
  <a href="docs/LINUX.md"><img src="docs/badges/linux.svg" alt="Linux roadmap — no Linux application release yet" height="94"></a>
  <p><sub><strong>UNSIGNED WINDOWS BUILD</strong> · No Authenticode publisher certificate.<br>Windows may display an unknown-publisher or SmartScreen warning. Check the release SHA-256 before use.</sub></p>
</div>

## Your game. Your overlay.

Keep an eye on real frame data, make the HUD your own, and get back to playing. Framoryx uses Windows ETW events through PresentMon for its default monitoring mode. In-game hooks are a separate, explicit choice.

| Measure | Customize | Take control |
| --- | --- | --- |
| Live FPS, average FPS, 1% lows and frame times | Glass, Compact and Telemetry Bar layouts | Optional DX11/DX12 frame limiter |
| Rolling frame graph and CSV export | Ten independently selectable readouts | Borderless play with window restoration |
| System CPU and memory readouts | Per-game style, accent, opacity and corner | Output volume, compensated bass, mic EQ and optional RNNoise |
| Automatic desktop HUD fallback | Automatic sizing and percentage overrides | Tray controls, hotkeys and desktop shortcuts |

<img src="docs/images/studio-glass.png" alt="Overlay Studio with the Glass layout and clearly labelled sample preview values" width="100%">

<details>
<summary>Compare Compact and Telemetry Bar</summary>

**Compact** keeps the same selectable information in a smaller footprint.

![Compact layout in Overlay Studio](docs/images/studio-compact.png)

**Telemetry Bar** arranges the selected readouts horizontally.

![Telemetry Bar in Overlay Studio](docs/images/studio-telemetry.png)

</details>

<details>
<summary>Frame limiter and process rules</summary>

![Frame limiter with a rounded FPS stepper](docs/images/limiter.png)

![Process rules with matching cards and inline validation](docs/images/rules.png)

</details>

## Get started

1. Download the Windows ZIP and extract the whole folder.
2. Open **Framoryx.exe**. Open your game, then choose its window or leave **Auto** selected.
3. Click **Show overlay**. Capture starts automatically.

Open **Overlay Studio** to choose a layout, readouts, size, colors and opacity. The Studio preview labels sample values when a game is not reporting data. **Desktop app size → Auto** adapts to your resolution, DPI and work area; percentages give you manual control. Maximizing preserves your selected size and centers the design with consistent proportions. **Create shortcut** adds a desktop link to the extracted copy.

Keep the application folder together. No account, installer or background service is required for normal monitoring.

**Use at your own risk.** Games with anti-cheat may restrict or penalize overlays and in-game hooks. You are responsible for checking and following each game's rules. Framoryx does not guarantee anti-cheat compatibility. Its maintainers accept no responsibility for bans, suspensions, lost access or other consequences arising from its use.

## Compatibility, clearly stated

- **Windows 10/11 x64** is the application target. Optional hooks include x64 DX11/DX12 and x86 DX11 support.
- The default **ETW + desktop overlay** path does not inject a DLL into the game.
- The optional in-game HUD/limiter requires a specific process, allow-list approval and an explicit confirmation. Block rules take priority.
- DX12 drawing uses an observed direct queue. Ambiguous queue configurations keep the limiter available and fall back to the desktop HUD in Auto mode.
- Vulkan, OpenGL and DX9 use the desktop monitoring path. Exclusive fullscreen can cover desktop overlays; the game's own borderless mode often provides the best starting point.
- Bass, microphone EQ and extra gain require a separately configured **Equalizer APO**. RNNoise downloads the official compatible x64 VST2 plugin automatically and needs a 48 kHz microphone. Normal output-volume control works immediately. See [audio setup](docs/AUDIO.md).

**In-game hooks remain experimental.** They can conflict with games, overlays or security tools, and no ban-free guarantee is possible. Microsoft Defender quarantined the optional injection stress-test helper during development. That interrupted run is not counted as a pass, and Framoryx does not bypass security protections. See [compatibility and security notes](docs/COMPATIBILITY.md) before enabling hooks.

## Build and verify

From PowerShell in the repository root:

```powershell
./source/build.ps1
```

The build script downloads a pinned, SHA-256-verified portable LLVM-MinGW toolchain when no compiler path is supplied. It builds the app and its test scenes, then runs deterministic core and isolated audio-configuration checks.

```powershell
# Reuse a toolchain and build directory
./source/build.ps1 -ToolchainBin C:\llvm-mingw\bin -BuildDirectory C:\scratch\framoryx

# Non-injecting ETW/borderless soak
./source/validate.ps1 -ToolchainBin C:\llvm-mingw\bin -SoakSeconds 120

# Explicit local graphics checks against bundled test scenes only
./source/validate.ps1 -ToolchainBin C:\llvm-mingw\bin -IncludeGraphicsTests
```

See [building and testing](docs/BUILDING.md) for details. The Linux CI job checks the portable core and headless dependency regressions; it does **not** build a Linux desktop application.

## Project status

Version **0.2.3** improves audio request handling, slider commits, capture error reporting, CSV exports and page-transition memory use. Full control flows, native UI previews and graphics/window checks run on an isolated CI runner. The toolkit includes Bassy voice, microphone processing, Overlay Studio, three HUD layouts, per-game profiles, DPI/percentage scaling, frame limiting, borderless play and tray access.

Bundled test scenes establish behavior on the tested configuration; they are not a commercial-game compatibility certification. [Feature checks](docs/FEATURE-CHECKS.md), [validation](docs/VALIDATION.md), the [audit record](docs/AUDIT.md) and [release notes](docs/releases/v0.2.3.md) describe completed checks and current limits. A native Linux application is [planned separately](docs/LINUX.md).

## Contribute

Please use the [bug-report template](.github/ISSUE_TEMPLATE/bug.yml) for reproducible issues and the [game-compatibility template](.github/ISSUE_TEMPLATE/game.yml) for renderer/window-mode reports. Include only relevant, sanitized diagnostics.

Original project code is available under the [MIT license](LICENSE). Third-party components retain their own licenses; see [notices](THIRD-PARTY-NOTICES.txt).
