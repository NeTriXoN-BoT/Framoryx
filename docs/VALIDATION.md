# Version 0.2 validation

The local Windows validation completed:

- **32 core checks** covering parsing, rules, rolling statistics, memory budgets, scale bounds and motion timing.
- **20 isolated gain-configuration checks**, with no system audio changes.
- **38 microphone/bass configuration checks**, including channel/rate restrictions, EQ headroom, per-endpoint scope, backups and safe disable.
- Official RNNoise v1.10 mono and stereo VST2 binaries accepted all three configured voice thresholds and processed 480,000 finite offline samples each. This is a DSP/state check, not a listening-quality certification.
- **67 graphics/lifecycle checks** against bundled x64 DX11, x64 DX12 and x86 DX11 scenes.
- **Four UI/profile/scale checks**, including saved-layout round-trip through the actual controls, smaller/larger desktop percentages and Auto restoration.
- A **120-second non-injecting ETW/borderless soak**: 391 steady observations, no missing telemetry, seven window transitions, 36,864 bytes of post-warm-up collector growth and zero handle growth.

The graphics checks covered all three HUD layouts, selected readouts, live size/opacity/accent changes, font-atlas rebuilding, frame-ring integrity, 30/60/uncapped pacing, shader preservation, resizing, clean disable, heartbeat loss and conservative multi-queue DX12 fallback.

The UI animation check targeted the detected 144 Hz display, completed seven navigation transitions, exercised three window sizes and received real demo frames. Average paint time was approximately 1.4 ms; initial/layout/cache work caused higher peaks. This is a diagnostic workload, not a universal frame-presentation or idle-power benchmark.

## Interrupted test

Defender quarantined the optional injection soak helper. That run ended before completion and is **not a pass**. The validator now requires an explicit completion marker as well as a successful exit status. The standard soak path exercises ETW and window controls without injection. No security protections were bypassed or changed.

## What these results do not establish

Commercial games, every driver/display configuration, HDR behavior and anti-cheat approval have not been certified. No audible gain test on every output device or universal Linux application test was performed. Refer to [compatibility](COMPATIBILITY.md) and the [Linux plan](LINUX.md).

CI separately builds the native Windows package and runs the portable core on Linux. A passing Linux core job does not constitute a Linux GUI release.
