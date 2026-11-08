// Audio views use shared-mode endpoint effects. No recording, routing driver or plugin is
// installed.
const fg::Microphone* selectedMicrophone() {
    int index = int(SendMessageW(app.controls[MicDevice], CB_GETCURSEL, 0, 0));
    return index >= 0 && size_t(index) < app.microphones.size() ? &app.microphones[index] : nullptr;
}
#include "audio_work.hpp"
void refreshMicrophones() {
    app.microphones = fg::microphones();
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
              244, 745, 860, 22, 10, muted);
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
        auto gain = app.boost.configuredGain(app.audio.guid);
        auto bass = fg::OutputBass::configured(app.boost.directory(), app.audio.guid);
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
                                        std::to_wstring(mic->channels) + L" channels"
                                  : L"Connect a microphone, then Refresh.";
        bool noise = mic && fg::MicrophoneEffects::configured(app.boost.directory(), mic->guid);
        bool eq =
            mic && fg::MicrophoneEffects::equalizerConfigured(app.boost.directory(), mic->guid);
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
        label(g, L"EQ gains include automatic headroom reduction.", 700, 613, 380, 22, 11, muted);
        bool plugin = mic && fg::rnnoisePlugin(app.noisePlugin, mic->channels);
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
    label(g, app.audioNotice, 214, 777, 938, 38, 12, muted, false, true);
}
bool audioCommand(int id, int notification) {
    if (id == AudioOutputView || id == AudioMicView) {
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
        constexpr int presets[][3]{{0, 0, 0}, {-3, 0, 3}, {3, 0, 0}, {-3, 3, 0}};
        if (index >= 0 && index < 4) {
            app.microphoneOptions.low = presets[index][0];
            app.microphoneOptions.mid = presets[index][1];
            app.microphoneOptions.high = presets[index][2];
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
        SendMessageW(app.controls[MicEQPreset], CB_SETCURSEL, -1, 0);
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
        auto mic = selectedMicrophone();
        if (mic && app.microphoneOptions.noise &&
            !fg::rnnoisePlugin(app.noisePlugin, mic->channels)) {
            app.manualNoisePlugin = false;
            app.pendingMicrophoneApply = mic->id;
            app.pendingMicrophoneOptions = app.microphoneOptions;
            startNoiseUpdate(false);
            app.audioNotice = L"Downloading RNNoise. Your requested mic effects will be configured "
                              L"when it is ready.";
            return true;
        }
        auto error = mic ? fg::MicrophoneEffects::apply(app.boost.directory(), *mic,
                                                        app.noisePlugin, app.microphoneOptions)
                         : L"Select an available microphone first.";
        app.audioNotice =
            error.empty() ? L"Mic effects configured for the selected device. Verify in a "
                            L"recording; raw/exclusive audio may bypass APO. No audio was recorded."
                          : error;
    } else if (id == MicDisable) {
        app.pendingMicrophoneApply.clear();
        auto error = fg::MicrophoneEffects::disable(app.boost.directory());
        app.audioNotice =
            error.empty()
                ? L"Framoryx microphone effects disabled. Other audio effects are unchanged."
                : error;
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
        auto file = (fs::path(app.boost.directory()).parent_path() / L"Configurator.exe").wstring();
        if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES)
            ShellExecuteW(app.window, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        else
            app.audioNotice = L"Equalizer APO device configurator was not found. Install it first, "
                              L"or locate its config folder.";
    } else if (id == BassApply || id == BassDisable) {
        app.audio = fg::audioState();
        int db = int(SendMessageW(app.controls[BassAmount], CB_GETCURSEL, 0, 0)) * 3;
        auto error = id == BassApply
                         ? fg::OutputBass::apply(app.boost.directory(), app.audio.guid, db)
                         : fg::OutputBass::disable(app.boost.directory());
        app.audioNotice =
            error.empty()
                ? (id == BassApply
                       ? L"Bass configured with matching headroom compensation. Verify playback; "
                         L"requires Equalizer APO on this output."
                       : L"Framoryx bass enhancement disabled. Other audio effects are unchanged.")
                : error;
    } else
        return false;
    app.contentDirty = true;
    InvalidateRect(app.window, nullptr, FALSE);
    return true;
}
