# Version 0.3.0 verification

The [shared desktop workflow](https://github.com/NeTriXoN-BoT/framoryx/actions/workflows/desktop.yml) checks the Qt application and deployed Windows/Linux packages. X11 checks cover passive Present capture, automatic foreground selection, the external HUD with hidden controls, and reversible fullscreen. A separate private Weston session forces native Wayland with no X11 display and checks the UI plus actual OpenGL/Vulkan game-layer rendering, live limit changes, pause/resume and process blocks. HUD geometry acknowledgements distinguish drawing from frame collection. Wayland OpenGL and X11 Vulkan exercise 30/60 FPS; Wayland Vulkan uses 20/30 FPS because the software-composited Qt FIFO scene reaches about 34 FPS without any layer. Both caps must match their requested rates; an unattainable 60 FPS result is not counted as a pass. These are fixture checks, not certification of every compositor or commercial game.

Windows graphics coverage includes x64 DX11/DX12 and x86 DX11, customized readouts, font rebuilding, resize/restoration, live limits, heartbeat loss and conservative multi-queue fallback. The standard endurance check uses passive ETW capture. The separate [core workflow](https://github.com/NeTriXoN-BoT/framoryx/actions/workflows/ci.yml) also exercises audio files, updater flows and the diagnostic engine harness.

Linux audio checks use a private PipeWire session and a verified nonzero synthetic microphone signal, then exercise bass, Bassy Voice EQ, five-second recording/playback, verified official RNNoise, bypass and startup registration/removal. Input tests drag the volume slider rapidly to 200%, click all seven cached pages, preserve state, maximize and exercise automatic/percentage scaling. Packages start after removing the Qt build environment; the Linux workflow checks both display backends.

The final candidate must pass the complete build, shared desktop, security-extended CodeQL and full-history secret workflows before its packages can be promoted. Failed or incomplete runs remain visible and are not counted as passes. Fresh local address/undefined-behavior sanitizer checks passed 327,792 core assertions, 20,000 deterministic malformed-input cases, 20 audio configuration checks, 38 microphone/bass checks, 49 file/concurrency checks, 42 dependency checks and 146 export/worker/updater checks. These console checks open no app windows and change no system audio.

The app remains unsigned. Sensor availability, physical hardware, HDR, sandboxed/32-bit Linux runtimes, arbitrary compositors and anti-cheat approval retain the [documented limits](COMPATIBILITY.md). See [Linux support](LINUX.md), the [feature matrix](FEATURE-CHECKS.md) and the [audit](AUDIT.md).

## Earlier verification records

# Version 0.2.3 verification

The [feature matrix](FEATURE-CHECKS.md) maps each feature to its checks and practical limits. Native GUI/game tests run on an isolated Windows runner; local compilation and console tests open no app windows and change no system audio.

The expanded suite includes **85 control-flow assertions**, **8 audio UI checks**, **2 rule-editor checks**, **4 saved-profile/percentage checks**, **35 native layout checks**, **68 graphics/lifecycle checks**, a **180-second passive ETW/borderless soak**, **8 official RNNoise download/probe/staging checks**, and actual single-instance/shortcut validation. Twelve native page/rule previews support visual review. The [Windows CI job](https://github.com/NeTriXoN-BoT/Framoryx/actions/workflows/ci.yml) is the release gate; an incomplete or failed job is not a passing validation run.

The new **146 headless flow checks** cover CSV round trips, shutdown draining, rapid-edit coalescing, worker errors, updater completion snapshots and modified-cache rejection. The standard build includes them alongside the existing core/audio/dependency and 20,000 malformed-input cases. All these headless suites passed locally with address and undefined-behavior sanitizers. A stricter compiler review of the app, updater and new test translation units produced no diagnostics; Clang static analysis of the app/updater also produced none.

The first expanded run passed all 84 controls, profiles, layout, graphics, RNNoise and soak checks, then exposed a PowerShell null-string binding error in the new single-instance harness. That failed job remains in history and is not counted as an overall pass. The harness now supplies a native null value explicitly. The final gate must pass the actual two-process activation and shortcut checks as well.

Audio diagnostics use fake endpoints and isolated APO configuration. The official plugin test processes real offline audio through mono/stereo VST2 plugins; it does not record a microphone or reconfigure playback/capture devices. Hardware listening quality, commercial games, physical multi-monitor changes, HDR and every driver remain outside fixture coverage. The app remains unsigned. See [compatibility](COMPATIBILITY.md).

## Earlier 0.2.2 record

- The added **42-check headless dependency suite** passes under x64 address/undefined-behavior sanitizers and is part of both x64/x86 native builds and portable Linux CI. It exercises the actual patched ImGui code without creating native windows.
- Portable toolchain header fixes are checksum-guarded and idempotent; an altered-header fixture was rejected without changing either file.
- The fresh full native CodeQL security-extended scan reports **zero alerts**, clearing all 20 previously open findings without exclusions or dismissals. [Audit details](AUDIT.md).
- All seven-page window checks, graphics/limiter checks and the non-injecting capture soak execute on GitHub's isolated Windows runner. The DX12 throughput phase uses the same rendering workload as its baseline; customized HUD rendering and resize/restoration have separate checks.
- The existing 110 core, 20 gain, 38 microphone/bass, 49 file-integrity/concurrent-audio and 20,000 malformed-input checks remain in the build. Full-history secret scanning remains enabled.

## Earlier 0.2.1 record

The maintenance release completed the following Windows checks:

- **110 core checks**, including 75 viewport combinations across normal, maximized, ultrawide, smaller and 4K client sizes, three DPI levels and five percentage settings.
- **20 gain**, **38 microphone/bass** and **49 file-integrity/concurrent-audio checks** in scratch folders; no system audio changes.
- **20,000 deterministic malformed-input cases**. Core, malformed-input and all three audio suites also passed with address/undefined-behavior sanitizers.
- **67 graphics/lifecycle checks** against the bundled x64 DX11, x64 DX12 and x86 DX11 test scenes.
- **Five native layout checks** for maximize, 80%, 125%, Auto and restore, checking visible control bounds and maximized state on the local ultrawide display.
- **Eight audio UI checks**, including the live percentage, 200% maximum, Bassy voice gains and saved-preset label; **two process-rule UI checks**. Diagnostic audio mode suppresses Windows volume writes and uses a scratch APO config.
- **Four profile/percentage checks** through the real controls. The 28-second motion/profile run received 100 live demo samples, completed 13 transitions and targeted the detected 144 Hz display. It averaged 2.624 ms per paint, with a 196.617 ms peak during initial/layout/cache work while other audit workloads were running. The maximized audio run averaged 1.418 ms per paint and reused its body cache (6 full paints among 469 paints). These are diagnostic measurements, not frame-presentation or idle-power guarantees.
- A **fresh official RNNoise v1.10 download**, verified archive and isolated mono/stereo audio/preset probes; valid cached official DLLs were accepted, a modified DLL was rejected, and deeply nested metadata was rejected.
- A **120-second non-injecting ETW/borderless soak**: 391 steady observations, zero missing telemetry, seven window transitions, zero post-warm-up collector private-byte growth and zero handle growth.

CodeQL, clang-tidy and secret-scan scope/dispositions are documented in the [audit record](AUDIT.md). Public downloads are checked from an anonymous runner, including a complete ZIP/checksum comparison.

## Earlier 0.2.0 record

The local Windows validation completed:

- **32 core checks** covering parsing, rules, rolling statistics, memory budgets, scale bounds and motion timing.
- **20 isolated gain-configuration checks**, with no system audio changes.
- **38 microphone/bass configuration checks**, including channel/rate restrictions, EQ headroom, per-endpoint scope, backups and safe disable.
- Official RNNoise v1.10 mono and stereo VST2 binaries accepted all three configured voice thresholds and processed 480,000 finite offline samples each. This is a DSP/state check, not a listening-quality certification.
- **67 graphics/lifecycle checks** against bundled x64 DX11, x64 DX12 and x86 DX11 scenes.
- **Four UI/profile/scale checks**, including saved-layout round-trip through the actual controls, smaller/larger desktop percentages and Auto restoration.
- **Six audio UI checks** through the actual controls, covering live percentage state, the 200% range, voice presets and setup navigation. Windows volume writes are suppressed in this bounded diagnostic mode; any gain configuration uses an isolated scratch folder.
- **Two process-rule UI checks** for inline invalid-path rejection and valid-rule persistence.
- A successful automatic download of the latest stable upstream RNNoise release (v1.10), with archive SHA-256, mono/stereo selection and isolated audio/preset probes. Transient GitHub failures exercised retry and release-list/asset-route fallback behavior.
- A **120-second non-injecting ETW/borderless soak**: 391 steady observations, no missing telemetry, seven window transitions, 36,864 bytes of post-warm-up collector growth and zero handle growth.

The graphics checks covered all three HUD layouts, selected readouts, live size/opacity/accent changes, font-atlas rebuilding, frame-ring integrity, 30/60/uncapped pacing, shader preservation, resizing, clean disable, heartbeat loss and conservative multi-queue DX12 fallback.

The UI animation check targeted the detected 144 Hz display, completed seven navigation transitions, exercised three window sizes and received real demo frames. The latest 28-second run received 101 real demo samples and completed seven transitions, with an average paint time of approximately 1.28 ms; initial/layout/cache work caused higher peaks. This is a diagnostic workload, not a universal frame-presentation or idle-power benchmark.

## Interrupted test

Defender quarantined the optional injection soak helper. That run ended before completion and is **not a pass**. The validator now requires an explicit completion marker as well as a successful exit status. The standard soak path exercises ETW and window controls without injection. No security protections were bypassed or changed.

## What these results do not establish

Commercial games, every driver/display configuration, HDR behavior and anti-cheat approval have not been certified. No audible gain test on every output device or universal Linux application test was performed. Refer to [compatibility](COMPATIBILITY.md) and the [Linux plan](LINUX.md).

CI separately builds the native Windows package and runs the portable core on Linux. A passing Linux core job does not constitute a Linux GUI release.
