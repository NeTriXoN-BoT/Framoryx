# Audio setup

Framoryx has separate **Output** and **Microphone** views. Windows uses a separately configured [Equalizer APO](https://sourceforge.net/projects/equalizerapo/) for additional processing. The 0.3 Linux implementation uses PipeWire and WirePlumber. See [Linux release status](LINUX.md) before downloading.

## Linux processing

Normal output volume uses PipeWire and accepts 0–200%. Bass creates a **Framoryx Output** virtual sink; choose it in your playback settings after applying. Microphone EQ and RNNoise create **Framoryx Microphone**; choose that input in your game or voice app. Apply does not explicitly switch the system default device. The separate **Use as default** action is optional.

The filters target the physical device selected when applying. They do not fall back to another input or route into their own virtual device if that target disappears. Reconnect the device, refresh, and Apply again when needed. EQ reserves headroom in the same way as the Windows presets below. The noise plugin runs at 48 kHz internally.

Framoryx checks the official Linux plugin release, verifies the archive and extracted plugin, and prepares it automatically when noise suppression is requested. Effect processes are owned by Framoryx and stop when it exits. No system audio config, driver or service is installed.

## Windows output: 0–200%

The slider shows its percentage as you drag. From 0–100%, it controls the default Windows output. Above 100%, Windows stays at full volume and Framoryx configures APO preamplification: **200% means twice the sample amplitude, approximately +6.0206 dB**. Changes are coalesced on a worker thread. If APO's configuration is missing, the slider stays at 100% and explains the setup requirement.

APO must also be enabled for that playback device in its device configurator. Finding a config file does not prove processing is active. Positive amplification can clip; reduce the level if you hear distortion. The percentage is a requested output level, not a measured loudness value. Framoryx does not include Letasoft's audio engine or a dynamic compressor.

**Bass enhancement** adds a smooth 90 Hz low shelf, with an equal negative preamp for headroom. Choose 0, +3, +6 or +9 dB and Apply. The tradeoff is quieter frequencies outside the bass region. Other boosts and existing filters can still use up that headroom. Disable removes only Framoryx's bass contribution.

## Windows microphone: RNNoise and voice EQ

1. Install Equalizer APO from its official project. In its device configurator, enable your intended **capture device**, then restart if requested. The default communications microphone is preferred in Framoryx's device list.
2. For RNNoise, choose a mono or stereo **48,000 Hz** microphone format in Windows Sound settings. Refresh the device list after changing it.
3. Open **Microphone**. Framoryx checks the official [werman/noise-suppression-for-voice releases](https://github.com/werman/noise-suppression-for-voice/releases), downloads the stable Windows VST2 package and chooses mono/stereo automatically. No GitHub account is required.
4. Select noise suppression and/or microphone EQ, then **Apply mic effects**. If the plugin is still downloading, the requested configuration is applied when it is ready, provided the selected microphone has not changed.
5. Verify the result in a short recording or your usual voice-chat app. Select the same microphone there. Use **Disable effects** to disable Framoryx's microphone processing.

The RNNoise presets use voice-detection thresholds of 50% (**Natural**), 85% (**Balanced**) and 95% (**Strong**), with a 200 ms grace period and no retroactive lookahead. A higher threshold can cut quiet speech; try Natural if words disappear. RNNoise is designed for voice and cannot guarantee removal of every sound or echo. See the [upstream settings and sample-rate requirements](https://github.com/werman/noise-suppression-for-voice#plugin-settings).

**Microphone EQ** has low (150 Hz), mid (1.2 kHz) and high (6 kHz) bands, adjustable from −12 to +12 dB in 3 dB steps, plus Flat, Clarity, Warmth, Presence and **Bassy voice** presets. Bassy voice adds +6 dB below 150 Hz, cuts 1.2 kHz by 3 dB to reduce boxiness, and leaves the high band flat. A −6 dB preamp reserves headroom. This can warm a thin microphone; the result still depends on its capsule, placement and room. Preset labels restore from saved gains, and edited combinations show Custom. EQ alone does not require the noise plugin or a 48 kHz format.

## Automatic plugin updates

Automatic checks run when opening audio controls or preparing a requested effect, with cached release checks between refreshes. **Check for updates** requests a fresh check; the automatic-update switch can disable future automatic checks. Windows also allows a matching external x64 VST2 DLL to be chosen manually.

Downloads use HTTPS from the official GitHub repository and GitHub's asset servers, with bounded sizes, archive CRC checks, x64/plugin-name checks and an isolated audio/preset compatibility probe. Available upstream SHA-256 digests are checked; the verified v1.10 archive is additionally pinned to its known SHA-256. A locally calculated hash alone does not establish publisher identity.

Versions live in separate folders under `tools/rnnoise`, with source/version information. Cached DLLs are checked against stored hashes, with compiled known-file hashes for v1.10. Applying an effect verifies the selected bytes again and stages a separate copy under `EqualizerAPO/config/FramoryxPlugins/<SHA-256>/`; the APO config uses that installed copy. Moving or editing a download does not change an already installed plugin. Folder permissions still matter: keep your APO installation and configuration under your control.

A failed check leaves the existing configuration intact. New compatible versions become the automatic selection for the next Apply; Framoryx does not silently rewrite an active microphone chain just because a new version appears. Old installed versions remain available for existing configurations. Audio writes are serialized within the app and use atomic replacement to prevent gain, bass and microphone updates from losing each other's include lines.

The external plugin is GPL-3.0 and downloaded directly from its upstream distribution; it is not embedded in Framoryx's executable. [RNNoise source and license](https://github.com/werman/noise-suppression-for-voice) and [EasyEffects](https://github.com/wwmm/easyeffects) are separate projects. Framoryx's current Windows audio controls do not include the full EasyEffects effect chain.

## Windows persistence and limitations

Framoryx backs up `config.txt` before adding its first include for an effect. Its own boost, bass and microphone files are separate from existing filters; unowned files are not overwritten. Protected APO folders may require administrator rights. Effects remain configured after Framoryx exits, until disabled here or removed in APO.

Raw, exclusive and ASIO paths may bypass system effects. Driver/device combinations vary, so a **configured** status is not confirmation of audible processing. Framoryx does not install a virtual microphone driver, change the Windows default input, upload audio or install Equalizer APO automatically.

## Microphone test and privacy

**Record test** records for five seconds only after you choose it. A live meter shows input activity, and **Play sample** lets you hear the recording. Processed microphone testing on Linux uses Framoryx Microphone when ready. Windows uses the selected capture device and its configured system effects.

The sample stays in memory, is bounded in size, and is discarded when the app closes or another test replaces it. There is no automatic background recording or audio upload. A silent recording can indicate a muted input, disconnected routing, device permissions or an unavailable physical microphone.
