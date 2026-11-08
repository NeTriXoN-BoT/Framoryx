# Version 0.2.2 validation

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
