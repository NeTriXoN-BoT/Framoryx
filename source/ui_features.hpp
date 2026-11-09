// Private presentation implementation, included inside main.cpp's application namespace.
const uint32_t hudFields[]{fg::HudFps, fg::HudAverage, fg::HudLow, fg::HudTime, fg::HudGraph,
                           fg::HudCpu, fg::HudRam, fg::HudCap, fg::HudApi, fg::HudBrand, fg::HudGpu, fg::HudVram, fg::HudGpuTemp, fg::HudCpuTemp, fg::HudPower, fg::HudClock};
const wchar_t* hudTitles[]{L"Live FPS",    L"Average FPS",   L"1% low",     L"Frame time",
                           L"Frame graph", L"System CPU",    L"System RAM", L"Limit target",
                           L"Renderer", L"Framoryx label", L"GPU usage", L"VRAM usage", L"GPU temp", L"CPU temp", L"GPU power", L"GPU clock"};
Color hudAccent(int index) {
    const auto rgb=fg::accentColors[std::clamp(index,0,fg::hudRainbowAccent)];
    return Color(255,BYTE(rgb>>16),BYTE(rgb>>8),BYTE(rgb));
}
void showAppWindow() {
    ShowWindow(app.window, IsIconic(app.window) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(app.window);
}
void updateTray() {
    if (!app.window || app.shuttingDown || app.engineMode)
        return;
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = app.window;
    icon.uID = 1;
    icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    icon.uCallbackMessage = WM_APP + 20;
    icon.hIcon = LoadIconW(app.instance, MAKEINTRESOURCEW(1));
    std::wstring tooltip = L"Framoryx";
    if (app.stats.live)
        tooltip += L" · " + value(app.stats.fps) + L" FPS";
    else
        tooltip += app.capture.running() ? L" · Waiting for game frames" : L" · Ready";
    wcsncpy_s(icon.szTip, tooltip.c_str(), _TRUNCATE);
    if (Shell_NotifyIconW(app.trayRegistered ? NIM_MODIFY : NIM_ADD, &icon))
        app.trayRegistered = true;
}
void updateAnimationState(bool refresh = false) {
    if (!app.window)
        return;
    auto monitor = MonitorFromWindow(app.window, MONITOR_DEFAULTTONEAREST);
    if (refresh || monitor != app.animationMonitor) {
        app.animationMonitor = monitor;
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        app.animationHz = 60;
        if (GetMonitorInfoW(monitor, &info) &&
            EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) &&
            mode.dmDisplayFrequency >= 30)
            app.animationHz = std::clamp(int(mode.dmDisplayFrequency), 30, 144);
    }
    app.animation.configure(app.motion && !app.shuttingDown && !IsIconic(app.window) &&
                                IsWindowVisible(app.window) &&
                                (GetForegroundWindow() == app.window || app.diagnosticAnimate),
                            app.animationHz);
}
std::wstring createDesktopShortcut() {
    PWSTR desktop = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop)))
        return L"Windows could not locate the desktop folder.";
    std::wstring destination = std::wstring(desktop) + L"\\Framoryx.lnk";
    CoTaskMemFree(desktop);
    IShellLinkW* link = nullptr;
    auto result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                   __uuidof(IShellLinkW), reinterpret_cast<void**>(&link));
    if (FAILED(result))
        return L"Windows could not create a desktop shortcut.";
    wchar_t executable[32768]{};
    GetModuleFileNameW(nullptr, executable, 32768);
    if(app.engineMode){const auto frontend=fg::executableDir()+L"\\Framoryx.exe";if(frontend.size()>=32768){link->Release();return L"The application path is too long.";}wcscpy_s(executable,frontend.c_str());}
    auto existingTarget = [&](const std::wstring& path) {
        std::wstring target;
        IShellLinkW* previous = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                       __uuidof(IShellLinkW),
                                       reinterpret_cast<void**>(&previous)))) {
            IPersistFile* data = nullptr;
            if (SUCCEEDED(previous->QueryInterface(__uuidof(IPersistFile),
                                                   reinterpret_cast<void**>(&data)))) {
                if (SUCCEEDED(data->Load(path.c_str(), STGM_READ))) {
                    wchar_t buffer[32768]{};
                    if (SUCCEEDED(previous->GetPath(buffer, 32768, nullptr, SLGP_RAWPATH)))
                        target = buffer;
                }
                data->Release();
            }
            previous->Release();
        }
        return target;
    };
    if (GetFileAttributesW(destination.c_str()) != INVALID_FILE_ATTRIBUTES &&
        _wcsicmp(existingTarget(destination).c_str(), executable) != 0) {
        destination = destination.substr(0, destination.size() - 4) + L" Gaming Toolkit.lnk";
        if (GetFileAttributesW(destination.c_str()) != INVALID_FILE_ATTRIBUTES &&
            _wcsicmp(existingTarget(destination).c_str(), executable) != 0) {
            link->Release();
            return L"Those desktop shortcut names are already used. Existing shortcuts were "
                   L"preserved.";
        }
    }
    link->SetPath(executable);
    link->SetArguments(L"");
    link->SetShowCmd(SW_SHOWNORMAL);
    link->SetWorkingDirectory(fg::executableDir().c_str());
    link->SetDescription(L"Framoryx gaming toolkit");
    link->SetIconLocation((fg::executableDir() + L"\\assets\\Framoryx.ico").c_str(), 0);
    IPersistFile* file = nullptr;
    result = link->QueryInterface(__uuidof(IPersistFile), reinterpret_cast<void**>(&file));
    if (SUCCEEDED(result)) {
        result = file->Save(destination.c_str(), TRUE);
        file->Release();
    }
    link->Release();
    return SUCCEEDED(result) ? L"" : L"The desktop shortcut could not be saved.";
}
Color mixColor(Color from, Color to, double t) {
    t = std::clamp(t, 0., 1.);
    return Color(BYTE(from.GetA() + (to.GetA() - from.GetA()) * t),
                 BYTE(from.GetR() + (to.GetR() - from.GetR()) * t),
                 BYTE(from.GetG() + (to.GetG() - from.GetG()) * t),
                 BYTE(from.GetB() + (to.GetB() - from.GetB()) * t));
}
void selectComboValue(int id, int value) {
    int count = int(SendMessageW(app.controls[id], CB_GETCOUNT, 0, 0)), chosen = 0;
    for (int i = 0; i < count; ++i) {
        wchar_t label[128]{};
        SendMessageW(app.controls[id], CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(label));
        if (int(wcstol(label, nullptr, 10)) == value) {
            chosen = i;
            break;
        }
    }
    SendMessageW(app.controls[id], CB_SETCURSEL, chosen, 0);
}
int comboValue(int id) {
    wchar_t label[128]{};
    int selected = int(SendMessageW(app.controls[id], CB_GETCURSEL, 0, 0));
    if (selected >= 0)
        SendMessageW(app.controls[id], CB_GETLBTEXT, selected, reinterpret_cast<LPARAM>(label));
    return int(wcstol(label, nullptr, 10));
}
void syncHudControls() {
    if (!app.controls.count(HudSize))
        return;
    selectComboValue(HudSize, app.hudOptions.scale);
    selectComboValue(HudOpacity, app.hudOptions.opacity);
    selectComboValue(DesktopSize, app.desktopPercent);
    SendMessageW(app.controls[HudAccent], CB_SETCURSEL, app.hudOptions.accent, 0);
    SendMessageW(app.controls[HudStyle], CB_SETCURSEL, app.hudOptions.style, 0);
    SendMessageW(app.controls[HudMode], CB_SETCURSEL, app.hudMode, 0);
    SendMessageW(app.controls[HudPosition], CB_SETCURSEL, app.corner, 0);
    SendMessageW(app.controls[Corner], CB_SETCURSEL, app.corner, 0);
    for (int i = 0; i < 16; ++i)
        SendMessageW(app.controls[HudFirstField + i], BM_SETCHECK,
                     (app.hudOptions.fields & hudFields[i]) ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(app.controls[HudToggle], app.hudMode == 2 && !app.hook.active()
                                                ? L"Enable in-game"
                                            : app.overlay ? L"Hide overlay"
                                                          : L"Show overlay");
}
void applyProfile(const std::string& exe) {
    app.activeProfile = fg::lower(exe);
    auto found = app.hudProfiles.find(app.activeProfile);
    auto profile = found == app.hudProfiles.end() ? app.hudBase : found->second;
    app.hudOptions = profile.options;
    app.hudText = profile.text;
    app.hudOptions.temperatureColors = app.hudBase.options.temperatureColors;
    app.hudOptions.textGlow = app.hudBase.options.textGlow;
    app.hudText.fontFamily = app.hudBase.text.fontFamily;
    app.corner = profile.corner;
    app.hudMode = profile.mode;
    if(app.engineMode){const auto limit=app.gameLimits.find(app.activeProfile);if(limit!=app.gameLimits.end()){app.cap=limit->second;SetWindowTextW(app.controls[CapValue],std::to_wstring(app.cap).c_str());if(app.hook.pid()==app.selectedPid)app.hook.cap(app.cap);}}
    syncHudControls();
}
void pushHookPresentation() {
    if (!app.hook.pid())
        return;
    auto found = app.hudProfiles.find(fg::lower(app.hook.name()));
    auto profile = found == app.hudProfiles.end() ? app.hudBase : found->second;
    profile.options.temperatureColors = app.hudBase.options.temperatureColors;
    profile.options.textGlow = app.hudBase.options.textGlow;
    profile.text.fontFamily = app.hudBase.text.fontFamily;
    app.hook.hudOptions(profile.options);
    app.hook.hudText(profile.text);
    app.hook.corner(profile.corner);
    app.hook.overlay(app.overlay && profile.mode != 1&&!app.safetyFallback);
}
void storeHudProfile() {
    app.contentDirty = true;
    app.hudOptions.validate();
    fg::HudProfile profile{app.hudOptions, app.corner, app.hudMode, app.hudText};
    if (app.activeProfile.empty())
        app.hudBase = profile;
    else
        app.hudProfiles[app.activeProfile] = profile;
    saveSettings();
    pushHookPresentation();
    syncHudControls();
    positionHud();
    InvalidateRect(app.window, nullptr, FALSE);
}
void applyDesktopScale() {
    if (!app.window)
        return;
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(MonitorFromWindow(app.window, MONITOR_DEFAULTTONEAREST), &info);
    int availableWidth = info.rcWork.right - info.rcWork.left,
        availableHeight = info.rcWork.bottom - info.rcWork.top;
    UINT dpi = GetDpiForWindow(app.window);
    float scale = fg::desktopScale(app.desktopPercent, float(dpi), availableWidth, availableHeight);
    RECT desired{0, 0, int(BW * scale), int(BH * scale)};
    AdjustWindowRectExForDpi(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
    int width = desired.right - desired.left, height = desired.bottom - desired.top;
    app.transitionOld.reset();
    app.transitionNew.reset();
    pageControls(app.page);
    if (IsZoomed(app.window)) {
        layout();
        syncHudControls();
        InvalidateRect(app.window, nullptr, FALSE);
        return;
    }
    SetWindowPos(app.window, nullptr, info.rcWork.left + (availableWidth - width) / 2,
                 info.rcWork.top + (availableHeight - height) / 2, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    syncHudControls();
    InvalidateRect(app.window, nullptr, FALSE);
}
void paintHudContent(Graphics& g, const fg::HudOptions& options, const fg::Stats& stats,
                     bool preview, const fg::HudText& hudText = app.hudText) {
    const float phase = options.accent == fg::hudRainbowAccent
        ? fg::hudRainbowPhase(GetTickCount64()) : 0.f;
    auto drawText = [&](const std::wstring& caption, const RectF& bounds, float size, Color color,
                        bool bold = false, StringAlignment align = StringAlignmentNear,
                        bool temperatureTint = false) {
        StringFormat format;
        format.SetAlignment(align);
        format.SetFormatFlags(StringFormatFlagsNoWrap);
        format.SetTrimming(StringTrimmingEllipsisCharacter);
        const auto* font = hudFont(size, bold, hudText.fontFamily);
        const auto paint = [&](Brush& brush) {
            g.DrawString(caption.c_str(), int(caption.size()), font, bounds, &format, &brush);
        };
        const auto halo = [&](Brush& brush) {
            for (const float offset : {-1.f, 1.f}) {
                auto shifted = bounds;
                shifted.Offset(offset, 0.f);
                g.DrawString(caption.c_str(), int(caption.size()), font, shifted, &format, &brush);
            }
        };
        if (options.accent == fg::hudRainbowAccent && !temperatureTint && !caption.empty()) {
            RectF measured;
            g.MeasureString(caption.c_str(), int(caption.size()), font, bounds, &format, &measured);
            if (measured.Width > 0) {
                std::array<Color, fg::hudRainbowStops.size()> colors;
                std::array<REAL, fg::hudRainbowStops.size()> positions;
                for (size_t index = 0; index < colors.size(); ++index) {
                    positions[index] = REAL(index) / REAL(colors.size() - 1);
                    colors[index] = Color(0xff000000u | fg::hudRainbowColor(positions[index] + phase));
                }
                LinearGradientBrush spectrum(PointF(measured.X, 0),
                    PointF(measured.GetRight(), 0), colors.front(), colors.back());
                if (spectrum.SetInterpolationColors(colors.data(), positions.data(), int(colors.size())) == Ok) {
                    if (options.textGlow) {
                        for (auto& stop : colors) stop = Color(0x40000000u | (stop.GetValue() & 0xffffffu));
                        LinearGradientBrush glow(PointF(measured.X, 0), PointF(measured.GetRight(), 0),
                                                 colors.front(), colors.back());
                        if (glow.SetInterpolationColors(colors.data(), positions.data(), int(colors.size())) == Ok)
                            halo(glow);
                    }
                    paint(spectrum);
                    return;
                }
            }
        }
        if (options.textGlow) {
            SolidBrush glow(Color(0x40000000u | (color.GetValue() & 0xffffffu)));
            halo(glow);
        }
        SolidBrush brush(color);
        paint(brush);
    };
    auto customLabel = [&hudText](size_t index, const wchar_t* fallback) {
        const auto& caption = hudText.labels[index];
        return caption.empty() ? std::wstring(fallback) : fg::wide(caption);
    };
    if (!options.fields) {
        drawText(L"Choose at least one readout", RectF(12, 20, 250, 40), 14, muted);
        return;
    }
    auto accent=hudAccent(options.accent);const auto layout=fg::hudLayout(options);
    const bool compact=options.style==1||options.style==3,minimal=options.style==3;
    if(options.opacity>0)rounded(g,RectF(0,0,layout.width,layout.height),Color(BYTE(255*options.opacity/100),12,19,26),compact?8:14);
    if(!minimal&&options.opacity>0)line(g,14,1,layout.width-14,1,accent,2);
    auto number=[&](uint32_t field)->std::wstring{
        switch(field){
        case fg::HudFps: return (stats.live ? value(stats.fps) : preview ? L"144" : L"—")
            + L" " + customLabel(0, L"FPS");
        case fg::HudAverage:return !stats.frames.empty()?value(stats.average)+L" FPS":preview?L"141 FPS":L"—";
        case fg::HudLow:return stats.frames.size()>=100?value(stats.low)+L" FPS":preview?L"120 FPS":L"—";
        case fg::HudTime:return stats.live?value(stats.ms,2)+L" ms":preview?L"6.94 ms":L"—";
        case fg::HudCpu:return app.cpuReady?value(app.cpu)+L"%":L"—";
        case fg::HudRam:return app.memoryReady?value(app.ramGB,1)+L" GB":L"—";
        case fg::HudCap:return preview?L"144":app.hook.active()&&app.hook.pid()==app.selectedPid?(app.cap?value(app.cap):L"Uncapped"):L"Off";
        case fg::HudApi:return L"Desktop / ETW";
        default:if(field>=fg::HudGpu){int sensor=0;while(sensor<6&&field!=(fg::HudGpu<<sensor))++sensor;if(sensor<6){if(app.sensorValues[sensor]<0)return L"—";return value(app.sensorValues[sensor],sensor==4?1:0)+(sensor<2?L"%":sensor<4?L" °C":sensor==4?L" W":L" MHz");}}return L"FRAMORYX";
        }
    };
    for (size_t n = 0; n < layout.count; ++n) {
        const auto& cell = layout.cells[n];
        if (cell.field == fg::HudGraph) {
            float graphTop = cell.y + 4;
            float graphHeight = cell.height - 10;
            if (!hudText.labels[4].empty()) {
                drawText(customLabel(4, L""), RectF(cell.x, cell.y, cell.width, 14), 10, muted);
                graphTop += 12;
                graphHeight -= 12;
            }
            if (stats.frames.empty() && preview) {
                std::vector<PointF> path;
                path.reserve(110);
                const float amplitude = std::min(1.f, graphHeight / 30.f);
                for (int i = 0; i < 110; ++i) {
                    const float offset = float(8 + std::sin(i * .27) * 3 + (i % 43 == 0 ? 14 : 0));
                    path.emplace_back(cell.x + i * cell.width / 109,
                                      graphTop + graphHeight - offset * amplitude);
                }
                Pen trace(accent, 1.4f);
                g.DrawLines(&trace, path.data(), int(path.size()));
            } else {
                graph(g, RectF(cell.x, graphTop, cell.width, graphHeight), stats, true, accent);
            }
            continue;
        }
        if (cell.field == fg::HudFps) {
            drawText(number(cell.field), RectF(cell.x, cell.y - 3, cell.width, cell.height),
                     compact ? 28 : 34, accent, true);
            continue;
        }
        if (cell.field == fg::HudBrand) {
            drawText(customLabel(9, L"FRAMORYX"), RectF(cell.x, cell.y, cell.width, 18),
                     compact ? 9 : 10, muted, true);
            continue;
        }
        size_t index = 0;
        while (index < std::size(hudFields) && hudFields[index] != cell.field) ++index;
        if (index == std::size(hudFields)) continue;
        const auto title = customLabel(index, hudTitles[index]);
        Color valueColor = text;
        bool temperatureTint = false;
        if (options.temperatureColors && (cell.field == fg::HudGpuTemp || cell.field == fg::HudCpuTemp)) {
            const auto temperature = app.sensorValues[cell.field == fg::HudGpuTemp ? 2 : 3];
            if (const auto rgb = fg::hudTemperatureColor(temperature)) {
                valueColor = Color(255, BYTE(rgb >> 16), BYTE(rgb >> 8), BYTE(rgb));
                temperatureTint = true;
            }
        }
        if (cell.column) {
            drawText(title, RectF(cell.x, cell.y, cell.width - 5, 17), 10, muted);
            drawText(number(cell.field), RectF(cell.x, cell.y + 19, cell.width - 5, 25),
                     cell.field == fg::HudApi ? 11 : 17, valueColor, true, StringAlignmentNear,
                     temperatureTint);
        } else {
            drawText(title, RectF(cell.x, cell.y, cell.width - 110, 20), compact ? 11 : 12, muted);
            drawText(number(cell.field), RectF(cell.x + cell.width - 105, cell.y, 105, 20),
                     cell.field == fg::HudApi ? 10 : compact ? 12 : 13, valueColor, true,
                     StringAlignmentFar, temperatureTint);
        }
    }
}
void paintStudio(Graphics& g) {
    rounded(g, RectF(212, 190, 332, 562), card, 16, true);
    rounded(g, RectF(562, 190, 590, 420), card, 16, true);
    label(g, L"YOUR OVERLAY", 240, 213, 272, 26, 12, mint, true);
    label(g, L"METHOD", 240, 241, 132, 22, 10, muted, true);
    label(g, L"LAYOUT", 388, 241, 132, 22, 10, muted, true);
    label(g, L"SIZE", 240, 314, 132, 20, 10, muted, true);
    label(g, L"OPACITY", 388, 314, 130, 20, 10, muted, true);
    label(g, L"ACCENT", 240, 387, 132, 20, 10, muted, true);
    label(g, L"POSITION", 388, 387, 130, 20, 10, muted, true);
    label(g, L"READOUTS", 240, 460, 276, 22, 11, muted, true);
    label(g, L"LIVE PREVIEW", 588, 215, 260, 25, 11, mint, true);
    label(g, app.stats.live ? L"Current game data" : L"Sample values · layout preview", 854, 215,
          268, 25, 11, muted);
    RectF stage(588, 292, 538, 288);
    LinearGradientBrush gradient(PointF(stage.X, stage.Y),
                                 PointF(stage.GetRight(), stage.GetBottom()),
                                 Color(255, 10, 19, 26), Color(255, 22, 38, 43));
    g.FillRectangle(&gradient, stage);
    g.SetClip(stage);
    for (int i = 0; i < 18; ++i)
        line(g, stage.X + i * 36, stage.Y, stage.X + i * 36, stage.GetBottom(),
             Color(255, 25, 40, 47));
    for (int i = 0; i < 10; ++i)
        line(g, stage.X, stage.Y + i * 36, stage.GetRight(), stage.Y + i * 36,
             Color(255, 25, 40, 47));
    if (app.hudOptions.fields) {
        float preferred = fg::overlayScale(app.hudOptions.scale, 1080);
        float fit = std::min((stage.Width - 48) / fg::hudWidth(app.hudOptions),
                             (stage.Height - 40) / fg::hudHeight(app.hudOptions));
        float scale = std::min(preferred, fit);
        auto state = g.Save();
        float x = stage.X + 24, y = stage.Y + 20;
        if (app.corner % 2)
            x = stage.GetRight() - 24 - fg::hudWidth(app.hudOptions) * scale;
        if (app.corner >= 2)
            y = stage.GetBottom() - 20 - fg::hudHeight(app.hudOptions) * scale;
        g.TranslateTransform(x, y);
        g.ScaleTransform(scale, scale);
        paintHudContent(g, app.hudOptions, app.stats, true);
        g.Restore(state);
    } else
        label(g, L"Switch on a readout to preview your overlay.", stage.X + 50, stage.Y + 130,
              stage.Width - 100, 70, 17, muted, false, true);
    g.ResetClip();
    rounded(g, RectF(562, 628, 590, 124), card, 14, true);
    label(g, L"DESKTOP APP SIZE", 588, 647, 520, 23, 11, mint, true);
    label(g,
          L"Effective size: " + value(app.scale * 100) + L"%\n" +
              (app.desktopPercent == 0 ? L"Auto follows your display."
                                       : L"Fitted to your work area."),
          588, 684, 174, 48, 12, muted, false, true);
    auto profile = app.activeProfile.empty()
                       ? L"Default style · used for games without a saved profile"
                       : L"Style for " + fg::wide(app.activeProfile);
    label(g, profile, 214, 777, 938, 35, 12, muted);
}
void paintCompatibility(Graphics& g) {
    rounded(g, RectF(212, 190, 940, 94), card, 14, true);
    label(g, L"YOUR GAME", 232, 206, 500, 20, 11, muted, true);
    rounded(g, RectF(212, 300, 940, 448), card, 16, true);
    auto& state = app.compatibility;
    bool valid = app.selectedPid != 0;
    label(g,
          app.stats.live ? L"FRAMES RECEIVED"
          : valid        ? L"GAME SELECTED"
                         : L"CHOOSE A GAME",
          244, 324, 850, 24, 11, app.stats.live ? mint : muted, true);
    label(g,
          app.stats.live ? L"Your game is reporting frames."
          : valid        ? L"Find the right overlay path."
                         : L"Choose a game to check its setup.",
          241, 356, 850, 47, 27, text, true);
    const wchar_t* names[]{L"PROCESS", L"RENDERING HINT", L"WINDOW", L"OVERLAY PATH"};
    std::wstring values[]{state.architecture, state.runtime, state.window,
                          app.hudMode == 1      ? L"Desktop selected"
                          : app.hudMode == 2    ? L"In-game selected"
                          : app.hook.hudFresh() ? L"In-game HUD active"
                                                : L"Automatic / desktop"};
    for (int i = 0; i < 4; ++i) {
        float x = 244 + (i % 2) * 430, y = 416 + (i / 2) * 72;
        label(g, names[i], x, y, 400, 20, 10, muted, true);
        label(g, values[i], x, y + 23, 400, 30, 17, text, true);
    }
    label(g, state.note, 244, 570, 850, 54, 13, muted, false, true);
    std::wstring status =
        !app.compatibilityGuard.empty() ? L"In-game hooks blocked: " + app.compatibilityGuard
        : !state.otherOverlays.empty()  ? L"Other overlays found: " + state.otherOverlays
                                        : L"Runtime modules are hints. Compatibility is confirmed "
                                          L"by live frames and successful drawing.";
    label(g, status, 244, 630, 850, 35, 11, !app.compatibilityGuard.empty() ? amber : muted, false,
          true);
    label(g, app.notice, 214, 777, 938, 38, 12, muted, false, true);
}
std::unique_ptr<Bitmap> captureScene(bool contentOnly) {
    RECT region{};
    GetClientRect(app.window, &region);
    if (region.right < 1 || region.bottom < 1)
        return {};
    if (contentOnly) {
        region.left =
            std::clamp(LONG(std::floor(app.originX + 198 * app.scale)), 0L, region.right - 1);
        region.top = std::clamp(LONG(std::floor(app.originY)), 0L, region.bottom - 1);
        region.right = std::clamp(LONG(std::ceil(app.originX + BW * app.scale)), region.left + 1,
                                  region.right);
        region.bottom = std::clamp(LONG(std::ceil(app.originY + BH * app.scale)), region.top + 1,
                                   region.bottom);
        app.transitionRegion = region;
    }
    HDC screen = GetDC(app.window), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap =
        CreateCompatibleBitmap(screen, region.right - region.left, region.bottom - region.top);
    if (!screen || !dc || !bitmap) {
        if (bitmap)
            DeleteObject(bitmap);
        if (dc)
            DeleteDC(dc);
        if (screen)
            ReleaseDC(app.window, screen);
        return {};
    }
    auto old = SelectObject(dc, bitmap);
    SetViewportOrgEx(dc, -region.left, -region.top, nullptr);
    paintMain(dc, region);
    for (auto& control : app.controls)
        if (GetWindowLongPtrW(control.second, GWL_STYLE) & WS_VISIBLE) {
            RECT child{};
            GetWindowRect(control.second, &child);
            MapWindowPoints(nullptr, app.window, reinterpret_cast<POINT*>(&child), 2);
            RECT overlap{};
            if (!IntersectRect(&overlap, &child, &region))
                continue;
            HDC childDC = CreateCompatibleDC(screen);
            int width = std::max(1L, child.right - child.left),
                height = std::max(1L, child.bottom - child.top);
            HBITMAP childBitmap = CreateCompatibleBitmap(screen, width, height);
            auto oldChild = SelectObject(childDC, childBitmap);
            SendMessageW(control.second, WM_PRINT, reinterpret_cast<WPARAM>(childDC),
                         PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
            BitBlt(dc, child.left, child.top, width, height, childDC, 0, 0, SRCCOPY);
            SelectObject(childDC, oldChild);
            DeleteObject(childBitmap);
            DeleteDC(childDC);
        }
    SelectObject(dc, old);
    auto output = std::unique_ptr<Bitmap>(Bitmap::FromHBITMAP(bitmap, nullptr));
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(app.window, screen);
    return output && output->GetLastStatus() == Ok ? std::move(output) : nullptr;
}
void paintAnimatedMetrics(Graphics& g) {
    const float positions[]{229, 513, 733, 953}, widths[]{234, 170, 170, 182};
    std::wstring readings[]{app.stats.live ? value(app.metricFps) : L"—",
                            app.stats.frames.empty() ? L"—" : value(app.metricAverage),
                            app.stats.frames.size() < 100 ? L"—" : value(app.metricLow),
                            app.stats.live ? value(app.metricMs, 2) + L" ms" : L"—"};
    SolidBrush background(card);
    for (int i = 0; i < 4; ++i) {
        g.FillRectangle(&background, positions[i], 338.f, widths[i], 52.f);
        label(g, readings[i], positions[i], 338, widths[i], 52, i == 0 ? 44 : 34,
              i == 0 ? mint : text, true);
    }
    g.FillRectangle(&background, 1020.f, 490.f, 116.f, 37.f);
    label(g, app.cpuReady ? value(app.metricCpu) + L"%" : L"—", 1020, 490, 116, 37, 28, text, true);
    rounded(g, RectF(874, 538, 252, 4), edge, 2);
    if (app.metricCpu > 0)
        rounded(g, RectF(874, 538, std::max(4.f, float(app.metricCpu) * 2.52f), 4), mint, 2);
    if (app.graphEndValid) {
        float radius = 3 + float((std::sin(app.animationTime * 2.7) + 1) * 1.5);
        SolidBrush glow(Color(32, 125, 242, 198)), point(mint);
        g.FillEllipse(&glow, app.graphEnd.X - radius, app.graphEnd.Y - radius, radius * 2,
                      radius * 2);
        g.FillEllipse(&point, app.graphEnd.X - 1.5f, app.graphEnd.Y - 1.5f, 3.f, 3.f);
    }
}
void advanceMotion() {
    double now = animationClock(), dt = app.lastAnimation ? now - app.lastAnimation : 1. / 60;
    app.lastAnimation = now;
    bool foreground =
        (GetForegroundWindow() == app.window || app.diagnosticAnimate) && !IsIconic(app.window);
    if (foreground)
        app.animationTime += std::clamp(dt, 0., .05);
    if (app.transitionNew && (!app.motion || now - app.transitionStart >= .22 || !foreground)) {
        app.transitionOld.reset();
        app.transitionNew.reset();
        pageControls(app.page);
        ++app.transitionsCompleted;
        InvalidateRect(app.window, nullptr, FALSE);
    }
    if (!foreground) {
        app.metricFps = app.stats.fps;
        app.metricAverage = app.stats.average;
        app.metricLow = app.stats.low;
        app.metricMs = app.stats.ms;
        app.metricCpu = app.cpu;
        return;
    }
    for (auto& motion : app.hoverMotion) {
        int id = motion.first;
        double target = app.hoverId == id ? 1 : 0;
        double old = app.hoverMotion[id];
        app.hoverMotion[id] = app.motion ? fg::approach(old, target, dt) : target;
        if (foreground && std::abs(old - app.hoverMotion[id]) > .001)
            InvalidateRect(app.controls[id], nullptr, FALSE);
    }
    for (auto& target : app.toggleTargets) {
        double before = app.toggleMotion[target.first];
        app.toggleMotion[target.first] = app.motion
                                             ? fg::approach(before, target.second ? 1 : 0, dt, 22)
                                             : (target.second ? 1 : 0);
        if (std::abs(before - app.toggleMotion[target.first]) > .001)
            InvalidateRect(app.controls[target.first], nullptr, FALSE);
    }
    app.metricFps = app.motion ? fg::approach(app.metricFps, app.stats.fps, dt, 14) : app.stats.fps;
    app.metricAverage =
        app.motion ? fg::approach(app.metricAverage, app.stats.average, dt, 14) : app.stats.average;
    app.metricLow = app.motion ? fg::approach(app.metricLow, app.stats.low, dt, 14) : app.stats.low;
    app.metricMs = app.motion ? fg::approach(app.metricMs, app.stats.ms, dt, 14) : app.stats.ms;
    app.metricCpu = app.motion ? fg::approach(app.metricCpu, app.cpu, dt, 12) : app.cpu;
    if (foreground && app.motion) {
        RECT rect{int(app.originX), int(app.originY), int(app.originX + BW * app.scale),
                  int(app.originY + (app.transitionNew || app.page == 0 ? BH : 178) * app.scale) +
                      1};
        InvalidateRect(app.window, &rect, FALSE);
    }
}
