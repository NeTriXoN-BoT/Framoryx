void syncOutputVolume() {
    if (app.gainWorker.busy())
        return;
    app.audio = fg::audioState();
    double gain = app.boost.configuredGain(app.audio.guid).value_or(0);
    app.outputBoostActive = gain > .001;
    app.outputPercent =
        std::clamp(int(std::lround(app.audio.volume * 100 * std::pow(10., gain / 20.))), 0, 200);
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
        if (!app.pendingMicrophoneApply.empty()) {
            auto mic = selectedMicrophone();
            if (mic && mic->id == app.pendingMicrophoneApply) {
                auto error = fg::MicrophoneEffects::apply(
                    app.boost.directory(), *mic, app.noisePlugin, app.pendingMicrophoneOptions);
                app.audioNotice = error.empty()
                                      ? L"Latest RNNoise installed and mic effects configured. "
                                        L"Verify your capture-device setup in a recording."
                                      : error;
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
        app.contentDirty = true;
    }
}
