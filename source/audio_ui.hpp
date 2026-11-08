// Audio views configure shared-mode endpoint effects. No recording or routing driver is used.
const fg::Microphone* selectedMicrophone() {
    int index = int(SendMessageW(app.controls[MicDevice], CB_GETCURSEL, 0, 0));
    return index >= 0 && size_t(index) < app.microphones.size() ? &app.microphones[index] : nullptr;
}
#include "audio_work.hpp"
void refreshMicrophones() {
    app.microphones =
        app.smokeAudio
            ? std::vector<fg::Microphone>{{L"Diagnostic microphone", L"diagnostic-mic",
                                           L"{01234567-89ab-cdef-0123-456789abcdef}", 48000, 1}}
            : fg::microphones();
    auto combo = app.controls[MicDevice];
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    int selected = 0;
    for (size_t i = 0; i < app.microphones.size(); ++i) {
        SendMessageW(combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(app.microphones[i].name.c_str()));
        if (app.microphones[i].id == app.microphoneId)
            selected = int(i);
    }
    if (app.microphones.empty())
        SendMessageW(combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"No active microphone found"));
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
    if (auto mic = selectedMicrophone())
        app.microphoneId = mic->id;
    selectAutomaticNoisePlugin();
}
void syncMicrophoneControls() {
    SendMessageW(app.controls[MicEQPreset], CB_SETCURSEL,
                 fg::voicePresetIndex(app.microphoneOptions.low, app.microphoneOptions.mid,
                                      app.microphoneOptions.high),
                 0);
    SendMessageW(app.controls[MicNoise], BM_SETCHECK,
                 app.microphoneOptions.noise ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(app.controls[MicEQ], BM_SETCHECK,
                 app.microphoneOptions.equalizer ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(app.controls[MicNoisePreset], CB_SETCURSEL, app.microphoneOptions.preset, 0);
    for (auto pair : {std::pair{MicLow, app.microphoneOptions.low},
                      {MicMid, app.microphoneOptions.mid},
                      {MicHigh, app.microphoneOptions.high}})
        SendMessageW(app.controls[pair.first], CB_SETCURSEL, (pair.second + 12) / 3, 0);
}
std::wstring audioFooter() {
    if (!app.audioNotice.empty())
        return app.audioNotice;
    if (app.audioSetup)
        return L"Choose playback devices for output effects and capture devices for microphone "
               L"effects.";
    if (app.audioTab)
        return L"Mic processing needs Equalizer APO enabled on your recording device. RNNoise "
               L"requires 48 kHz.";
    return L"Normal volume works immediately. Boost above 100% requires Equalizer APO configured "
           L"for your output device.";
}
void paintAudio(Graphics& g) {
    rounded(g, RectF(212, 190, 940, 562), card, 16, true);
    if (app.audioSetup) {
        label(g, L"A little setup. A lot more sound.", 241, 270, 860, 43, 27, text, true);
        label(g,
              L"Windows volume works immediately. Extra effects use Equalizer APO on the device "
              L"you choose.",
              244, 322, 860, 44, 14, muted, false, true);
        const wchar_t* titles[]{L"Install Equalizer APO", L"Choose your devices",
                                L"Return and apply", L"Verify what you hear"};
        const wchar_t* notes[]{
            L"Get the official installer below. Framoryx does not install an audio driver or "
            L"change device settings for you.",
            L"In its device configurator, select your output for bass/boost and your capture "
            L"device for mic effects. Restart if requested.",
            L"Set your mic to 48,000 Hz for RNNoise. Framoryx downloads the latest compatible "
            L"plugin automatically; choose effects and Apply.",
            L"Test playback or a short mic recording in your usual app. Raw/exclusive modes may "
            L"skip effects. Disable effects here to undo them."};
        for (int i = 0; i < 4; ++i) {
            float x = 244 + (i % 2) * 440, y = 382 + (i / 2) * 112;
            rounded(g, RectF(x, y, 420, 98), Color(255, 17, 25, 32), 11, true);
            label(g, L"0" + std::to_wstring(i + 1), x + 15, y + 13, 36, 24, 12, mint, true);
            label(g, titles[i], x + 49, y + 10, 352, 26, 16, text, true);
            label(g, notes[i], x + 15, y + 43, 390, 48, 12, muted, false, true);
        }
        label(g,
              L"Bass and EQ include headroom reduction. Amplifying above 100% can still clip. "
              L"Effects remain configured after exit.",
              244, 605, 860, 22, 10, muted);
        label(g, audioFooter(), 214, 777, 938, 38, 12, muted, false, true);
        return;
    }
    if (app.audioTab == 0) {
        label(g, L"Feel every detail.", 241, 270, 830, 43, 28, text, true);
        label(g, app.audio.device, 244, 321, 855, 27, 14, muted);
        label(g, L"OUTPUT VOLUME", 244, 358, 400, 20, 10, muted, true);
        label(g, std::to_wstring(app.outputPercent) + L"%" + (app.audio.mute ? L" · muted" : L""),
              948, 375, 156, 36, 24, text, true);
        rounded(g, RectF(244, 440, 416, 204), Color(255, 17, 25, 32), 12, true);
        rounded(g, RectF(680, 440, 424, 204), Color(255, 17, 25, 32), 12, true);
        auto gain = app.cachedGain;
        auto bass = app.cachedBass;
        label(g, L"Extra volume", 263, 457, 370, 28, 17, text, true);
        label(g, L"Bass enhancement", 699, 457, 380, 28, 17, text, true);
        label(g, gain ? L"Configured: +" + value(*gain, 2) + L" dB" : L"Not configured", 264, 551,
              370, 22, 12, gain ? mint : muted);
        label(g, L"Preamplification can distort. Start gently and reduce it if audio clips.", 264,
              582, 366, 45, 12, muted, false, true);
        label(g,
              bass ? L"Configured: +" + std::to_wstring(*bass) + L" dB at 90 Hz"
                   : L"Not configured",
              700, 551, 380, 22, 12, bass ? mint : muted);
        label(g,
              L"A smooth low shelf with matching headroom reduction. Other boosts can still cause "
              L"clipping.",
              700, 582, 380, 45, 12, muted, false, true);
        label(g,
              app.boost.available()
                  ? L"Equalizer APO config found · device setup still needs verification."
                  : L"Extra volume and bass need Equalizer APO enabled for your playback device.",
              244, 655, 860, 24, 12, app.boost.available() ? mint : amber);
    } else {
        label(g, L"Your voice, clearer.", 241, 270, 690, 43, 28, text, true);
        auto mic = selectedMicrophone();
        label(g, L"MICROPHONE", 244, 316, 830, 20, 10, muted, true);
        std::wstring format = mic ? std::to_wstring(mic->rate) + L" Hz · " +
                                        std::to_wstring(mic->channels) +
                                        (mic->channels == 1 ? L" channel" : L" channels")
                                  : L"Connect a microphone, then Refresh.";
        bool noise = app.cachedNoise;
        bool eq = app.cachedEqualizer;
        format += noise || eq ? L" · Effects configured; verify in a recording"
                              : L" · Effects not configured";
        label(g, format, 244, 380, 860, 23, 12, noise || eq ? mint : muted);
        rounded(g, RectF(244, 414, 416, 232), Color(255, 17, 25, 32), 12, true);
        rounded(g, RectF(680, 414, 424, 232), Color(255, 17, 25, 32), 12, true);
        label(g, L"VOICE DETECTION", 264, 467, 365, 20, 10, muted, true);
        label(g, L"VOICE PRESET", 700, 467, 366, 20, 10, muted, true);
        label(g, L"LOW · 150 Hz", 700, 539, 116, 19, 10, muted, true);
        label(g, L"MID · 1.2 kHz", 832, 539, 116, 19, 10, muted, true);
        label(g, L"HIGH · 6 kHz", 964, 539, 116, 19, 10, muted, true);
        label(g,
              fg::voicePresetIndex(app.microphoneOptions.low, app.microphoneOptions.mid,
                                   app.microphoneOptions.high) == 4
                  ? L"Bassy voice · warmer lows, less boxiness. Headroom included."
                  : L"EQ gains include automatic headroom reduction.",
              700, 613, 380, 22, 10, muted);
        bool plugin = app.cachedPlugin;
        label(g,
              app.noiseUpdater.running()
                  ? app.noiseUpdater.status()
                  : (plugin ? L"RNNoise VST2 ready · 48 kHz required"
                            : L"RNNoise downloads automatically · 48 kHz required"),
              264, 589, 373, 23, 10, plugin ? mint : muted);
        label(g,
              L"One-time setup: enable Equalizer APO for this capture device. Effects persist "
              L"until disabled.",
              244, 727, 860, 20, 11, muted);
    }
    label(g, audioFooter(), 214, 777, 938, 38, 12, muted, false, true);
}
bool audioCommand(int id, int notification) {
    bool selection = id == MicDevice || id == MicNoisePreset || id == MicEQPreset ||
                     (id >= MicLow && id <= MicHigh);
    if (selection && notification != CBN_SELCHANGE)
        return true;
    if (id == MicRefresh || id == MicDevice || id == MicNoise || id == MicEQ ||
        id == MicNoisePreset || id == MicEQPreset || id == MicLow || id == MicMid ||
        id == MicHigh || id == MicChoosePlugin || id == MicDownload)
        cancelPendingMicrophoneEdit();
    if (id == AudioOutputView || id == AudioMicView) {
        if (!app.gainWorker.busy() && !app.effectWorker.busy() &&
            app.pendingMicrophoneApply.empty())
            app.audioNotice.clear();
        app.audioSetup = false;
        app.audioTab = id == AudioMicView ? 1 : 0;
        if (app.audioTab)
            refreshMicrophones();
        if (app.audioTab && app.autoNoiseUpdates)
            startNoiseUpdate(false);
        pageControls(1);
    } else if (id == MicRefresh) {
        refreshMicrophones();
        app.audioNotice =
            L"Microphone list refreshed. Device formats are read without recording audio.";
    } else if (id == MicDevice && notification == CBN_SELCHANGE) {
        if (auto mic = selectedMicrophone())
            app.microphoneId = mic->id;
        selectAutomaticNoisePlugin();
        saveSettings();
    } else if (id == MicNoise || id == MicEQ) {
        app.microphoneOptions.noise =
            SendMessageW(app.controls[MicNoise], BM_GETCHECK, 0, 0) == BST_CHECKED;
        app.microphoneOptions.equalizer =
            SendMessageW(app.controls[MicEQ], BM_GETCHECK, 0, 0) == BST_CHECKED;
        saveSettings();
    } else if (id == MicNoisePreset && notification == CBN_SELCHANGE) {
        app.microphoneOptions.preset = int(SendMessageW(app.controls[id], CB_GETCURSEL, 0, 0));
        saveSettings();
    } else if (id == MicEQPreset && notification == CBN_SELCHANGE) {
        int index = int(SendMessageW(app.controls[id], CB_GETCURSEL, 0, 0));
        if (index >= 0 && size_t(index) < fg::voicePresets.size()) {
            const auto& preset = fg::voicePresets[index];
            app.microphoneOptions.low = preset.low;
            app.microphoneOptions.mid = preset.mid;
            app.microphoneOptions.high = preset.high;
            app.microphoneOptions.equalizer = true;
            syncMicrophoneControls();
            saveSettings();
        }
    } else if (id >= MicLow && id <= MicHigh && notification == CBN_SELCHANGE) {
        int value = 3 * int(SendMessageW(app.controls[id], CB_GETCURSEL, 0, 0)) - 12;
        if (id == MicLow)
            app.microphoneOptions.low = value;
        if (id == MicMid)
            app.microphoneOptions.mid = value;
        if (id == MicHigh)
            app.microphoneOptions.high = value;
        SendMessageW(app.controls[MicEQPreset], CB_SETCURSEL,
                     fg::voicePresetIndex(app.microphoneOptions.low, app.microphoneOptions.mid,
                                          app.microphoneOptions.high),
                     0);
        saveSettings();
    } else if (id == MicChoosePlugin) {
        wchar_t path[32768]{};
        OPENFILENAMEW of{};
        of.lStructSize = sizeof(of);
        of.hwndOwner = app.window;
        of.lpstrFilter = L"RNNoise VST2 DLL\0rnnoise_*.dll\0";
        of.lpstrFile = path;
        of.nMaxFile = 32768;
        of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&of)) {
            auto mic = selectedMicrophone();
            if (mic && fg::rnnoisePlugin(path, mic->channels)) {
                app.noisePlugin = path;
                app.manualNoisePlugin = true;
                saveSettings();
                app.audioNotice = L"Plugin selected. Click Apply mic effects after completing "
                                  L"capture-device setup.";
            } else
                app.audioNotice = L"Select the official x64 rnnoise_mono.dll or rnnoise_stereo.dll "
                                  L"matching this mic’s channels.";
        }
    } else if (id == MicDownload) {
        app.manualNoisePlugin = false;
        startNoiseUpdate(true);
        saveSettings();
    } else if (id == MicAutoUpdates) {
        app.autoNoiseUpdates =
            SendMessageW(app.controls[MicAutoUpdates], BM_GETCHECK, 0, 0) == BST_CHECKED;
        saveSettings();
        if (app.autoNoiseUpdates)
            startNoiseUpdate(false);
    } else if (id == MicApply) {
        app.pendingMicrophoneApply.clear();
        auto mic = selectedMicrophone();
        if (mic && app.microphoneOptions.noise &&
            (!fg::rnnoisePlugin(app.noisePlugin, mic->channels) ||
             (!app.manualNoisePlugin && selectedNoiseDigest().empty()))) {
            app.manualNoisePlugin = false;
            app.pendingMicrophoneApply = mic->id;
            app.pendingMicrophoneOptions = app.microphoneOptions;
            startNoiseUpdate(false);
            app.audioNotice = L"Downloading RNNoise. Your requested mic effects will be configured "
                              L"when it is ready.";
            return true;
        }
        if (mic)
            queueMicrophoneEffects(*mic, app.microphoneOptions);
        else
            app.audioNotice = L"Select an available microphone first.";
    } else if (id == MicDisable) {
        app.pendingMicrophoneApply.clear();
        bool queued = app.effectWorker.submit(
            [directory = app.boost.directory()] {
                auto error = fg::MicrophoneEffects::disable(directory);
                return error.empty() ? std::wstring(L"Framoryx microphone effects disabled. Other "
                                                    L"effects are unchanged.")
                                     : error;
            },
            1);
        app.audioNotice =
            queued ? L"Disabling microphone effects…" : L"Audio queue is busy. Try again shortly.";
    } else if (id == MicSetup) {
        app.audioSetup = true;
        pageControls(1);
    } else if (id == AudioSetupBack) {
        app.audioSetup = false;
        pageControls(1);
    } else if (id == AudioGetAPO) {
        ShellExecuteW(app.window, L"open", L"https://sourceforge.net/projects/equalizerapo/",
                      nullptr, nullptr, SW_SHOWNORMAL);
    } else if (id == AudioConfigurator) {
        auto file =
            (fs::path(fg::AudioBoost::registeredDirectory()).parent_path() / L"Configurator.exe")
                .wstring();
        if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES)
            ShellExecuteW(app.window, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        else
            app.audioNotice = L"Equalizer APO device configurator was not found. Install it first, "
                              L"or locate its config folder.";
    } else if (id == BassApply || id == BassDisable) {
        if (!app.smokeAudio)
            app.audio = fg::audioState();
        int db = int(SendMessageW(app.controls[BassAmount], CB_GETCURSEL, 0, 0)) * 3;
        bool queued = app.effectWorker.submit(
            [directory = app.boost.directory(), guid = app.audio.guid, db, id] {
                auto error = id == BassApply ? fg::OutputBass::apply(directory, guid, db)
                                             : fg::OutputBass::disable(directory);
                return error.empty()
                           ? std::wstring(
                                 id == BassApply
                                     ? L"Bass configured with matching headroom compensation. "
                                       L"Verify playback with APO enabled."
                                     : L"Framoryx bass disabled. Other effects are unchanged.")
                           : error;
            },
            2);
        app.audioNotice =
            queued ? L"Updating bass enhancement…" : L"Audio queue is busy. Try again shortly.";
    } else
        return false;
    refreshAudioStatus();
    app.contentDirty = true;
    InvalidateRect(app.window, nullptr, FALSE);
    return true;
}
