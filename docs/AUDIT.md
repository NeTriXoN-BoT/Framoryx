# 0.2.1 audit record

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
- **CodeQL security-extended:** a full native Windows build of the application, hooks, broker and helpers. The baseline produced **41 alerts**; the categories and reachability review are below. Scans remain enabled on relevant pushes and pull requests, including vendored code.
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
