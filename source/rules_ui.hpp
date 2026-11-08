// Native, keyboard-accessible process-rule editor. Included inside the app namespace.
struct RulesDialogData {
    float scale = 1;
    UINT dpi = 96;
    HFONT font = nullptr;
    HBRUSH field = CreateSolidBrush(RGB(16, 24, 31));
    std::wstring error;
    int errorField = 0;
};
void scaleRuleControls(HWND window, RulesDialogData* data, float ratio) {
    HFONT previous = data->font;
    data->font = CreateFontW(-std::max(10, int(13 * data->scale)), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                             FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    struct Layout {
        float ratio;
        HFONT font;
    } layout{ratio, data->font};
    EnumChildWindows(
        window,
        [](HWND child, LPARAM value) -> BOOL {
            auto layout = reinterpret_cast<Layout*>(value);
            RECT r{};
            GetWindowRect(child, &r);
            MapWindowPoints(nullptr, GetParent(child), reinterpret_cast<POINT*>(&r), 2);
            SetWindowPos(child, nullptr, int(r.left * layout->ratio), int(r.top * layout->ratio),
                         int((r.right - r.left) * layout->ratio),
                         int((r.bottom - r.top) * layout->ratio), SWP_NOZORDER | SWP_NOACTIVATE);
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(layout->font), TRUE);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&layout));
    if (previous)
        DeleteObject(previous);
}
LRESULT CALLBACK ruleEditProc(HWND w, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL) {
        SendMessageW(w, EM_LINESCROLL, 0, -GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * 3);
        InvalidateRect(GetParent(w), nullptr, FALSE);
        return 0;
    }
    auto result = DefSubclassProc(w, msg, wp, lp);
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_KEYUP || msg == WM_VSCROLL)
        InvalidateRect(GetParent(w), nullptr, FALSE);
    return result;
}
int ruleCount(HWND edit) {
    std::wistringstream lines(editText(edit));
    int count = 0;
    std::wstring line;
    while (std::getline(lines, line))
        count += line.find_first_not_of(L" \t\r") != std::wstring::npos;
    return count;
}
void paintRules(HWND window, HDC dc, RulesDialogData* data) {
    RECT client{};
    GetClientRect(window, &client);
    Bitmap buffer(std::max(1L, client.right), std::max(1L, client.bottom), PixelFormat32bppPARGB);
    {
        Graphics g(&buffer);
        g.Clear(bg);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.ScaleTransform(data->scale, data->scale);
        label(g, L"PROCESS CONTROL", 26, 22, 540, 19, 10, mint, true);
        label(g, L"Your games. Your rules.", 23, 46, 610, 38, 25, text, true);
        label(g, L"Choose which games can use optional hooks and auto borderless.", 26, 90, 610, 24,
              13, muted);
        for (int id = 1; id <= 2; ++id) {
            float x = id == 1 ? 24.f : 340.f;
            rounded(g, RectF(x, 130, 296, 282), card, 14, true);
            rounded(g, RectF(x + 16, 150, 4, 19), id == 1 ? mint : amber, 2);
            label(g, id == 1 ? L"Allow list" : L"Block list", x + 30, 146, 245, 28, 17, text, true);
            label(g,
                  id == 1 ? L"Hook eligibility · auto borderless"
                          : L"Hidden targets · changes prevented",
                  x + 16, 178, 264, 22, 11, muted);
            rounded(g, RectF(x + 16, 207, 264, 164), Color(255, 16, 24, 31), 8, true);
            bool focused = GetFocus() == GetDlgItem(window, id);
            if (focused || data->errorField == id) {
                Color color = data->errorField == id ? amber : mint;
                line(g, x + 28, 370, x + 268, 370, color, 1.5f);
            }
            HWND edit = GetDlgItem(window, id);
            int count = ruleCount(edit);
            label(g, std::to_wstring(count) + (count == 1 ? L" rule" : L" rules"), x + 17, 384, 250,
                  19, 11, muted);
            int lines = int(SendMessageW(edit, EM_GETLINECOUNT, 0, 0));
            int first = int(SendMessageW(edit, EM_GETFIRSTVISIBLELINE, 0, 0));
            HDC editDC = GetDC(edit);
            auto old = SelectObject(editDC, data->font);
            TEXTMETRICW metric{};
            GetTextMetricsW(editDC, &metric);
            SelectObject(editDC, old);
            ReleaseDC(edit, editDC);
            RECT area{};
            GetClientRect(edit, &area);
            int visible = std::max(1L, area.bottom / std::max(1L, metric.tmHeight));
            if (lines > visible) {
                float height = std::max(18.f, 142.f * visible / lines);
                float y =
                    218 + (142 - height) *
                              std::clamp(float(first) / std::max(1, lines - visible), 0.f, 1.f);
                rounded(g, RectF(x + 272, 218, 3, 142), edge, 1.5f);
                rounded(g, RectF(x + 272, y, 3, height), focused ? mint : muted, 1.5f);
            }
        }
        if (!data->error.empty()) {
            label(g, data->error, 26, 429, 608, 47, 12, amber, false, true);
        } else {
            label(g, L"One .exe name per line. Wildcards * and ? work too. Scroll to see more.", 26,
                  429, 608, 23, 12, text);
            label(g,
                  L"System and known anti-cheat processes stay blocked. Allowing a game is not "
                  L"anti-cheat approval.",
                  26, 456, 608, 36, 11, muted, false, true);
        }
        line(g, 24, 500, 636, 500);
        label(g, L"Block rules always take priority.", 26, 522, 330, 24, 11, muted);
    }
    Graphics target(dc);
    target.DrawImage(&buffer, 0, 0);
}
LRESULT CALLBACK rulesProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    auto data = reinterpret_cast<RulesDialogData*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        data = new RulesDialogData;
        data->scale = app.scale;
        data->dpi = GetDpiForWindow(window);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
        for (int id = 1; id <= 2; ++id) {
            HWND edit = CreateWindowExW(
                0, L"EDIT", rulesText(id == 1 ? app.rules.allow : app.rules.deny).c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                id == 1 ? 50 : 366, 218, 238, 142, window, reinterpret_cast<HMENU>(INT_PTR(id)),
                app.instance, nullptr);
            SendMessageW(edit, EM_SETLIMITTEXT, 65535, 0);
            SetWindowSubclass(edit, ruleEditProc, 1, 0);
        }
        CreateWindowW(L"BUTTON", L"Save rules", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                      390, 514, 116, 38, window, reinterpret_cast<HMENU>(3), app.instance, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 520,
                      514, 116, 38, window, reinterpret_cast<HMENU>(4), app.instance, nullptr);
        scaleRuleControls(window, data, data->scale);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window, &ps);
        if (data)
            paintRules(window, dc, data);
        EndPaint(window, &ps);
        return 0;
    }
    case WM_PRINTCLIENT:
        if (data)
            paintRules(window, reinterpret_cast<HDC>(wp), data);
        return 0;
    case WM_CTLCOLOREDIT:
        SetTextColor(reinterpret_cast<HDC>(wp), RGB(233, 242, 246));
        SetBkColor(reinterpret_cast<HDC>(wp), RGB(16, 24, 31));
        return reinterpret_cast<LRESULT>(data ? data->field : app.brush);
    case WM_DRAWITEM: {
        auto d = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (d->CtlType != ODT_BUTTON)
            break;
        Graphics g(d->hDC);
        g.Clear(bg);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        float w = float(d->rcItem.right), h = float(d->rcItem.bottom);
        bool primary = d->CtlID == 3, pressed = d->itemState & ODS_SELECTED;
        rounded(g, RectF(0, 0, w - 1, h - 1),
                pressed ? Color(255, 62, 118, 99) : (primary ? mint : card), 8, !primary);
        wchar_t title[80]{};
        GetWindowTextW(d->hwndItem, title, 80);
        SolidBrush b(primary ? bg : text);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(title, -1, uiFont(std::max(10.f, 13 * (data ? data->scale : 1)), true),
                     RectF(0, 0, w, h), &format, &b);
        if (d->itemState & ODS_FOCUS) {
            Pen p(primary ? bg : mint, 1);
            g.DrawRectangle(&p, 4.f, 4.f, w - 9, h - 9);
        }
        return TRUE;
    }
    case WM_COMMAND:
        if (HIWORD(wp) == EN_CHANGE || HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == EN_KILLFOCUS) {
            if (data && HIWORD(wp) == EN_CHANGE) {
                data->error.clear();
                data->errorField = 0;
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if ((LOWORD(wp) == IDCANCEL && HIWORD(wp) == 0 && lp == 0) || LOWORD(wp) == 4) {
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wp) == 3) {
            fg::Rules rules;
            rules.allow.clear();
            rules.deny.clear();
            for (int id = 1; id <= 2; ++id) {
                std::wistringstream lines(editText(GetDlgItem(window, id)));
                std::wstring line;
                int number = 0;
                while (std::getline(lines, line)) {
                    ++number;
                    size_t start = line.find_first_not_of(L" \t\r"),
                           end = line.find_last_not_of(L" \t\r");
                    if (start == std::wstring::npos)
                        continue;
                    auto rule = fg::utf8(line.substr(start, end - start + 1));
                    if (!fg::validRule(rule)) {
                        data->error = (id == 1 ? L"Allow list" : L"Block list") +
                                      std::wstring(L", line ") + std::to_wstring(number) +
                                      L": enter an .exe name, such as game.exe or game*.exe. File "
                                      L"paths are not accepted.";
                        data->errorField = id;
                        SetFocus(GetDlgItem(window, id));
                        InvalidateRect(window, nullptr, FALSE);
                        return 0;
                    }
                    (id == 1 ? rules.allow : rules.deny).push_back(rule);
                }
            }
            app.rules = rules;
            app.capture.setRules(rules);
            app.borderless.restoreBlocked(rules);
            app.cachedForegroundPid = 0;
            app.cachedForegroundWindow = nullptr;
            auto processes = fg::processes();
            if (app.autoPid &&
                (!processes.count(app.autoPid) || rules.blocked(processes[app.autoPid]))) {
                app.autoPid = 0;
                app.autoWindow = nullptr;
            }
            if (app.hook.targetAlive() && (app.hook.active() || app.hook.status() == 0)) {
                if (!processes.count(app.hook.pid()) ||
                    !rules.hookAllowed(processes[app.hook.pid()]))
                    app.hook.disable();
            }
            bool persisted = saveSettings();
            refreshTargets();
            setNotice(
                persisted
                    ? L"Game rules saved. No hook is enabled automatically."
                    : L"Game rules updated for this session. Preferences could not be saved.");
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_DPICHANGED:
        if (data) {
            auto rect = reinterpret_cast<RECT*>(lp);
            MONITORINFO monitor{sizeof(monitor)};
            GetMonitorInfoW(MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST), &monitor);
            int availableW = monitor.rcWork.right - monitor.rcWork.left;
            int availableH = monitor.rcWork.bottom - monitor.rcWork.top;
            RECT frame{};
            AdjustWindowRectExForDpi(&frame, DWORD(GetWindowLongPtrW(window, GWL_STYLE)), FALSE,
                                     DWORD(GetWindowLongPtrW(window, GWL_EXSTYLE)), HIWORD(wp));
            float next = std::min(data->scale * float(HIWORD(wp)) / std::max(1u, data->dpi),
                                  std::min((availableW - (frame.right - frame.left) - 16) / 660.f,
                                           (availableH - (frame.bottom - frame.top) - 16) / 574.f));
            next = std::max(.25f, next);
            float ratio = next / data->scale;
            data->dpi = HIWORD(wp);
            data->scale = next;
            int width = int(660 * next) + frame.right - frame.left;
            int height = int(574 * next) + frame.bottom - frame.top;
            int x = std::clamp(rect->left, monitor.rcWork.left,
                               std::max(monitor.rcWork.left, monitor.rcWork.right - width));
            int y = std::clamp(rect->top, monitor.rcWork.top,
                               std::max(monitor.rcWork.top, monitor.rcWork.bottom - height));
            SetWindowPos(window, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
            scaleRuleControls(window, data, ratio);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (!app.shuttingDown) {
            EnableWindow(app.window, TRUE);
            SetForegroundWindow(app.window);
        }
        return 0;
    case WM_NCDESTROY:
        if (data) {
            if (data->font)
                DeleteObject(data->font);
            if (data->field)
                DeleteObject(data->field);
            delete data;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        }
        break;
    }
    return DefWindowProcW(window, msg, wp, lp);
}
void showRules() {
    RECT parent{};
    GetWindowRect(app.window, &parent);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    RECT bounds{0, 0, int(660 * app.scale), int(574 * app.scale)};
    AdjustWindowRectExForDpi(&bounds, style, FALSE, WS_EX_DLGMODALFRAME,
                             GetDpiForWindow(app.window));
    MONITORINFO m{};
    m.cbSize = sizeof(m);
    GetMonitorInfoW(MonitorFromWindow(app.window, MONITOR_DEFAULTTONEAREST), &m);
    int w = bounds.right - bounds.left, h = bounds.bottom - bounds.top;
    int x = std::clamp((parent.left + parent.right - w) / 2, m.rcWork.left,
                       std::max(m.rcWork.left, m.rcWork.right - w));
    int y = std::clamp((parent.top + parent.bottom - h) / 2, m.rcWork.top,
                       std::max(m.rcWork.top, m.rcWork.bottom - h));
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, L"Framoryx.Rules", L"Framoryx — Game rules",
                                  style, x, y, w, h, app.window, nullptr, app.instance, nullptr);
    if (window) {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
        EnableWindow(app.window, FALSE);
        ShowWindow(window, SW_SHOW);
        SetForegroundWindow(window);
        SetFocus(GetDlgItem(window, 1));
    }
}
// Export this application's own client rendering for bounded UI diagnostics.
bool saveRulesSnapshot(HWND window, const std::wstring& path) {
    RECT r{};
    GetClientRect(window, &r);
    HDC screen = GetDC(window), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, r.right, r.bottom);
    auto old = SelectObject(dc, bitmap);
    SendMessageW(window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
    for (int id = 1; id <= 4; ++id) {
        HWND child = GetDlgItem(window, id);
        RECT rect{};
        GetWindowRect(child, &rect);
        MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&rect), 2);
        HDC childDC = CreateCompatibleDC(screen);
        HBITMAP childBitmap =
            CreateCompatibleBitmap(screen, rect.right - rect.left, rect.bottom - rect.top);
        auto previous = SelectObject(childDC, childBitmap);
        SendMessageW(child, WM_PRINT, reinterpret_cast<WPARAM>(childDC),
                     PRF_CLIENT | PRF_ERASEBKGND);
        BitBlt(dc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, childDC, 0,
               0, SRCCOPY);
        SelectObject(childDC, previous);
        DeleteObject(childBitmap);
        DeleteDC(childDC);
    }
    SelectObject(dc, old);
    auto image = std::unique_ptr<Bitmap>(Bitmap::FromHBITMAP(bitmap, nullptr));
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(window, screen);
    UINT count = 0, bytes = 0;
    GetImageEncodersSize(&count, &bytes);
    std::vector<BYTE> info(bytes);
    auto encoders = reinterpret_cast<ImageCodecInfo*>(info.data());
    if (GetImageEncoders(count, bytes, encoders) == Ok)
        for (UINT i = 0; i < count; ++i)
            if (wcscmp(encoders[i].MimeType, L"image/png") == 0)
                return image->Save(path.c_str(), &encoders[i].Clsid, nullptr) == Ok;
    return false;
}
