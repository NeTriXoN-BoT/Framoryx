# Framoryx 0.2.3 feature verification

Reviewed on 8 November 2026. Native app/game windows were exercised only on isolated GitHub Windows runners. Local checks used compilation and headless console tests. No local volume, microphone, driver or security settings were changed.

## Coverage

| Feature | Verification | Scope |
| --- | --- | --- |
| FPS, frame time, averages, 1% lows, graph | Real PresentMon events from the bundled game scene; rolling-statistics and malformed-CSV tests | Real capture plus deterministic statistics; commercial games are separate |
| CPU and memory | Real Win32 system metrics, range/readiness checks | System-wide readings |
| Capture start, stop and restart | Actual controls, retained non-live rolling data, frames after restart | Missing collector error also checked |
| Desktop overlay | Actual layered window shown on the foreground game fixture; hotkey hide/show | Exclusive fullscreen can cover this kind of window |
| Injected HUD and limiter | x64 DX11, x64 DX12 and x86 DX11 scenes; 30/60/uncapped pacing, shader state, resize, clean disable and heartbeat loss | No Vulkan/OpenGL/DX9 limiter; no commercial-game or anti-cheat certification |
| Automatic HUD fallback | Conservative multi-queue DX12 fixture, renderer/queue state checks | No automatic injection or bypass of security protections |
| Three HUD layouts and ten readouts | Actual Studio controls, every field off/on, all fields, size 50–300%, opacity, corner and accent; renderer regression suite | Native previews inspected separately |
| Per-game profiles and FPS limits | Save/reload through real controls; default reset; stale entries removed on reload | Isolated settings file |
| Desktop Auto/percentage scaler | Five native maximize/80%/125%/Auto/restore checks on each of seven pages; portable viewport combinations | No physical multi-monitor/DPI transition certification |
| Borderless and restoration | Actual window style/rectangle checks; explicit and allow-listed automatic modes; repeated resize soak | Only owned test-scene windows; some games can reject window changes |
| Process rules | Actual editor, invalid rule feedback, persistence, deny priority, immutable system blocks, restore on block | An allow list does not establish publisher approval |
| Volume slider 0–200% | Real mouse/keyboard handlers and live number state, interrupted capture, gain config readback and disable | Fake endpoints plus scratch APO config; no physical output changed |
| Output bass | Real controls, actual generated filters, headroom, coexistence and disable checks | Requires Equalizer APO enabled on playback device |
| Microphone EQ/Bassy voice | Saved preset/gain matching, all band/headroom config tests; actual EQ-only Apply/Disable and independent bass preservation | Acoustic improvement depends on the mic, voice and recording chain |
| Automatic RNNoise | Fresh official stable Windows download, mono/stereo offline DSP probes, validated cache reuse, verified installed copy, tamper rejection and disable | 48 kHz mono/stereo VST2; actual microphone routing requires APO setup |
| Audio responsiveness and exit | Coalesced rapid changes, final disable, worker exception handling, pending Apply cancellation, complete accepted writes on shutdown | Tested with isolated files, including ASan/UBSan |
| Tray and hotkeys | Global hotkey registration and command routing; tray menu commands hide/show/navigate | Taskbar icon presentation depends on Explorer; no local desktop interaction |
| Single instance and shortcut | Actual second process exits and restores the first; clean close; created shortcut target verified | Runs on isolated CI desktop |
| CSV export | Actual Export action plus UTF-8/quoted text/relative time/locale/write-failure round-trip tests | Rolling captured data; no save dialog in diagnostics |
| Motion and UI responsiveness | Real transitions, animation pause/resume when hidden/minimized, profile/resize sequence and paint/resource measurements | Runner timings are diagnostics, not a universal refresh-rate guarantee |
| Artwork, guide and setup | Package files present; native page/rule screenshots inspected | External device setup is not performed automatically |
| Distribution and security | Clean Windows build, Linux core, sanitizers, CodeQL security-extended, full-history secret scan, SHA-256 manifests and anonymous latest download | App remains unsigned |

## Bugs fixed

- Audio workers could discard an accepted final change when the app closed. They now finish accepted work before exit.
- Rapid mic/bass actions could fill the queue and reject a later Disable. Pending actions coalesce independently by effect category.
- Separate updater result/busy reads could lose the completion needed for a deferred microphone Apply. One synchronized snapshot supplies both.
- Editing microphone settings while downloading could apply stale settings afterward. A selection change cancels that deferred Apply; opening/closing a dropdown preserves it.
- Interrupted mouse capture could skip the final slider commit. It now commits on capture loss.
- Gain disable could leave the displayed percentage stale. Completion resynchronizes it.
- Failed capture/attach attempts could display a success state. Failed actions now return before enabling it.
- Reloading preferences retained removed FPS-limit entries. Reload now clears them first.
- CSV text containing quotes/commas and locale-specific numbers could be ambiguous. Exports now quote text, use UTF-8 and stable decimal formatting, and start time at zero.
- Page transitions stored full-window bitmaps. They now store only the content canvas, reducing temporary memory on wide windows.

- The compatibility page now groups its game selector consistently with other pages. Audio guidance follows the selected view, mono device text uses the singular form, and manual desktop sizing explains work-area fitting.

## Practical limits

Tests do not establish that every game, GPU driver, HDR mode or physical display arrangement works. Audible output gain and actual microphone processing require the user's Equalizer APO device configuration; automated tests verify the generated configuration and real offline RNNoise processing without changing system audio. No Linux desktop binary is included. Optional hooks can conflict with games or security tools, and no ban-free guarantee exists. The previously Defender-blocked optional injection soak remains incomplete and is not counted as a pass.
