// Bounded diagnostics of this application's own controls and its managed test scene.
// Enabled only with a diagnostic report, isolated settings and an explicit smoke flag.
void featureCheck(bool success, const char* label) {
    ++app.featureChecks;
    if (!success)
        app.featureFailures.emplace_back(label);
}
HWND featureFixtureWindow() {
    struct Search {
        DWORD pid;
        HWND window = nullptr;
    } search{app.demoPid};
    EnumWindows(
        [](HWND window, LPARAM context) -> BOOL {
            auto search = reinterpret_cast<Search*>(context);
            DWORD pid = 0;
            GetWindowThreadProcessId(window, &pid);
            if (pid == search->pid && IsWindowVisible(window)) {
                search->window = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.window;
}
void featureCommand(int id) {
    SendMessageW(app.window, WM_COMMAND, id, 0);
}
void featureCombo(int id, int index) {
    SendMessageW(app.controls[id], CB_SETCURSEL, index, 0);
    SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE),
                 reinterpret_cast<LPARAM>(app.controls[id]));
}
bool featureSelectFixture() {
    refreshTargets();
    for (size_t i = 0; i < app.targets.size(); ++i)
        if (app.targets[i].pid == app.demoPid) {
            featureCombo(Target, int(i + 1));
            return true;
        }
    return false;
}
void featureRules(bool block) {
    featureCommand(Rules);
    auto dialog = GetLastActivePopup(app.window);
    if (!dialog || dialog == app.window) {
        featureCheck(false, "open native rule editor");
        return;
    }
    SetWindowTextW(GetDlgItem(dialog, 1), L"FramoryxTest.exe\r\nexplorer.exe");
    SetWindowTextW(GetDlgItem(dialog, 2), block ? L"FramoryxTest.exe" : L"");
    SendMessageW(dialog, WM_COMMAND, 3, reinterpret_cast<LPARAM>(GetDlgItem(dialog, 3)));
    featureCheck(!IsWindow(dialog), "save rules through actual editor");
}
void runFeatureStep() {
    if (app.featureRunning)
        return;
    if (app.featureStep == 23 && !app.stats.live)
        return;
    if ((app.featureStep == 13 || app.featureStep == 14 || app.featureStep == 15 ||
         app.featureStep == 16) &&
        (app.gainWorker.busy() || app.effectWorker.busy()))
        return;
    app.featureRunning = true;
    int step = app.featureStep++;
    auto window = featureFixtureWindow();
    auto volume = [&](int amount) {
        SendMessageW(app.controls[Volume], TBM_SETPOS, TRUE, amount);
        SendMessageW(app.window, WM_HSCROLL, TB_THUMBPOSITION,
                     reinterpret_cast<LPARAM>(app.controls[Volume]));
    };
    switch (step) {
    case 0:
        featureCheck(window && featureSelectFixture(), "discover and select managed game window");
        featureCheck(!app.monitors.empty(), "enumerate displays");
        featureCheck(app.hotkeysRegistered, "register advertised global hotkeys");
        if (!app.capture.running())
            featureCommand(Capture);
        featureCheck(app.capture.running(), "start presentation capture through UI");
        if (!app.overlay)
            featureCommand(Overlay);
        if (window)
            SetForegroundWindow(window);
        positionHud();
        featureCheck(app.overlay && IsWindowVisible(app.hud),
                     "show layered desktop overlay on foreground fixture");
        break;
    case 1:
        featureCheck(app.stats.live && app.stats.frames.size() > 20 && app.stats.fps > 10,
                     "receive real game frames and graph data");
        featureCheck(app.cpuReady && app.cpu >= 0 && app.cpu <= 100 && app.memoryReady &&
                         app.totalGB > 0,
                     "read real CPU and memory metrics");
        featureCommand(CompatibilityTab);
        featureCommand(CompatibilityRefresh);
        featureCheck(app.compatibility.inspected && app.compatibility.dxgi,
                     "inspect selected renderer compatibility");
        featureCommand(Export);
        featureCheck(fs::exists(fs::path(app.diagnosticPath).parent_path() / L"feature-export.csv"),
                     "export live capture through UI");
        break;
    case 2:
        SendMessageW(app.window, WM_HOTKEY, 1, 0);
        featureCheck(!app.overlay, "route overlay hide hotkey");
        SendMessageW(app.window, WM_HOTKEY, 1, 0);
        featureCheck(app.overlay, "route overlay show hotkey");
        SendMessageW(app.window, WM_HOTKEY, 3, 0);
        featureCheck(!app.hook.active(), "route hook-disable hotkey safely");
        featureCommand(LimiterTab);
        SetWindowTextW(app.controls[CapValue], L"60");
        updateCap();
        featureCommand(CapUp);
        featureCheck(app.cap == 61, "increase FPS stepper");
        featureCommand(CapDown);
        featureCheck(app.cap == 60, "decrease FPS stepper");
        SetWindowTextW(app.controls[CapValue], L"9");
        featureCheck(!updateCap() && app.cap == 60,
                     "reject out-of-range limit without changing cap");
        SetWindowTextW(app.controls[CapValue], L"180");
        featureCheck(updateCap(), "save per-game frame limit");
        break;
    case 3:
        featureCombo(Target, 0);
        featureCheck(featureSelectFixture() && app.cap == 180, "restore selected game's FPS limit");
        featureCommand(OverlayTab);
        for (int style = 0; style < 5; ++style) {
            featureCombo(HudStyle, style);
            featureCheck(app.hudOptions.style == style, "select each HUD layout");
        }
        for (int i = 0; i < 16; ++i) {
            SendMessageW(app.controls[HudFirstField + i], BM_SETCHECK, BST_UNCHECKED, 0);
            featureCommand(HudFirstField + i);
            featureCheck(!(app.hudOptions.fields & hudFields[i]), "turn off each HUD readout");
        }
        featureCheck(app.hudOptions.fields == 0, "hide all readouts");
        for (int i = 0; i < 16; ++i)
            SendMessageW(app.controls[HudFirstField + i], BM_SETCHECK, BST_CHECKED, 0);
        featureCommand(HudFirstField);
        featureCheck(app.hudOptions.fields == fg::HudAll, "restore all readouts");
        break;
    case 4:
        for (int percent : {50, 100, 200, 300}) {
            selectComboValue(HudSize, percent);
            SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(HudSize, CBN_SELCHANGE), 0);
            featureCheck(app.hudOptions.scale == percent, "apply overlay percentage range");
        }
        selectComboValue(HudOpacity, 75);
        SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(HudOpacity, CBN_SELCHANGE), 0);
        featureCombo(HudAccent, 3);
        featureCombo(HudPosition, 3);
        featureCombo(HudMode, 1);
        featureCheck(app.hudOptions.opacity == 75 && app.hudOptions.accent == 3 &&
                         app.corner == 3 && app.hudMode == 1,
                     "apply opacity, accent, corner and desktop mode");
        break;
    case 5: {
        saveSettings();
        app.gameLimits["stale.exe"] = 999;
        app.hudProfiles["stale.exe"] = app.hudBase;
        loadSettings();
        featureCheck(!app.gameLimits.count("stale.exe") && !app.hudProfiles.count("stale.exe"),
                     "reload removes stale profiles and limits");
        applyProfile("FramoryxTest.exe");
        featureCheck(app.hudOptions.scale == 300 && app.hudOptions.opacity == 75 &&
                         app.hudOptions.fields == fg::HudAll,
                     "round-trip customized game profile");
        featureCommand(HudReset);
        featureCheck(app.hudOptions.scale == 0 && app.hudOptions.style == 0,
                     "reset current overlay profile");
        break;
    }
    case 6:
        featureCommand(BorderlessTab);
        if (window) {
            GetWindowRect(window, &app.featureOriginalRect);
            app.featureOriginalStyle = GetWindowLongPtrW(window, GWL_STYLE);
        }
        featureCommand(Borderless);
        SendMessageW(app.window, WM_HOTKEY, 2, 0);
        featureCheck(window && app.borderless.contains(window), "apply borderless through UI");
        break;
    case 7: {
        RECT rect{};
        if (window)
            GetWindowRect(window, &rect);
        auto monitor = selectedMonitor(window);
        featureCheck(window && EqualRect(&rect, &monitor) &&
                         !(GetWindowLongPtrW(window, GWL_STYLE) & WS_CAPTION),
                     "borderless fills selected display");
        featureCommand(Restore);
        break;
    }
    case 8: {
        RECT rect{};
        if (window)
            GetWindowRect(window, &rect);
        featureCheck(window && EqualRect(&rect, &app.featureOriginalRect) &&
                         GetWindowLongPtrW(window, GWL_STYLE) == app.featureOriginalStyle,
                     "restore original window rectangle and style");
        SendMessageW(app.controls[AutoBorderless], BM_SETCHECK, BST_CHECKED, 0);
        featureCommand(AutoBorderless);
        break;
    }
    case 9:
        featureCheck(window && app.autoBorderless && app.borderless.contains(window),
                     "automatically borderless an allowed fixture");
        featureRules(true);
        featureCheck(app.rules.blocked("FramoryxTest.exe") &&
                         !app.rules.hookAllowed("FramoryxTest.exe"),
                     "block list takes priority over allow list");
        break;
    case 10:
        featureCheck(!app.selectedPid && std::none_of(app.targets.begin(), app.targets.end(),
                                                      [](auto& t) { return t.pid == app.demoPid; }),
                     "blocked game is removed from selection");
        featureCheck(!app.borderless.contains(window),
                     "blocking a game restores managed borderless state");
        featureCheck(!app.rules.hookAllowed("explorer.exe"),
                     "immutable process block survives allow-list edit");
        featureRules(false);
        SendMessageW(app.controls[AutoBorderless], BM_SETCHECK, BST_UNCHECKED, 0);
        featureCommand(AutoBorderless);
        featureCheck(featureSelectFixture(), "restore game selection after unblocking");
        featureCommand(AudioTab);
        featureCommand(AudioOutputView);
        break;
    case 11:
        featureCommand(AudioTab);
        featureCommand(AudioOutputView);
        SendMessageW(app.controls[Volume], WM_KEYDOWN, VK_HOME, 0);
        featureCheck(app.outputPercent == 0, "volume keyboard Home sets zero");
        SendMessageW(app.controls[Volume], WM_KEYDOWN, VK_END, 0);
        featureCheck(app.outputPercent == 200, "volume keyboard End reaches 200 percent");
        SendMessageW(app.controls[Volume], WM_KEYDOWN, VK_LEFT, 0);
        featureCheck(app.outputPercent == 199, "volume keyboard step updates live percentage");
        {
            RECT rect{};
            GetClientRect(app.controls[Volume], &rect);
            int x = int(12 * app.scale + (rect.right - 24 * app.scale) * .375);
            SendMessageW(app.controls[Volume], WM_LBUTTONDOWN, MK_LBUTTON,
                         MAKELPARAM(x, int(13 * app.scale)));
            SetCapture(app.window);
            ReleaseCapture();
            int position = int(SendMessageW(app.controls[Volume], TBM_GETPOS, 0, 0));
            featureCheck(!app.volumeDragging &&
                             std::abs(app.audio.volume - position / 100.f) < .001f,
                         "commit last slider value when mouse capture is interrupted");
        }
        volume(150);
        featureCheck(app.outputPercent == 150, "show requested boosted percentage immediately");
        break;
    case 12: {
        auto gain = app.boost.configuredGain(app.audio.guid);
        featureCheck(!app.gainWorker.busy() && gain &&
                         std::abs(100 * std::pow(10., *gain / 20.) - 150) < .001,
                     "commit real scratch gain from slider");
        featureCommand(DisableBoost);
        break;
    }
    case 13:
        featureCheck(app.boost.configuredGain(app.audio.guid) == 0 && app.outputPercent == 100,
                     "disable gain and synchronize displayed percentage");
        featureCombo(BassAmount, 2);
        featureCommand(BassApply);
        break;
    case 14:
        featureCheck(fg::OutputBass::configured(app.boost.directory(), app.audio.guid) == 6,
                     "apply bass through audio controls");
        featureCommand(AudioMicView);
        SendMessageW(app.controls[MicNoise], BM_SETCHECK, BST_UNCHECKED, 0);
        featureCommand(MicNoise);
        featureCombo(MicEQPreset, 4);
        app.pendingMicrophoneApply = L"diagnostic-mic";
        SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(MicHigh, CBN_DROPDOWN), 0);
        SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(MicHigh, CBN_CLOSEUP), 0);
        featureCheck(!app.pendingMicrophoneApply.empty(),
                     "browsing microphone choices preserves pending Apply");
        featureCombo(MicHigh, 4);
        featureCheck(app.pendingMicrophoneApply.empty(),
                     "editing microphone cancels stale download-time Apply");
        featureCommand(MicApply);
        break;
    case 15:
        featureCheck(fg::MicrophoneEffects::equalizerConfigured(app.boost.directory(),
                                                                app.microphones.front().guid) &&
                         !fg::MicrophoneEffects::configured(app.boost.directory(),
                                                            app.microphones.front().guid),
                     "apply EQ-only mic configuration without RNNoise");
        featureCheck(fg::OutputBass::configured(app.boost.directory(), app.audio.guid) == 6,
                     "microphone apply preserves bass include");
        for (int i = 0; i < 20; ++i)
            featureCommand(MicApply);
        featureCheck(app.audioNotice.find(L"busy") == std::wstring::npos,
                     "coalesce rapid mic requests without filling audio queue");
        featureCommand(MicDisable);
        featureCommand(BassDisable);
        break;
    case 16: {
        featureCheck(!fg::MicrophoneEffects::equalizerConfigured(app.boost.directory(),
                                                                 app.microphones.front().guid) &&
                         !fg::OutputBass::configured(app.boost.directory(), app.audio.guid),
                     "disable microphone and bass while preserving gain config");
        auto previous = app.boost.directory();
        app.boost.directory((fs::path(previous) / L"missing").wstring());
        volume(200);
        featureCheck(app.outputPercent == 100, "clamp boost without APO config and keep UI usable");
        app.boost.directory(previous);
        featureCommand(402);
        app.featureAnimationFrames = app.animationFrames;
        featureCheck(!IsWindowVisible(app.window), "hide application through tray command");
        break;
    }
    case 17:
        featureCheck(app.animationFrames - app.featureAnimationFrames <= 2,
                     "pause animated rendering while hidden");
        featureCommand(401);
        featureCheck(IsWindowVisible(app.window), "restore application through tray command");
        featureCommand(405);
        featureCheck(app.page == 5, "open Overlay Studio through tray command");
        SendMessageW(app.controls[Motion], BM_SETCHECK, BST_UNCHECKED, 0);
        featureCommand(Motion);
        featureCheck(!app.motion, "turn off motion effects");
        SendMessageW(app.controls[Motion], BM_SETCHECK, BST_CHECKED, 0);
        featureCommand(Motion);
        break;
    case 18:
        ShowWindow(app.window, SW_MINIMIZE);
        app.featureAnimationFrames = app.animationFrames;
        featureCheck(IsIconic(app.window), "minimize main window");
        break;
    case 19:
        featureCheck(app.animationFrames - app.featureAnimationFrames <= 2,
                     "pause animated rendering while minimized");
        ShowWindow(app.window, SW_RESTORE);
        featureCheck(!IsIconic(app.window), "restore minimized main window");
        break;
    case 20:
        featureCheck(app.animationFrames > app.featureAnimationFrames + 2,
                     "resume motion after restoring");
        featureCommand(GamingTab);
        featureCommand(Export);
        {
            std::error_code error;
            auto size = fs::file_size(
                fs::path(app.diagnosticPath).parent_path() / L"feature-export.csv", error);
            featureCheck(!error && size > 100,
                         "export current rolling capture after UI state changes");
        }
        break;
    case 21:
        featureCheck(createDesktopShortcut().empty(),
                     "create actual desktop shortcut on isolated runner");
        featureCheck(fs::exists(fs::path(fg::executableDir()) / L"README.txt"),
                     "package includes quick-guide target");
        featureCheck(fs::exists(fs::path(fg::executableDir()) / L"assets" / L"artwork.png"),
                     "package includes artwork");
        break;
    case 22:
        if (app.capture.running())
            featureCommand(Capture);
        featureCheck(!app.capture.running(), "stop capture through UI");
        {
            auto snapshot = app.capture.snapshot(app.demoPid);
            featureCheck(!snapshot.live && !snapshot.frames.empty(),
                         "retain stopped rolling data without reporting live FPS");
        }
        featureCommand(Capture);
        featureCheck(app.capture.running(), "restart capture through UI");
        break;
    case 23:
        featureCheck(app.stats.live && !app.stats.frames.empty(),
                     "receive real frames after capture restart");
        featureCheck(app.peakTransitionBytes > 0, "exercise cropped page-transition snapshots");
        featureCheck(app.peakTransitionBytes < app.matchingFullTransitionBytes,
                     "transition snapshots avoid full-window bitmap storage");
        break;
    }
    app.featureRunning = false;
}
