# Framoryx 0.3 audit additions

The shared desktop, Linux integration and bundled graphics runtime were added to the audit. The expanded C++ scan initially found 25 upstream findings. Source changes removed unused upload/shell helpers, fixed formatting and parsing, bounded font allocation arithmetic and used reentrant time conversion. The [expanded a8669f1 scan](https://github.com/NeTriXoN-BoT/framoryx/actions/runs/42513798080) completed with zero findings across its Windows, Linux and native analyses, without exclusions or dismissals. The final release commit is scanned again.

Integration checks found and corrected saved-game restoration order, out-of-order volume writes, malformed-preference replacement, startup command limits, quiet-start behavior, older PipeWire graph ordering/routing, duplicate Vulkan layer activation and running-game rule enforcement. The Linux test microphone now verifies its synthesized signal before testing effects. The final review also corrected X11 automatic capture when no previous capture connection exists. A private native Wayland session checks UI, game layers and HUD geometry. Failed or incomplete runs remain visible and are not treated as passes.

Default monitoring loads no game layer. Explicit game-layer launches wait for pending compatibility inspection, known indicators select external monitoring, and loaded code still requires a game restart to remove fully. Audio remains opt-in, microphone samples stay in memory, and downloaded plugins are verified before use.

Native Wayland testing also found a runtime cleanup fault on desktops without a keyboard/seat object. A captured native backtrace identified an unconditional proxy destroy. Cleanup now checks object existence, removes surface references once and erases display entries safely. Framoryx mode skips the unused game-keyboard listener entirely. The live suite verifies graceful Vulkan scene exit as well as forced fixture cleanup. Its software-rendered Qt FIFO scene cannot reach 60 FPS even without a layer, so Wayland Vulkan exercises distinct 20/30 FPS targets; OpenGL and X11 keep their 30/60 FPS checks.

See [feature verification](FEATURE-CHECKS.md) for current integration scope and [runtime sources](RUNTIME-SOURCES.md) for dependencies and licenses. No universal game, ban-free, zero-overhead or physical-device certification is claimed.

## Earlier audit records

# 0.2.3 feature audit

Reviewed on 8 November 2026. Scope: capture and overlay state, optional renderer/limiter paths, UI/profile/scaling controls, borderless/rules, audio workers and updater, CSV export, lifecycle, packaging and public downloads. The [feature verification matrix](FEATURE-CHECKS.md) records the scope and limits.

Fixes include accepted audio requests lost on shutdown, stale deferred mic Apply, rapid-edit queue saturation, inconsistent updater completion reads, interrupted slider commits, stale percentage/FPS-limit state, misleading capture/attach success state, and ambiguous CSV formatting. Content-only page snapshots reduce temporary bitmap storage. Visual review also corrected the compatibility selector card, contextual audio guidance and sizing explanation. The unused split-result updater API was removed so consumers use a synchronized completion snapshot.

The fresh native security-extended [0.2.3 scan](https://github.com/NeTriXoN-BoT/Framoryx/actions/runs/42489818861) reported **0 alerts across 95 rules**, without exclusions or dismissals. Scanning remains a release gate for the final changes. Local ASan/UBSan runs passed all headless suites, including 146 new flow checks; compiler warning and static-analyzer checks of the reviewed app/updater code were clean. Full-history secret scanning remains enabled.

All app/game UI execution for this audit takes place on isolated CI Windows machines. No local app windows, system audio writes, driver installation or security-setting changes are needed. This remains an engineering audit, not universal game, acoustic or security certification. The unsigned distribution and earlier incomplete Defender-blocked injection soak are disclosed in the compatibility/validation records.

## Earlier 0.2.2 audit record

Reviewed on 8 November 2026. The **20 alerts remaining in 0.2.1 are cleared** by source changes. The fresh full native CodeQL security-extended analysis reported **0 alerts**, without query exclusions, vendor filters, custom suppression models or alert dismissals. The first clean scan is [recorded here](https://github.com/NeTriXoN-BoT/Framoryx/actions/runs/42484741699).

The changes cover ten formatting alerts, three font allocation alerts, three memory-layout alerts, one slider arithmetic alert, two compiler CET initialization alerts and one stream-offset arithmetic alert. See the [dependency patch record](DEPENDENCY-PATCHES.md) for the exact changes, supported format profile, attribution and checksum-guarded toolchain patches.

The headless dependency suite includes **42 checks** for signed/unsigned edge cases, rejected and mismatched formats, literal-pointer selection, truncation, high-precision values, large slider ranges, font atlases at three sizes, concave geometry and CPU draw data. The x64 address/undefined-behavior sanitizer run passed all 42. Native x64/x86 and portable Linux builds include the same suite.

The toolchain patch was applied and reapplied successfully; a deliberately altered header fixture was rejected without changing either test header. Input and output checksums are validated before replacement.

Graphics and window tests run on an isolated GitHub Windows runner. No local application or graphics-scene windows need to open. The initial remote DX12 throughput check compared an unrendered baseline with a customized HUD; its workload comparison was corrected, while the earlier failed runs remain visible in CI history. A failed/incomplete run is not counted as a pass.

This is an engineering audit, not a proof that the program is free of vulnerabilities. Framoryx remains unsigned, and the existing hook, driver/game, audio-device and commercial-game coverage limits still apply. The earlier Defender-blocked injection stress test remains incomplete; no protections or exclusions were changed.

## Earlier 0.2.1 record

Reviewed on 8 November 2026. Scope: Windows application, optional DX11/DX12 and x86 hook paths, broker, capture lifetime, audio configuration/updater, packaging, workflows and public downloads. This is an engineering review with automated checks, not an independent security certification.

## Changes made

| Finding | Resolution |
| --- | --- |
| Resizing could replace the requested desktop percentage; scaled coordinates were inconsistent when centered | A single DPI/work-area-aware viewport positions painting and native controls; maximize preserves the percentage and restore remains aligned |
| Recreating control fonts during unchanged layout work | Rebuild only when the actual font pixel size changes |
| Static pages and animation frames copied more pixels than necessary | Cached static bodies, changed-state invalidation and bounded animation blits |
| Gain, bass and microphone writes could lose another effect's root include | Serialize complete transactions within the app, not just individual writes |
| Predictable temporary-file replacement and unbounded local reads | Create new temporary files exclusively, flush and atomically replace; bound reads and reject reparse leaf files |
| Config overrides could name remote or ambiguous paths | Require absolute local-drive paths; reject UNC/mapped network drives, alternate streams and control characters |
| Cached plugin validation only checked PE shape | Verify file hashes on cache reuse and again when staging for Apply; known v1.10 files have compiled expected hashes |
| APO config referenced a movable download cache | Copy verified bytes into a content-addressed folder beside the selected APO config; preserve existing installed versions |
| Mic/bass file operations could stall window message handling | Bounded background work queue; caught startup/task failures and visible results |
| Malformed or deeply nested updater metadata | Size/type/time/version/host checks and a nesting limit |
| A capture reader failure could leave its collector/session alive | Stop the owned collector and unique ETW session on failure and normal shutdown |
| Voice preset label did not match restored/custom band values | Derive the label from saved gains; add Bassy voice and a Custom state |
| Probe power multiplication occurred in float precision | Widen samples before multiplication and reject nonfinite/excessive output |
| Hidden legacy overlay control and small unnecessary allocations | Removed the unused control/handler and reserve bounded graph/statistics storage |

Audio configuration permissions remain those of the selected APO installation. Framoryx does not change ACLs, elevate silently or install a driver. Transaction locking coordinates this app's workers; it does not lock out another editor or a different user session. Review existing filters before combining boosts.

## Automated review

- **Clang 23 analyzer/clang-tidy:** all 15 current C++ translation units compiled without analysis errors. The broad ruleset produced 163 distinct diagnostics, including test code and repeated API idioms. These were reviewed by category: Win32 flag/HMENU conversions, bounded graphics arithmetic, deterministic test seeds, supported `#pragma once`, optional fixture accesses, exception policy and style suggestions. The analyzer's only reported analyzer-specific item was padding in the one global app object. This is not a zero-warning claim.
- **CodeQL security-extended:** a full native Windows build of the application, hooks, broker and helpers. The baseline produced **41 alerts**. The updated scan cleared all 19 application path-flow alerts and both probe arithmetic alerts, leaving **20 alerts in vendored libraries/toolchain headers**. None were hidden with broad exclusions or bulk dismissals. Categories and reachability review are below. Scans remain enabled on relevant pushes and pull requests, including vendored code.
- **Gitleaks 8.30.1:** full public Git history, with redacted output. One baseline match was an upstream ImGui keyboard-enum comparison, not a credential. Only that exact commit/file/rule/line fingerprint is ignored; no directory or general rule is excluded. The reviewed scan found no remaining secret matches. CI scans full history on pushes and pull requests.
- **Runtime sanitizers:** core, malformed-input, gain, microphone/bass and file-integrity/concurrent-audio tests passed with address and undefined-behavior sanitizers. Windows LeakSanitizer is not enabled; resource observations are separate. Game hooks were tested with their normal graphics regression suite, not an ASan-instrumented commercial game.

### CodeQL baseline disposition

| Alerts | Category | Review |
| --- | --- | --- |
| 19 | Local configuration path flow | Paths come from the installed APO registry location or an explicit local folder selection. Added local-path validation, bounded regular-file reads, exclusive temporary creation and atomic replacement. The chosen folder remains an authorized destination; it is not a web-supplied filename. Flow warnings may remain because generic analysis does not model this selection policy. |
| 2 | Probe power arithmetic | Widen multiplication operands before calculating RMS; regression/probe checks retained. |
| 10 | ImGui nonconstant format helpers | Library APIs accept a format argument by design. Framoryx supplies internal format literals; external text uses unformatted drawing or `%s` arguments. No user-controlled format string was found. |
| 3 | MinHook/ImGui typed arena arithmetic | MinHook reserves a complete 64-byte slot after its header within a 4096-byte page. ImGui partitions a size-calculated scratch arena into nodes and pointer arrays. These offsets are intentional; HUD shapes/counts are internally generated. |
| 5 | Library/font/stream integer multiplication | Reviewed against fixed bundled fonts, clamped HUD sizes, bounded file/config input and internally generated widget data. Untrusted fonts or arbitrary ImGui documents are not accepted. These remain dependency maintenance concerns; no general library-wide safety proof is claimed. |
| 2 | Compiler CET intrinsic output | Inline-assembly output operands are initialized by the instruction; the analyzer does not model that output. Toolchain headers were not edited. |

The ImGui zero-length text-replacement temporary is initialized locally to remove a compiler warning; this small vendor change is documented here. Upstream ImGui 1.93.0 is newer than the pinned 1.91.9b used by the tested hook. Its font/backend migration is deferred until separate renderer validation, rather than introducing that change into this maintenance release.

## Dependencies and trust

Reviewed the published advisory endpoints for [MinHook](https://github.com/TsudaKageyu/minhook/security), [miniz](https://github.com/richgel999/miniz/security), [nlohmann/json](https://github.com/nlohmann/json/security), [ImGui](https://github.com/ocornut/imgui/security), [PresentMon](https://github.com/GameTechDev/PresentMon/security) and [RNNoise VST](https://github.com/werman/noise-suppression-for-voice/security). Those endpoints returned no published repository advisories at review time. This is narrower than proving that every dependency has no vulnerability.

Compiler and monitoring downloads are version/SHA-256 pinned. GitHub Actions are pinned to commit IDs. RNNoise uses the latest compatible official stable release, HTTPS host restrictions, archive/file bounds, available upstream hashes, a pinned v1.10 archive and an isolated offline audio probe. A hash from a writable metadata file alone does not establish publisher identity. Protect installation/configuration folders from unwanted local modification.

## Limits retained

Framoryx is unsigned. Optional game hooks can conflict with games and security tools; no anti-cheat approval or ban-free guarantee exists. The earlier Defender-blocked injection soak remains incomplete and is not counted as a pass. No protections or exclusions were changed. Commercial-game, driver, HDR and audible-device coverage remain limited. The [validation record](VALIDATION.md) identifies the completed runtime checks. Linux core CI is not a Linux desktop application release.
