void refreshAudioStatus() {
    if (app.gainWorker.busy() || app.effectWorker.busy())
        return;
    auto old = std::tuple(app.cachedGain, app.cachedBass, app.cachedNoise, app.cachedEqualizer,
                          app.cachedPlugin);
    app.cachedGain = app.boost.configuredGain(app.audio.guid);
    app.cachedBass = fg::OutputBass::configured(app.boost.directory(), app.audio.guid);
    auto mic = selectedMicrophone();
    app.cachedNoise = mic && fg::MicrophoneEffects::configured(app.boost.directory(), mic->guid);
    app.cachedEqualizer =
        mic && fg::MicrophoneEffects::equalizerConfigured(app.boost.directory(), mic->guid);
    bool digestReady =
        app.manualNoisePlugin ||
        (mic && !(mic->channels == 1 ? app.noiseRelease.monoDigest : app.noiseRelease.stereoDigest)
                     .empty());
    app.cachedPlugin = mic && digestReady && fg::rnnoisePlugin(app.noisePlugin, mic->channels);
    if (old != std::tuple(app.cachedGain, app.cachedBass, app.cachedNoise, app.cachedEqualizer,
                          app.cachedPlugin))
        app.contentDirty = true;
}
void syncOutputVolume() {
    if (app.gainWorker.busy() || app.effectWorker.busy())
        return;
    auto old = std::tuple(app.audio.device, app.audio.guid, app.outputPercent, app.audio.mute,
                          app.audio.available);
    app.audio = fg::audioState();
    refreshAudioStatus();
    double gain = app.cachedGain.value_or(0);
    app.outputBoostActive = gain > .001;
    app.outputPercent =
        std::clamp(int(std::lround(app.audio.volume * 100 * std::pow(10., gain / 20.))), 0, 200);
    if (old != std::tuple(app.audio.device, app.audio.guid, app.outputPercent, app.audio.mute,
                          app.audio.available))
        app.contentDirty = true;
    if (app.controls.count(Volume))
        SendMessageW(app.controls[Volume], TBM_SETPOS, TRUE, app.outputPercent);
}
void startNoiseUpdate(bool force) {
    app.noiseUpdater.start(fg::executableDir() + L"\\tools\\rnnoise", force);
}
void selectAutomaticNoisePlugin() {
    auto mic = selectedMicrophone();
    if (mic && !app.manualNoisePlugin && !app.noiseRelease.version.empty())
        app.noisePlugin = mic->channels == 1 ? app.noiseRelease.mono : app.noiseRelease.stereo;
}
std::string selectedNoiseDigest() {
    if (app.manualNoisePlugin)
        return {};
    auto mic = selectedMicrophone();
    return !mic
               ? std::string()
               : (mic->channels == 1 ? app.noiseRelease.monoDigest : app.noiseRelease.stereoDigest);
}
void queueMicrophoneEffects(const fg::Microphone& mic, const fg::MicrophoneOptions& options) {
    bool queued =
        app.effectWorker.submit([directory = app.boost.directory(), mic, plugin = app.noisePlugin,
                                 options, digest = selectedNoiseDigest()] {
            auto error = fg::MicrophoneEffects::apply(directory, mic, plugin, options, digest);
            return error.empty()
                       ? std::wstring(L"Mic effects configured for the selected device. Verify in "
                                      L"a recording; raw/exclusive modes can bypass APO.")
                       : error;
        });
    app.audioNotice = queued ? L"Applying microphone effects… The controls stay available."
                             : L"Audio queue is busy. Try again shortly.";
}
void pollAudioWork() {
    auto status = app.noiseUpdater.status();
    if (status != app.noiseUpdateStatus) {
        app.noiseUpdateStatus = status;
        app.contentDirty = true;
    }
    if (auto release = app.noiseUpdater.takeResult()) {
        app.noiseRelease = *release;
        selectAutomaticNoisePlugin();
        saveSettings();
        refreshAudioStatus();
        if (!app.pendingMicrophoneApply.empty()) {
            auto mic = selectedMicrophone();
            if (mic && mic->id == app.pendingMicrophoneApply) {
                queueMicrophoneEffects(*mic, app.pendingMicrophoneOptions);
            } else
                app.audioNotice =
                    L"RNNoise is ready. Select your microphone and click Apply mic effects.";
            app.pendingMicrophoneApply.clear();
        }
        app.contentDirty = true;
    }
    if (!app.noiseUpdater.running() && !app.pendingMicrophoneApply.empty()) {
        app.pendingMicrophoneApply.clear();
        app.audioNotice = app.noiseUpdater.status();
        app.contentDirty = true;
    }
    if (auto result = app.gainWorker.takeResult()) {
        if (!result->empty()) {
            app.audioNotice = *result;
            syncOutputVolume();
        }
        refreshAudioStatus();
        app.contentDirty = true;
    }
    if (auto result = app.effectWorker.takeResult()) {
        app.audioNotice = *result;
        refreshAudioStatus();
        app.contentDirty = true;
    }
}
