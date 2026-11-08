#include "capture.hpp"
#include "audio.hpp"
#include "compatibility.hpp"
#include "animation.hpp"
#include <shlobj.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>

using namespace Gdiplus;
namespace fs = std::filesystem;
namespace {
constexpr int BW = 1180, BH = 820;
const Color bg(255, 12, 17, 23), card(255, 20, 28, 36), edge(255, 37, 50, 61),
    mint(255, 125, 242, 198), text(255, 233, 242, 246), muted(255, 142, 163, 178),
    amber(255, 247, 190, 111);
enum Id {
    Capture = 101,
    Target,
    Refresh,
    Overlay,
    Corner,
    Rules,
    Export,
    CapValue,
    Preset,
    Attach,
    Disable,
    Monitor,
    Borderless,
    Restore,
    AutoBorderless,
    Test,
    Help,
    HookHud,
    CaptureDetails,
    OverlayTab = 240,
    CompatibilityTab,
    HudSize,
    HudOpacity,
    HudAccent,
    HudMode,
    HudPosition,
    DesktopSize,
    HudReset,
    CompatibilityRefresh,
    CompatibilityBorderless,
    HudToggle,
    CreateShortcut,
    HudStyle,
    HudFirstField = 270
};
enum AudioId {
    GamingTab = 201,
    AudioTab,
    LimiterTab,
    BorderlessTab,
    Motion,
    FullGuide,
    Volume = 210,
    BoostLevel,
    ApplyBoost,
    DisableBoost,
    AudioSetup,
    AudioRefresh,
    AudioFolder,
    SoundSettings
};
struct TargetWindow {
    HWND hwnd;
    DWORD pid;
    std::string exe;
    std::wstring title;
};
struct MonitorInfo {
    HMONITOR handle;
    RECT rect;
    std::wstring name;
};
struct App {
    HINSTANCE instance{};
    HWND window{}, hud{};
    float scale = 1;
    std::map<int, HWND> controls;
    std::vector<TargetWindow> targets;
    std::vector<MonitorInfo> monitors;
    fg::Capture capture;
    fg::HookClient hook;
    fg::Borderless borderless;
    fg::Rules rules;
    fg::Stats stats;
    DWORD autoPid = 0, selectedPid = 0;
    HWND autoWindow{};
    fg::Store hookHistory;
    LONGLONG hookCursor = 0;
    bool hookSource = false;
    bool overlay = false, autoBorderless = false, hookHud = true;
    int corner = 0, cap = 60;
    fg::HudOptions hudOptions;
    fg::HudProfile hudBase;
    std::map<std::string, fg::HudProfile> hudProfiles;
    std::string activeProfile, autoExe;
    int hudMode = 0, desktopPercent = 0;
    fg::Compatibility compatibility;
    DWORD compatibilityPid = 0;
    std::wstring compatibilityGuard;
    int hudWidth = 260, hudHeight = 180;
    float hudRenderScale = 1;
    double animationTime = 0, lastAnimation = 0, metricFps = 0, metricAverage = 0, metricLow = 0,
           metricMs = 0, metricCpu = 0;
    std::map<int, double> hoverMotion, toggleMotion;
    std::map<int, bool> toggleTargets;
    std::unique_ptr<Bitmap> transitionOld, transitionNew;
    double transitionStart = 0;
    bool capturingScene = false;
    ULONGLONG transitionsCompleted = 0;

    std::wstring notice = L"Choose a game, or leave Auto selected. Press Start capture to begin.";
    std::wstring observedCaptureMessage;
    ULONGLONG noticeUntil = 0;
    std::wstring blocker;
    double cpu = 0, ramGB = 0, totalGB = 0;
    ULONGLONG oldIdle = 0, oldKernel = 0, oldUser = 0;
    bool cpuReady = false, memoryReady = false;
    DWORD cachedForegroundPid = 0;
    HWND cachedForegroundWindow = nullptr;
    LONGLONG oldHookFrames = 0;
    double oldHookTime = 0, hookFps = 0;
    std::unique_ptr<Image> artwork, logo;
    HFONT controlFont{};
    HBRUSH brush{};
    int tick = 0;
    bool shuttingDown = false;
    bool preferencesSaved = true;
    bool trayRegistered = false;
    UINT trayRestartMessage = 0;
    fg::AnimationPump animation;
    bool animationRunning = false, diagnosticAnimate = false;
    int animationHz = 60;
    HMONITOR animationMonitor = nullptr;
    ULONGLONG animationFrames = 0;
    bool motion = true, lastCaptureRunning = false;
    HDC bufferDC{};
    HDC contentDC{};
    HBITMAP contentBitmap{}, contentOriginal{};
    bool contentDirty = true;
    PointF graphEnd{};
    bool graphEndValid = false;
    HBITMAP bufferBitmap{}, bufferOriginal{};
    int bufferWidth = 0, bufferHeight = 0;
    int hoverId = 0;
    int page = 0;
    fg::AudioState audio;
    fg::AudioBoost boost;
    std::unique_ptr<Bitmap> headerCache, logoCache;
    int headerPage = -1;
    bool headerLive = false;
    float headerScale = 0;
    std::map<std::pair<float, bool>, std::unique_ptr<Font>> fonts;
    std::map<std::string, int> gameLimits;
    std::wstring diagnosticPath;
    std::wstring snapshotPath;
    bool snapshotSaved = false;
    std::wstring settingsOverride;
    bool smokeCustomize = false, customizationRoundTrip = false;
    int customizationStep = 0, customizationChecks = 0;
    double smokeSeconds = 0, diagnosticStart = 0, paintMilliseconds = 0, updateMilliseconds = 0,
           messageMilliseconds = 0, maxPaintMs = 0;
    ULONGLONG paintCount = 0, fullPaintCount = 0, foregroundSamples = 0, sampleCount = 0,
              messageCount = 0;
    DWORD peakGdi = 0, initialGdi = 0, peakHandles = 0;
    int fixedSmokePage = -1;
    std::vector<double> cpuSamples;
    ULONGLONG lastVolumeWrite = 0;
    fg::Handle demoProcess, demoJob;
    DWORD demoPid = 0, pendingDemoPid = 0;
    bool smokeDemo = false;
    ULONGLONG demoLiveSamples = 0;
    bool smokeResize = false, smokeDialogOpened = false;
    int smokeResizeStep = -1, smokeOuterWidth = 0, smokeOuterHeight = 0;
    std::wstring audioNotice = L"Normal volume works immediately. Boost above 100% requires "
                               L"Equalizer APO configured for your output device.";
} app;
struct PerfMeter {
    double* total;
    LARGE_INTEGER start{};
    explicit PerfMeter(double& value) : total(app.diagnosticPath.empty() ? nullptr : &value) {
        if (total)
            QueryPerformanceCounter(&start);
    }
    ~PerfMeter() {
        if (total) {
            LARGE_INTEGER end{}, frequency{};
            QueryPerformanceCounter(&end);
            QueryPerformanceFrequency(&frequency);
            *total += 1000.0 * (end.QuadPart - start.QuadPart) / frequency.QuadPart;
        }
    }
};

std::wstring value(double v, int decimals = 0) {
    std::wostringstream out;
    out << std::fixed << std::setprecision(decimals) << v;
    return out.str();
}
std::wstring configPath() {
    if (!app.settingsOverride.empty())
        return app.settingsOverride;
    return fg::executableDir() + L"\\settings.ini";
}
std::string encodeHud(const fg::HudProfile& profile) {
    std::ostringstream out;
    out << profile.options.fields << '|' << profile.options.scale << '|' << profile.options.opacity
        << '|' << profile.options.accent << '|' << profile.corner << '|' << profile.mode << '|'
        << profile.options.style;
    return out.str();
}
std::optional<fg::HudProfile> decodeHud(const std::string& value) {
    std::istringstream in(value);
    std::string part;
    std::vector<int> numbers;
    while (std::getline(in, part, '|')) {
        auto n = fg::number(part);
        if (!n || *n < 0 || *n > 10000 || std::floor(*n) != *n)
            return {};
        numbers.push_back(int(*n));
    }
    if (numbers.size() != 6 && numbers.size() != 7)
        return {};
    fg::HudProfile profile;
    profile.options = {uint32_t(numbers[0]), numbers[1], numbers[2], numbers[3]};
    if (numbers.size() == 7)
        profile.options.style = numbers[6];
    profile.options.validate();
    profile.corner = std::clamp(numbers[4], 0, 3);
    profile.mode = std::clamp(numbers[5], 0, 2);
    return profile;
}
bool saveSettings() {
    std::ostringstream out;
    out << "; Framoryx portable settings. Executable basenames support * and ?. Block rules "
           "win.\ncap="
        << app.cap << "\ncorner=" << app.corner << "\nauto_borderless=" << app.autoBorderless
        << "\n";
    out << "motion=" << app.motion << "\n";
    out << "desktop_scale=" << app.desktopPercent << "\n";
    out << "hud_base=" << encodeHud(app.hudBase) << "\n";
    for (auto& profile : app.hudProfiles)
        out << "hud_profile=" << profile.first << '|' << encodeHud(profile.second) << '\n';
    for (auto& p : app.rules.allow)
        out << "allow=" << p << "\n";
    for (auto& p : app.rules.deny)
        out << "deny=" << p << "\n";
    for (auto& p : app.gameLimits)
        out << "limit=" << p.first << '|' << p.second << '\n';
    out << "apo_config_dir=" << fg::utf8(app.boost.directory()) << '\n';
    auto path = configPath(), temporary = path + L".tmp";
    std::ofstream file{fs::path(temporary), std::ios::binary};
    file << out.str();
    file.close();
    if (!file || !MoveFileExW(temporary.c_str(), path.c_str(),
                              MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        app.notice = L"Settings could not be saved. Move the app to a writable folder.";
        app.preferencesSaved = false;
        return false;
    }
    app.preferencesSaved = true;
    return true;
}
void loadSettings() {
    std::ifstream file{fs::path(configPath())};
    if (!file)
        return;
    app.hudProfiles.clear();
    app.rules.allow.clear();
    app.rules.deny.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;
        auto key = line.substr(0, pos), val = line.substr(pos + 1);
        if (key == "allow" && fg::validRule(val))
            app.rules.allow.push_back(val);
        if (key == "deny" && fg::validRule(val))
            app.rules.deny.push_back(val);
        if (key == "limit") {
            auto delimiter = val.find_last_of('|');
            if (delimiter != std::string::npos) {
                auto exe = val.substr(0, delimiter);
                auto fps = fg::number(val.substr(delimiter + 1));
                if (fg::validRule(exe) && fps && std::floor(*fps) == *fps &&
                    (*fps == 0 || (*fps >= 10 && *fps <= 1000)))
                    app.gameLimits[fg::lower(exe)] = int(*fps);
            }
        }
        if (key == "apo_config_dir" && !val.empty())
            app.boost.directory(fg::wide(val));
        if (key == "hud_base") {
            auto profile = decodeHud(val);
            if (profile)
                app.hudBase = *profile;
        }
        if (key == "hud_profile") {
            auto delimiter = val.find('|');
            if (delimiter != std::string::npos && fg::validRule(val.substr(0, delimiter))) {
                auto profile = decodeHud(val.substr(delimiter + 1));
                if (profile)
                    app.hudProfiles[fg::lower(val.substr(0, delimiter))] = *profile;
            }
        }
        auto n = fg::number(val);
        if (key == "desktop_scale" && n && (*n == 0 || (*n >= 60 && *n <= 175)))
            app.desktopPercent = int(*n);
        if (key == "cap" && n && (*n == 0 || (*n >= 10 && *n <= 1000)))
            app.cap = int(*n);
        if (key == "corner" && n && *n >= 0 && *n <= 3)
            app.corner = app.hudBase.corner = int(*n);
        if (key == "motion")
            app.motion = val != "0";
        if (key == "auto_borderless")
            app.autoBorderless = val == "1";
    }
}
double animationClock() {
    LARGE_INTEGER now{}, frequency{};
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    return double(now.QuadPart) / frequency.QuadPart;
}
void syncHudControls();
void pushHookPresentation();
void applyProfile(const std::string& exe);
void applyDesktopScale();
void paintMain(HDC, const RECT&);
void positionHud();
std::unique_ptr<Bitmap> captureScene();
void rounded(Graphics& g, RectF r, Color color, float radius = 13, bool border = false) {
    GraphicsPath path;
    float d = radius * 2;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    path.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    path.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    path.CloseFigure();
    SolidBrush b(color);
    g.FillPath(&b, &path);
    if (border) {
        Pen p(edge, 1);
        g.DrawPath(&p, &path);
    }
}
Font* uiFont(float size, bool bold = false) {
    size = std::round(size * 2) / 2;
    auto& font = app.fonts[{size, bold}];
    if (!font)
        font = std::make_unique<Font>(L"Segoe UI", size, bold ? FontStyleBold : FontStyleRegular,
                                      UnitPixel);
    return font.get();
}
void label(Graphics& g, const std::wstring& s, float x, float y, float w, float h, float size = 13,
           Color c = text, bool bold = false, bool wrap = false) {
    SolidBrush brush(c);
    StringFormat format;
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    if (!wrap)
        format.SetFormatFlags(StringFormatFlagsNoWrap);
    g.DrawString(s.c_str(), int(s.size()), uiFont(size, bold), RectF(x, y, w, h), &format, &brush);
}
void line(Graphics& g, float x, float y, float x2, float y2, Color c = edge, float width = 1) {
    Pen p(c, width);
    g.DrawLine(&p, x, y, x2, y2);
}
void tile(Graphics& g, float x, float w, const std::wstring& title, const std::wstring& val,
          const std::wstring& sub, bool big = false) {
    rounded(g, RectF(x, 300, w, 112), card, 14, true);
    label(g, title, x + 20, 317, w - 40, 21, 11, muted, true);
    label(g, val, x + 17, 338, w - 34, 52, big ? 44 : 34, big ? mint : text, true);
    label(g, sub, x + 20, 391, w - 40, 18, 10, muted);
}
void graph(Graphics& g, RectF r, const fg::Stats& s, bool hud = false, Color accent = mint) {
    if (!hud)
        app.graphEndValid = false;
    if (s.frames.empty()) {
        label(g, hud ? L"Waiting for frames" : L"Your game’s frame times will appear here.",
              r.X + 15, r.Y + r.Height / 2 - 15, r.Width - 30, 28, hud ? 11 : 14, muted);
        return;
    }
    double maximum = 33.34;
    for (auto& f : s.frames)
        maximum = std::max(maximum, std::min(120.0, f.ms * 1.15));
    double latest = s.frames.back().time;
    for (int i = 0; i <= 3; ++i) {
        float y = r.Y + r.Height * i / 3;
        line(g, r.X, y, r.GetRight(), y, Color(255, 31, 43, 53));
        if (!hud)
            label(g, value(maximum * (3 - i) / 3, 1), r.GetRight() + 7, y - 7, 43, 17, 10, muted);
    }
    // Pixel-width min/max buckets retain stutter spikes without drawing tens of thousands of
    // points.
    const int buckets = std::max(1, int(r.Width));
    std::vector<double> low(buckets, 1e9), high(buckets, -1), sum(buckets, 0);
    std::vector<int> count(buckets, 0);
    for (auto& f : s.frames) {
        int x = int((f.time - (latest - 30.0)) / 30.0 * (buckets - 1));
        if (x < 0 || x >= buckets)
            continue;
        low[x] = std::min(low[x], f.ms);
        high[x] = std::max(high[x], f.ms);
        sum[x] += f.ms;
        ++count[x];
    }
    std::vector<PointF> points;
    for (int i = 0; i < buckets; ++i)
        if (count[i]) {
            float x = r.X + i, y = r.GetBottom() -
                                   float(std::min(maximum, sum[i] / count[i]) / maximum) * r.Height;
            points.emplace_back(x, y);
            if (high[i] - low[i] > 1.0)
                line(g, x, r.GetBottom() - float(std::min(maximum, low[i]) / maximum) * r.Height, x,
                     r.GetBottom() - float(std::min(maximum, high[i]) / maximum) * r.Height,
                     Color(130, accent.GetR(), accent.GetG(), accent.GetB()));
        }
    if (points.size() > 1) {
        if (!hud) {
            app.graphEnd = points.back();
            app.graphEndValid = s.live;
        }
        Pen glow(Color(32, accent.GetR(), accent.GetG(), accent.GetB()), 6), p(accent, 1.5f);
        g.DrawLines(&glow, points.data(), int(points.size()));
        g.DrawLines(&p, points.data(), int(points.size()));
    }
}
TargetWindow* selected() {
    auto it = app.controls.find(Target);
    if (it == app.controls.end())
        return nullptr;
    int i = int(SendMessageW(it->second, CB_GETCURSEL, 0, 0));
    return i > 0 && size_t(i - 1) < app.targets.size() ? &app.targets[i - 1] : nullptr;
}
RECT selectedMonitor(HWND w) {
    int i = int(SendMessageW(app.controls[Monitor], CB_GETCURSEL, 0, 0));
    if (i > 0 && size_t(i - 1) < app.monitors.size())
        return app.monitors[i - 1].rect;
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi);
    return mi.rcMonitor;
}
void refreshTargets() {
    HWND previous = selected() ? selected()->hwnd : nullptr;
    app.targets.clear();
    auto procs = fg::processes();
    struct Context {
        std::map<DWORD, std::string>* procs;
    };
    Context context{&procs};
    EnumWindows(
        [](HWND w, LPARAM p) -> BOOL {
            auto ctx = reinterpret_cast<Context*>(p);
            if (!IsWindowVisible(w) || GetWindow(w, GW_OWNER) || w == app.window || w == app.hud)
                return TRUE;
            wchar_t title[512]{};
            GetWindowTextW(w, title, 512);
            if (!title[0])
                return TRUE;
            RECT r{};
            GetWindowRect(w, &r);
            if (r.right - r.left < 100 || r.bottom - r.top < 80)
                return TRUE;
            DWORD pid = 0;
            GetWindowThreadProcessId(w, &pid);
            auto i = ctx->procs->find(pid);
            if (i == ctx->procs->end() || app.rules.blocked(i->second))
                return TRUE;
            app.targets.push_back({w, pid, i->second, title});
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&context));
    std::sort(app.targets.begin(), app.targets.end(),
              [](auto& a, auto& b) { return fg::lower(a.exe) < fg::lower(b.exe); });
    SendMessageW(app.controls[Target], CB_RESETCONTENT, 0, 0);
    SendMessageW(app.controls[Target], CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"Auto — follow the foreground game"));
    int restore = 0;
    for (size_t i = 0; i < app.targets.size(); ++i) {
        auto& t = app.targets[i];
        std::wstring title =
            fg::wide(t.exe) + L"  —  " + t.title + L"  (#" + std::to_wstring(t.pid) + L")";
        SendMessageW(app.controls[Target], CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(title.c_str()));
        if (t.hwnd == previous)
            restore = int(i + 1);
    }
    SendMessageW(app.controls[Target], CB_SETCURSEL, restore, 0);
}
void refreshMonitors() {
    app.monitors.clear();
    EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR m, HDC, LPRECT, LPARAM) -> BOOL {
            MONITORINFOEXW mi{};
            mi.cbSize = sizeof(mi);
            GetMonitorInfoW(m, &mi);
            std::wstring name = L"Display " + std::to_wstring(app.monitors.size() + 1) + L"  ·  " +
                                std::to_wstring(mi.rcMonitor.right - mi.rcMonitor.left) + L" × " +
                                std::to_wstring(mi.rcMonitor.bottom - mi.rcMonitor.top);
            app.monitors.push_back({m, mi.rcMonitor, name});
            return TRUE;
        },
        0);
    SendMessageW(app.controls[Monitor], CB_RESETCONTENT, 0, 0);
    SendMessageW(app.controls[Monitor], CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"Game’s current display"));
    for (auto& m : app.monitors)
        SendMessageW(app.controls[Monitor], CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(m.name.c_str()));
    SendMessageW(app.controls[Monitor], CB_SETCURSEL, 0, 0);
}
void setNotice(const std::wstring& message) {
    app.contentDirty = true;
    app.notice = message;
    app.noticeUntil = GetTickCount64() + 7000;
    InvalidateRect(app.window, nullptr, FALSE);
}
void makeBorderless() {
    auto* t = selected();
    if (!t) {
        setNotice(L"Select a game window from the list first. Auto mode only monitors.");
        return;
    }
    if (app.rules.blocked(t->exe)) {
        setNotice(L"This process is blocked by your rules.");
        return;
    }
    auto err = app.borderless.apply(t->hwnd, selectedMonitor(t->hwnd));
    setNotice(
        err.empty()
            ? L"Borderless applied. Use Restore window to return to its original size and borders."
            : err);
}
bool updateCap(bool persist = true) {
    wchar_t number[20]{};
    GetWindowTextW(app.controls[CapValue], number, 20);
    auto n = fg::number(fg::utf8(number));
    if (!n || std::floor(*n) != *n || (*n != 0 && (*n < 10 || *n > 1000))) {
        setNotice(L"Enter a whole FPS limit from 10 to 1000, or 0 for uncapped.");
        return false;
    }
    app.cap = int(*n);
    if (app.selectedPid == app.hook.pid())
        app.hook.cap(app.cap);
    if (selected())
        app.gameLimits[fg::lower(selected()->exe)] = app.cap;
    if (persist)
        saveSettings();
    return true;
}
void attachHook() {
    auto* t = selected();
    if (!t) {
        setNotice(L"Select a specific game window before enabling the experimental limiter.");
        return;
    }
    if (!updateCap())
        return;
    if (app.rules.blocked(t->exe)) {
        setNotice(L"This game is blocked by Game rules. Block rules always take priority.");
        return;
    }
    auto blocker = fg::antiCheatRunning();
    if (!blocker.empty()) {
        setNotice(L"Hook blocked by an active anti-cheat or inspection error: " + blocker +
                  L". Normal ETW monitoring remains available.");
        return;
    }
    std::wstring prompt =
        L"Enable the experimental hook in " + fg::wide(t->exe) +
        L"?\n\nThis loads a DLL into that process to limit DX11/DX12 frames and draw a DX11 "
        L"overlay. It can cause crashes or anti-cheat bans. A clear scan does not mean a game is "
        L"safe. Use only in games that permit it.\n\n";
    prompt += L"Windows security can also flag or quarantine injection components. If blocked, use "
              L"Desktop mode. Framoryx does not bypass security tools.\n\n";
    if (!app.rules.hookAllowed(t->exe))
        prompt += L"This also adds the executable to your allow list, including optional automatic "
                  L"borderless changes if enabled.\n\n";
    prompt += L"Disable restores hook entry points. Restart the game to remove the DLL "
              L"completely.\n\nEnable for this process now?";
    if (MessageBoxW(app.window, prompt.c_str(), L"Enable experimental hook",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;
    if (!app.rules.hookAllowed(t->exe)) {
        app.rules.allow.push_back(t->exe);
        saveSettings();
        app.capture.setRules(app.rules);
    }
    auto err = app.hook.attach(t->pid, t->exe, app.rules, app.cap);
    app.overlay = true;
    SetWindowTextW(app.controls[Overlay], L"Hide overlay");
    pushHookPresentation();
    app.hook.corner(app.corner);
    app.oldHookFrames = 0;
    app.oldHookTime = fg::clockSeconds();
    app.hookFps = 0;
    app.hookHistory.clear();
    app.hookCursor = 0;
    setNotice(err.empty()
                  ? L"Hook requested for the selected process. Waiting for DXGI presentation."
                  : err);
}
void exportCSV() {
    if (app.stats.frames.empty()) {
        setNotice(L"There are no ETW frames to export yet. Start capture and select a game.");
        return;
    }
    wchar_t filename[32768] = L"Framoryx-capture.csv";
    OPENFILENAMEW of{};
    of.lStructSize = sizeof(of);
    of.hwndOwner = app.window;
    of.lpstrFilter = L"CSV frame data\0*.csv\0\0";
    of.lpstrFile = filename;
    of.nMaxFile = 32768;
    of.lpstrDefExt = L"csv";
    of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&of))
        return;
    std::ofstream file{fs::path(filename), std::ios::binary};
    file << "Application,ProcessID,SwapChainAddress,TimeInSeconds,MsBetweenPresents\n"
         << std::fixed << std::setprecision(6);
    double firstTimestamp = app.stats.frames.front().time;
    for (auto& f : app.stats.frames) {
        std::string name;
        for (char c : app.stats.app) {
            if (c == '"')
                name += '"';
            name += c;
        }
        file << '"' << name << "\"," << app.stats.pid << ',' << app.stats.chain << ','
             << f.time - firstTimestamp << ',' << f.ms << '\n';
    }
    file.close();
    setNotice(file ? L"Saved the selected swapchain’s rolling 30-second capture."
                   : L"The CSV could not be saved.");
}
std::wstring rulesText(const std::vector<std::string>& rules) {
    std::wstring out;
    for (auto& p : rules)
        out += fg::wide(p) + L"\r\n";
    return out;
}
std::wstring editText(HWND edit) {
    int n = GetWindowTextLengthW(edit);
    std::wstring s(n + 1, 0);
    GetWindowTextW(edit, s.data(), n + 1);
    s.resize(n);
    return s;
}
struct RulesDialogData {
    float scale = 1;
    UINT dpi = 96;
    HFONT font = nullptr;
};
void scaleRuleControls(HWND window, RulesDialogData* data, float ratio) {
    HFONT previous = data->font;
    data->font = CreateFontW(-std::max(11, int(14 * data->scale)), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                             FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    struct Layout {
        float ratio;
        HFONT font;
    };
    Layout layout{ratio, data->font ? data->font : HFONT(GetStockObject(DEFAULT_GUI_FONT))};
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
            SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&layout));
    if (previous)
        DeleteObject(previous);
}
LRESULT CALLBACK rulesProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        auto data = new RulesDialogData;
        data->scale = app.scale;
        data->dpi = GetDpiForWindow(window);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
        auto staticText = [&](const wchar_t* text, int x, int y, int w, int h) {
            return CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, window,
                                 nullptr, app.instance, nullptr);
        };
        staticText(L"Hook allow list", 22, 18, 270, 26);
        staticText(L"Block list — takes priority", 322, 18, 280, 26);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", rulesText(app.rules.allow).c_str(),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE |
                            ES_AUTOVSCROLL | ES_WANTRETURN,
                        22, 50, 278, 220, window, reinterpret_cast<HMENU>(1), app.instance,
                        nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", rulesText(app.rules.deny).c_str(),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE |
                            ES_AUTOVSCROLL | ES_WANTRETURN,
                        322, 50, 278, 220, window, reinterpret_cast<HMENU>(2), app.instance,
                        nullptr);
        staticText(
            L"One executable name per line, e.g. game.exe or game*.exe.\nAllow list: permits "
            L"manual hook mode and optional auto borderless.\nBlock list: hides matching targets "
            L"and prevents modifications.\nSystem processes and known anti-cheat names are always "
            L"blocked.\nAn allow-list entry is your preference, not proof of anti-cheat approval.",
            22, 290, 580, 115);
        CreateWindowW(L"BUTTON", L"Save rules", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                      360, 416, 116, 34, window, reinterpret_cast<HMENU>(3), app.instance, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 488,
                      416, 112, 34, window, reinterpret_cast<HMENU>(4), app.instance, nullptr);
        scaleRuleControls(window, data, data->scale);
        for (int id = 1; id <= 2; ++id)
            SendMessageW(GetDlgItem(window, id), EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                         MAKELPARAM(8, 8));
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT r{};
        GetClientRect(window, &r);
        FillRect(reinterpret_cast<HDC>(wp), &r, app.brush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
        SetTextColor(reinterpret_cast<HDC>(wp), RGB(233, 242, 246));
        SetBkColor(reinterpret_cast<HDC>(wp), RGB(20, 28, 36));
        return reinterpret_cast<LRESULT>(app.brush);
    case WM_DRAWITEM: {
        auto d = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (d->CtlType != ODT_BUTTON)
            break;
        Graphics g(d->hDC);
        g.Clear(card);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        float w = float(d->rcItem.right), h = float(d->rcItem.bottom);
        rounded(g, RectF(0, 0, w - 1, h - 1), d->CtlID == 3 ? mint : Color(255, 30, 43, 53), 7,
                true);
        wchar_t title[80]{};
        GetWindowTextW(d->hwndItem, title, 80);
        SolidBrush b(d->CtlID == 3 ? bg : text);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        auto data = reinterpret_cast<RulesDialogData*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        g.DrawString(title, -1, uiFont(std::max(11.0f, 13 * (data ? data->scale : 1)), true),
                     RectF(0, 0, w, h), &format, &b);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDCANCEL && HIWORD(wp) == 0 && lp == 0) {
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
                while (std::getline(lines, line)) {
                    size_t start = line.find_first_not_of(L" \t\r"),
                           end = line.find_last_not_of(L" \t\r");
                    if (start == std::wstring::npos)
                        continue;
                    auto rule = fg::utf8(line.substr(start, end - start + 1));
                    if (!fg::validRule(rule)) {
                        MessageBoxW(window,
                                    L"Use executable basenames ending in .exe, one per line. "
                                    L"Wildcards * and ? are supported. Do not enter paths.",
                                    L"Check process rule", MB_OK | MB_ICONINFORMATION);
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
            auto currentProcesses = fg::processes();
            if (app.autoPid && (!currentProcesses.count(app.autoPid) ||
                                rules.blocked(currentProcesses[app.autoPid]))) {
                app.autoPid = 0;
                app.autoWindow = nullptr;
            }
            if (app.hook.targetAlive() && (app.hook.active() || app.hook.status() == 0)) {
                auto procs = fg::processes();
                if (!procs.count(app.hook.pid()) || !rules.hookAllowed(procs[app.hook.pid()]))
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
        if (LOWORD(wp) == 4) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_DPICHANGED: {
        auto data = reinterpret_cast<RulesDialogData*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        auto rect = reinterpret_cast<RECT*>(lp);
        if (data) {
            float ratio = float(HIWORD(wp)) / std::max(1u, data->dpi);
            data->dpi = HIWORD(wp);
            data->scale *= ratio;
            SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left,
                         rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            scaleRuleControls(window, data, ratio);
        }
        return 0;
    }
    case WM_NCDESTROY: {
        auto data = reinterpret_cast<RulesDialogData*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (data) {
            if (data->font)
                DeleteObject(data->font);
            delete data;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        EnableWindow(app.window, TRUE);
        SetForegroundWindow(app.window);
        return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}
void showRules() {
    RECT r{};
    GetWindowRect(app.window, &r);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    RECT bounds{0, 0, int(622 * app.scale), int(468 * app.scale)};
    AdjustWindowRectExForDpi(&bounds, style, FALSE, WS_EX_DLGMODALFRAME,
                             GetDpiForWindow(app.window));
    HWND w = CreateWindowExW(WS_EX_DLGMODALFRAME, L"Framoryx.Rules", L"Framoryx — Game rules",
                             style, r.left + int(70 * app.scale), r.top + int(70 * app.scale),
                             bounds.right - bounds.left, bounds.bottom - bounds.top, app.window,
                             nullptr, app.instance, nullptr);
    if (w) {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(w, 20, &dark, sizeof(dark));
        EnableWindow(app.window, FALSE);
        ShowWindow(w, SW_SHOW);
        SetForegroundWindow(w);
        SetFocus(GetDlgItem(w, 1));
    }
}

void layout() {
    RECT r{};
    GetClientRect(app.window, &r);
    if (r.right < 1 || r.bottom < 1)
        return;
    app.scale = std::min(float(r.right) / BW, float(r.bottom) / BH);
    float z = app.scale;
    auto place = [&](int id, int x, int y, int w, int h) {
        SetWindowPos(app.controls[id], nullptr, int(x * z), int(y * z), int(w * z), int(h * z),
                     SWP_NOZORDER | SWP_NOACTIVATE);
    };
    place(GamingTab, 18, 166, 151, 42);
    place(LimiterTab, 18, 216, 151, 42);
    place(BorderlessTab, 18, 266, 151, 42);
    place(OverlayTab, 18, 316, 151, 42);
    place(CompatibilityTab, 18, 366, 151, 42);
    place(AudioTab, 18, 416, 151, 42);
    place(Rules, 18, 490, 151, 42);
    place(Help, 18, 540, 151, 42);
    place(Motion, 22, 682, 148, 26);
    place(FullGuide, 244, 705, 260, 38);
    if (app.page == 5)
        place(Target, 588, 246, 538, 300);
    else
        place(Target, 232, 231, 550, 300);
    place(Refresh, 796, 229, 110, 38);
    place(Capture, 920, 229, 212, 38);
    place(Overlay, 212, 706, 188, 42);
    place(Corner, 414, 709, 166, 220);
    place(Export, 594, 706, 180, 42);
    place(CaptureDetails, 788, 706, 364, 42);
    place(Test, 520, 705, 260, 38);
    place(CapValue, 244, 450, 110, 46);
    place(Preset, 370, 457, 140, 260);
    place(Attach, 532, 448, 256, 50);
    place(Disable, 804, 448, 300, 50);
    place(HookHud, 244, 527, 760, 30);
    place(Monitor, 244, 445, 328, 240);
    place(Borderless, 592, 441, 242, 50);
    place(Restore, 854, 441, 250, 50);
    place(AutoBorderless, 244, 520, 840, 30);
    place(Volume, 250, 358, 560, 40);
    place(BoostLevel, 252, 510, 230, 220);
    place(ApplyBoost, 502, 508, 260, 44);
    place(DisableBoost, 778, 508, 300, 44);
    place(AudioSetup, 252, 686, 190, 40);
    place(AudioRefresh, 456, 686, 146, 40);
    place(AudioFolder, 616, 686, 222, 40);
    place(SoundSettings, 852, 686, 224, 40);
    place(HudMode, 240, 264, 132, 240);
    place(HudStyle, 388, 264, 127, 240);
    place(HudSize, 240, 338, 132, 260);
    place(HudOpacity, 388, 338, 127, 240);
    place(HudAccent, 240, 412, 132, 220);
    place(HudPosition, 388, 412, 127, 220);
    for (int i = 0; i < 10; ++i)
        place(HudFirstField + i, 240 + (i % 2) * 150, 488 + (i / 2) * 32, 145, 25);
    place(HudToggle, 240, 681, 156, 40);
    place(HudReset, 410, 681, 106, 40);
    place(DesktopSize, 768, 687, 220, 230);
    place(CreateShortcut, 1004, 687, 124, 38);
    place(CompatibilityRefresh, 244, 675, 230, 44);
    place(CompatibilityBorderless, 490, 675, 286, 44);
    HFONT previousFont = app.controlFont;
    app.controlFont = CreateFontW(-std::max(11, int(14 * z)), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                  FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    for (auto& c : app.controls) {
        SendMessageW(c.second, WM_SETFONT, reinterpret_cast<WPARAM>(app.controlFont), TRUE);
        wchar_t cls[32]{};
        GetClassNameW(c.second, cls, 32);
        if (_wcsicmp(cls, L"COMBOBOX") == 0) {
            SendMessageW(c.second, CB_SETITEMHEIGHT, -1, int(25 * z));
            SendMessageW(c.second, CB_SETITEMHEIGHT, 0, int(27 * z));
        }
    }
    if (previousFont)
        DeleteObject(previousFont);
}
void pageControls(int page) {
    SetWindowPos(app.controls[Target], nullptr, int((page == 5 ? 588 : 232) * app.scale),
                 int((page == 5 ? 246 : 231) * app.scale), int((page == 5 ? 538 : 550) * app.scale),
                 int(300 * app.scale), SWP_NOZORDER | SWP_NOACTIVATE);
    if (page == 1) {
        app.audio = fg::audioState();
        SendMessageW(app.controls[Volume], TBM_SETPOS, TRUE, int(app.audio.volume * 100));
        EnableWindow(app.controls[Volume], app.audio.available);
        EnableWindow(app.controls[ApplyBoost], app.audio.available && app.boost.available());
    }
    for (auto& c : app.controls) {
        int id = c.first;
        bool show = false;
        if ((id >= 201 && id <= 205) || id == OverlayTab || id == CompatibilityTab || id == Rules ||
            id == Help)
            show = true;
        else if (page == 0)
            show = id == Target || id == Refresh || id == Capture || id == Overlay ||
                   id == Corner || id == Export || id == CaptureDetails;
        else if (page == 1)
            show = id >= 210 && id <= SoundSettings;
        else if (page == 2)
            show = id == Target || id == Refresh || id == Capture || id == CapValue ||
                   id == Preset || id == Attach || id == Disable;
        else if (page == 3)
            show = id == Target || id == Refresh || id == Capture || id == Monitor ||
                   id == Borderless || id == Restore || id == AutoBorderless;
        else if (page == 4)
            show = id == FullGuide || id == Test;
        else if (page == 5)
            show = id == Target || (id >= HudSize && id <= HudReset) || id == HudToggle ||
                   id == CreateShortcut || id == HudStyle ||
                   (id >= HudFirstField && id < HudFirstField + 10);
        else if (page == 6)
            show = id == Target || id == Refresh || id == Capture || id == CompatibilityRefresh ||
                   id == CompatibilityBorderless;
        ShowWindow(c.second, show ? SW_SHOW : SW_HIDE);
        InvalidateRect(c.second, nullptr, FALSE);
    }
    InvalidateRect(app.window, nullptr, FALSE);
}
void switchPage(int page) {
    bool animate = page != app.page && app.motion && app.bufferDC &&
                   (GetForegroundWindow() == app.window || app.diagnosticAnimate);
    auto previous = animate ? captureScene() : nullptr;
    app.transitionOld.reset();
    app.transitionNew.reset();
    app.page = page;
    app.contentDirty = true;
    pageControls(page);
    if (page == 5)
        syncHudControls();
    if (animate && previous) {
        auto next = captureScene();
        if (next) {
            app.transitionOld = std::move(previous);
            app.transitionNew = std::move(next);
            app.transitionStart = animationClock();
            for (auto& control : app.controls) {
                RECT rect{};
                GetWindowRect(control.second, &rect);
                MapWindowPoints(nullptr, app.window, reinterpret_cast<POINT*>(&rect), 2);
                if (rect.left > int(190 * app.scale))
                    ShowWindow(control.second, SW_HIDE);
            }
        }
    }
    InvalidateRect(app.window, nullptr, FALSE);
}
void paintAudio(Graphics& g) {
    rounded(g, RectF(212, 190, 940, 562), card, 16, true);
    label(g, L"SOUND, ON YOUR TERMS", 250, 218, 780, 25, 11, mint, true);
    label(g, L"Turn up the quiet moments.", 247, 251, 850, 54, 30, text, true);
    label(g, app.audio.device, 251, 311, 850, 27, 15, text, true);
    label(g, L"OUTPUT VOLUME", 252, 343, 300, 20, 11, muted, true);
    label(g, value(app.audio.volume * 100) + L"%" + (app.audio.mute ? L" · muted" : L""), 850, 363,
          230, 36, 25, text, true);
    line(g, 252, 426, 1112, 426);
    label(g, L"EXTRA BOOST", 252, 450, 320, 25, 13, text, true);
    auto gain = app.boost.configuredGain(app.audio.guid);
    label(g, gain ? L"Configured: +" + std::to_wstring(*gain) + L" dB" : L"Not configured", 850,
          451, 250, 24, 12, gain ? mint : muted, true);
    label(g,
          app.boost.available()
              ? L"Equalizer APO config found · playback-device setup still needs verification."
              : L"Requires Equalizer APO · use Setup instructions for the one-time setup.",
          252, 480, 850, 24, 12, app.boost.available() ? mint : amber);
    label(g,
          L"Start with a gentle boost. Reduce it if you hear distortion.\nApplies to your current "
          L"default output and stays applied until you disable it.\nThis is preamplification; it "
          L"does not include a dynamic compressor.",
          252, 579, 840, 78, 14, muted, false, true);
    label(g, app.audioNotice, 214, 777, 938, 38, 12, muted, false, true);
}
void paintHeaderBase(Graphics& g) {
    double phase = 0;
    SolidBrush background(bg);
    g.FillRectangle(&background, 0, 0, 188, 145);
    g.FillRectangle(&background, 208, 0, 960, 178);
    rounded(g, RectF(212, 24, 940, 144), Color(255, 18, 29, 35), 16, true);
    g.SetClip(RectF(684, 25, 467, 142));
    if (app.artwork && app.artwork->GetLastStatus() == Ok) {
        float sh = float(app.artwork->GetWidth()) / (468.0f / 144.0f);
        float sy =
            (float(app.artwork->GetHeight()) - sh) * 0.5f + float(std::sin(phase * 0.16)) * 12;
        g.DrawImage(app.artwork.get(), RectF(684, 24, 468, 144), 0, sy,
                    float(app.artwork->GetWidth()), sh, UnitPixel);
        LinearGradientBrush shade(Point(684, 0), Point(1152, 0), Color(255, 18, 29, 35),
                                  Color(40, 18, 29, 35));
        g.FillRectangle(&shade, 684, 24, 468, 144);
    }
    g.ResetClip();
    label(g, L"FRAMORYX", 22, 98, 150, 30, 20, text, true);
    label(g, L"PLAY AT YOUR BEST", 24, 129, 149, 20, 9, muted, true);
    const wchar_t* headline = app.page == 5   ? L"Make it unmistakably yours."
                              : app.page == 6 ? L"Built around your game."
                              : app.page == 1 ? L"Sound that pulls you in."
                              : app.page == 2 ? L"Find your smooth."
                              : app.page == 3 ? L"A bigger way to play."
                              : app.page == 4 ? L"Ready when you are."
                                              : L"Every frame. Under control.";
    const wchar_t* subtitle = app.page == 5 ? L"Every readout. Every detail. Your choice."
                              : app.page == 6
                                  ? L"A clear path from game detection to a visible overlay."
                              : app.page == 1 ? L"Your audio. A little more presence."
                              : app.page == 2 ? L"Set the pace. Keep the frames consistent."
                              : app.page == 3 ? L"Edge-to-edge play, with one click."
                              : app.page == 4 ? L"A few steps. Then get back to your game."
                                              : L"Measure performance. Shape your play.";
    label(g, headline, 238, 55, 690, 44, 28, text, true);
    label(g, subtitle, 240, 107, 650, 26, 14, muted);
    rounded(g, RectF(1002, 130, 130, 24), Color(220, 20, 42, 39), 12);
    SolidBrush dot(app.capture.running() ? mint : muted);
    g.FillEllipse(&dot, 1014, 139, 6, 6);
    label(g, app.capture.running() ? L"CAPTURE LIVE" : L"READY", 1029, 135, 98, 18, 9,
          app.capture.running() ? mint : muted, true);
    line(g, 188, 0, 188, 178, edge);
}
void paintHeader(Graphics& g) {
    if (!app.headerCache || app.headerPage != app.page || app.headerLive != app.capture.running() ||
        app.headerScale != app.scale) {
        int w = std::max(1, int(BW * app.scale)), h = std::max(1, int(178 * app.scale));
        app.headerCache = std::make_unique<Bitmap>(w, h, PixelFormat32bppPARGB);
        Graphics cached(app.headerCache.get());
        cached.Clear(bg);
        cached.SetSmoothingMode(SmoothingModeAntiAlias);
        cached.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        cached.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        cached.ScaleTransform(app.scale, app.scale);
        paintHeaderBase(cached);
        app.headerPage = app.page;
        app.headerLive = app.capture.running();
        app.headerScale = app.scale;
        int size = std::max(1, int(62 * app.scale));
        app.logoCache = std::make_unique<Bitmap>(size, size, PixelFormat32bppPARGB);
        Graphics logo(app.logoCache.get());
        logo.Clear(Color(0, 0, 0, 0));
        logo.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        if (app.logo && app.logo->GetLastStatus() == Ok)
            logo.DrawImage(app.logo.get(), Rect(0, 0, size, size));
    }
    auto previous = g.GetInterpolationMode();
    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
    g.DrawImage(app.headerCache.get(), RectF(0, 0, float(BW), 178));
    double phase = app.motion ? app.animationTime : 0;
    float pulse = app.motion ? float((std::sin(phase * 1.8) + 1) * 0.5) : 0.5f;
    SolidBrush halo(Color(BYTE(8 + pulse * 12), 125, 242, 198));
    g.FillEllipse(&halo, 16, 18, 72, 72);
    if (app.logoCache)
        g.SetInterpolationMode(InterpolationModeHighQualityBilinear);
    if (app.logoCache)
        g.DrawImage(app.logoCache.get(),
                    RectF(22, 23 + (app.motion ? float(std::sin(phase * 1.3)) * 1.5f : 0), 62, 62));
    if (app.motion) {
        g.SetClip(RectF(700, 27, 448, 120));
        for (int lane = 0; lane < 4; ++lane) {
            std::vector<PointF> path;
            for (int i = 0; i <= 60; ++i) {
                float x = 708 + i * 7.3f,
                      y = 87 + lane * 12 + float(std::sin(i * .12 + phase * .8 + lane * .55)) * 9;
                path.emplace_back(x, y);
            }
            Pen trace(Color(BYTE(12 + lane * 4), 125, 242, 198), 1);
            g.DrawLines(&trace, path.data(), int(path.size()));
            float travel = float(std::fmod(phase * .12 + lane * .21, 1.)), at = travel * 60;
            float x = 708 + at * 7.3f,
                  y = 87 + lane * 12 + float(std::sin(at * .12 + phase * .8 + lane * .55)) * 9;
            float end = at + 2.3f, fade = std::clamp(std::min(travel, 1 - travel) * 12, 0.f, 1.f);
            Pen pulse(Color(BYTE(90 * fade), 125, 242, 198), 1.8f);
            g.DrawLine(&pulse, x, y, 708 + end * 7.3f,
                       87 + lane * 12 + float(std::sin(end * .12 + phase * .8 + lane * .55)) * 9);
        }
        g.SetClip(RectF(854, 27, 294, 100));
        for (int i = 0; i < 18; ++i) {
            float x = 1000 + float(std::sin(phase * (.07 + i * .001) + i * 3.7)) * 137,
                  y = 81 + float(std::cos(phase * (.05 + i * .002) + i * 2.1)) * 38;
            SolidBrush b(Color(BYTE(35 + (i % 4) * 14), 125, 242, 198));
            g.FillEllipse(&b, x, y, 2.0f, 2.0f);
        }
        g.ResetClip();
        float travel = float(std::fmod(phase * .065, 1.)),
              fade = float(std::pow(std::sin(travel * 3.14159265), 2));
        float x = 238 + travel * 860;
        line(g, x, 167, x + 54, 167, Color(BYTE(70 * fade), 125, 242, 198), 1);
    }
    g.SetInterpolationMode(previous);
}
void paintOverview(Graphics& g) {
    tile(g, 212, 268, L"LIVE FPS",
         app.stats.live ? value(app.motion ? app.metricFps : app.stats.fps)
                        : (app.hook.active() && app.selectedPid == app.hook.pid() && app.hookFps > 0
                               ? value(app.hookFps)
                               : L"—"),
         app.stats.live ? L"Presented frames / second" : L"Waiting for game frames", true);
    tile(g, 496, 204, L"AVERAGE FPS",
         app.stats.frames.empty() ? L"—"
                                  : value(app.motion ? app.metricAverage : app.stats.average),
         L"Last 30 seconds");
    tile(g, 716, 204, L"1% LOW FPS",
         app.stats.frames.size() < 100 ? L"—" : value(app.motion ? app.metricLow : app.stats.low),
         L"Slowest 1% of frames");
    tile(g, 936, 216, L"FRAME TIME",
         app.stats.live ? value(app.motion ? app.metricMs : app.stats.ms, 2) + L" ms" : L"—",
         L"Lower is faster");
    rounded(g, RectF(212, 430, 620, 248), card, 14, true);
    label(g, L"Frame pacing", 236, 450, 310, 29, 17, text, true);
    label(g, app.hookSource ? L"HOOK / 30 SEC" : L"ETW / 30 SEC", 670, 454, 140, 22, 10, muted,
          true);
    graph(g, RectF(238, 506, 516, 118), app.stats);
    label(g, L"−30 s", 238, 644, 110, 20, 11, muted);
    label(g, L"Now", 732, 644, 70, 20, 11, muted);
    rounded(g, RectF(848, 430, 304, 248), card, 14, true);
    label(g, L"System pulse", 872, 450, 254, 28, 17, text, true);
    label(g, L"PROCESSOR", 872, 498, 128, 22, 10, muted, true);
    label(g, app.cpuReady ? value(app.motion ? app.metricCpu : app.cpu) + L"%" : L"—", 1020, 490,
          116, 37, 28, text, true);
    rounded(g, RectF(874, 538, 252, 4), edge, 2);
    if (app.cpu > 0)
        rounded(g, RectF(874, 538, std::max(4.0f, float(app.cpu) * 2.52f), 4), mint, 2);
    label(g, L"MEMORY", 872, 571, 120, 22, 10, muted, true);
    label(g, app.memoryReady ? value(app.ramGB, 1) + L" GB" : L"—", 992, 564, 140, 36, 26, text,
          true);
    label(g,
          app.memoryReady ? L"of " + value(app.totalGB, 1) + L" GB system RAM"
                          : L"Reading system memory…",
          872, 614, 264, 24, 12, muted);
}
void paintLimiter(Graphics& g) {
    rounded(g, RectF(212, 300, 940, 448), card, 16, true);
    label(g, L"A steadier pace.", 242, 333, 820, 46, 30, text, true);
    label(g, L"Choose a target your game can sustain. 0 means uncapped.", 244, 393, 840, 27, 15,
          muted);
    label(g, L"FRAME LIMIT", 244, 426, 260, 22, 11, muted, true);
    rounded(g, RectF(244, 584, 860, 119), Color(255, 29, 36, 40), 12, true);
    label(g,
          app.hook.active()        ? (app.hook.frameCount() ? L"Limiter connected"
                                                            : L"Waiting for supported game frames")
          : app.hook.status() == 0 ? L"Connecting to game…"
          : app.hook.status() < 0  ? L"Hook unavailable · restart the game to retry"
                                   : L"Experimental · explicit opt-in",
          264, 600, 810, 29, 16, app.hook.active() ? mint : amber, true);
    label(g,
          app.hook.active()
              ? L"PID " + std::to_wstring(app.hook.pid()) +
                    (app.hook.hudFresh()
                         ? (app.hook.renderer() == 12 ? L" · DX12 in-game HUD active"
                                                      : L" · DX11 in-game HUD active")
                         : L" · Desktop fallback available in Overlay Studio")
              : L"Select a game and enable the limiter to add it to your allow list. Hooks can "
                L"cause crashes or anti-cheat bans. A clear scan does not prove a game is safe.",
          264, 639, 810, 54, 13, muted, false, true);
}
void paintBorderless(Graphics& g) {
    rounded(g, RectF(212, 300, 940, 448), card, 16, true);
    label(g, L"Give your game the whole screen.", 242, 333, 840, 47, 28, text, true);
    label(g, L"Start in Windowed mode. Choose a display, then make it borderless.", 244, 391, 850,
          28, 15, muted);
    label(g, L"DESTINATION DISPLAY", 244, 421, 420, 22, 11, muted, true);
    rounded(g, RectF(244, 580, 860, 124), Color(255, 23, 41, 41), 12, true);
    label(g, L"Overlay hidden by fullscreen?", 264, 598, 810, 30, 17, mint, true);
    label(g,
          L"Borderless mode lets the desktop overlay sit above most games. Auto borderless only "
          L"changes games on your allow list. Restore window returns the original borders and "
          L"position; exiting Framoryx restores saved windows too.",
          264, 639, 810, 58, 14, muted, false, true);
}
void paintGuide(Graphics& g) {
    rounded(g, RectF(212, 190, 940, 562), card, 16, true);
    label(g, L"START IN THREE STEPS", 244, 217, 840, 24, 11, mint, true);
    label(g, L"Less setup. More play.", 241, 251, 850, 48, 30, text, true);
    const wchar_t* titles[]{L"Open your game", L"Start capture", L"Show the overlay"};
    const wchar_t* notes[]{
        L"Leave Auto selected, or choose a specific game window from the list.",
        L"See live FPS and frame times. Capture keeps running while you switch pages.",
        L"Use borderless mode if fullscreen hides the desktop counter."};
    for (int i = 0; i < 3; ++i) {
        float x = 244 + i * 289;
        rounded(g, RectF(x, 320, 272, 163), Color(255, 24, 37, 43), 12, true);
        label(g, L"0" + std::to_wstring(i + 1), x + 18, 338, 210, 29, 17, mint, true);
        label(g, titles[i], x + 18, 378, 235, 26, 17, text, true);
        label(g, notes[i], x + 18, 417, 235, 58, 13, muted, false, true);
    }
    label(g, L"Need steadier frames?", 244, 512, 400, 28, 17, text, true);
    label(g,
          L"Open Frame limiter, select a game and choose your FPS. Hooks require an explicit "
          L"confirmation and can carry anti-cheat risk.",
          244, 550, 407, 64, 13, muted, false, true);
    label(g, L"Need louder audio?", 697, 512, 400, 28, 17, text, true);
    label(g,
          L"Audio controls your Windows output volume. Extra boost uses optional Equalizer APO; "
          L"its Setup instructions guide you through it.",
          697, 550, 405, 64, 13, muted, false, true);
    line(g, 244, 633, 1118, 633);
    label(g, L"Ctrl + Alt + O    Overlay", 244, 653, 282, 25, 12, mint, true);
    label(g, L"Ctrl + Alt + B    Borderless", 538, 653, 282, 25, 12, mint, true);
    label(g, L"Ctrl + Alt + L    Disable hook", 831, 653, 286, 25, 12, mint, true);
}
#include "ui_features.hpp"
void paintMain(HDC destination, const RECT& invalid) {
    LARGE_INTEGER paintingStart{}, paintingEnd{}, frequency{};
    QueryPerformanceCounter(&paintingStart);
    QueryPerformanceFrequency(&frequency);
    RECT r{};
    GetClientRect(app.window, &r);
    bool fresh = false;
    if (!app.bufferDC || app.bufferWidth != r.right || app.bufferHeight != r.bottom) {
        if (app.contentDC) {
            SelectObject(app.contentDC, app.contentOriginal);
            DeleteObject(app.contentBitmap);
            DeleteDC(app.contentDC);
        }
        app.contentDC = CreateCompatibleDC(destination);
        app.contentBitmap =
            CreateCompatibleBitmap(destination, std::max(1L, r.right), std::max(1L, r.bottom));
        app.contentOriginal = HBITMAP(SelectObject(app.contentDC, app.contentBitmap));
        if (app.bufferDC) {
            SelectObject(app.bufferDC, app.bufferOriginal);
            DeleteObject(app.bufferBitmap);
            DeleteDC(app.bufferDC);
        }
        app.bufferDC = CreateCompatibleDC(destination);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = std::max(1L, r.right);
        info.bmiHeader.biHeight = -std::max(1L, r.bottom);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        app.bufferBitmap =
            CreateDIBSection(destination, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        app.bufferOriginal = HBITMAP(SelectObject(app.bufferDC, app.bufferBitmap));
        app.bufferWidth = r.right;
        app.bufferHeight = r.bottom;
        fresh = true;
    }
    bool full = fresh || app.contentDirty;
    if (!full)
        BitBlt(app.bufferDC, 0, 0, r.right, r.bottom, app.contentDC, 0, 0, SRCCOPY);
    {
        Graphics g(app.bufferDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.ScaleTransform(app.scale, app.scale);
        if (full)
            ++app.fullPaintCount;
        if (full) {
            g.Clear(bg);
            line(g, 188, 0, 188, 820, edge);
            line(g, 24, 476, 162, 476);
            label(g, L"YOUR TOOLKIT", 24, 722, 145, 20, 9, muted, true);
            label(g, L"FRAMORYX 0.2 · WINDOWS X64", 24, 747, 146, 40, 9, muted, false, true);
            if (!app.preferencesSaved)
                label(g, L"Preferences not saved", 24, 794, 145, 20, 9, amber, true);
            if (app.page == 5)
                paintStudio(g);
            else if (app.page == 6)
                paintCompatibility(g);
            else if (app.page == 1)
                paintAudio(g);
            else if (app.page == 4)
                paintGuide(g);
            else {
                rounded(g, RectF(212, 190, 940, 94), card, 14, true);
                label(g, L"YOUR GAME", 232, 206, 500, 20, 11, muted, true);
                if (app.page == 0)
                    paintOverview(g);
                else if (app.page == 2)
                    paintLimiter(g);
                else
                    paintBorderless(g);
                label(g, app.notice, 214, 777, 936, 38, 12, muted, false, true);
            }
            g.Flush(FlushIntentionSync);
            auto copy = g.GetHDC();
            BitBlt(app.contentDC, 0, 0, r.right, r.bottom, copy, 0, 0, SRCCOPY);
            g.ReleaseHDC(copy);
            app.contentDirty = false;
        }
        paintHeader(g);
        if (app.page == 0 && app.motion)
            paintAnimatedMetrics(g);
        if (app.transitionNew && !app.capturingScene) {
            double t = fg::easeOut((animationClock() - app.transitionStart) / .22);
            g.SetClip(RectF(198, 0, BW - 198, BH));
            SolidBrush background(bg);
            g.FillRectangle(&background, 198, 0, BW - 198, BH);
            auto layer = [&](Bitmap* image, float opacity, float offset) {
                if (!image)
                    return;
                ColorMatrix matrix{};
                matrix.m[0][0] = matrix.m[1][1] = matrix.m[2][2] = 1;
                matrix.m[3][3] = opacity;
                matrix.m[4][4] = 1;
                ImageAttributes attributes;
                attributes.SetColorMatrix(&matrix);
                g.DrawImage(
                    image,
                    RectF(offset, 0, app.bufferWidth / app.scale, app.bufferHeight / app.scale), 0,
                    0, float(image->GetWidth()), float(image->GetHeight()), UnitPixel, &attributes);
            };
            layer(app.transitionOld.get(), float(1 - t), float(-14 * t));
            layer(app.transitionNew.get(), float(t), float(18 * (1 - t)));
            g.ResetClip();
        }
    }
    BitBlt(destination, 0, 0, r.right, r.bottom, app.bufferDC, 0, 0, SRCCOPY);
    QueryPerformanceCounter(&paintingEnd);
    ++app.paintCount;
    double paintMs = 1000.0 * (paintingEnd.QuadPart - paintingStart.QuadPart) / frequency.QuadPart;
    app.paintMilliseconds += paintMs;
    app.maxPaintMs = std::max(app.maxPaintMs, paintMs);
}
void positionHud() {
    if (!app.overlay || !app.hudOptions.fields || app.hudMode == 2) {
        ShowWindow(app.hud, SW_HIDE);
        return;
    }
    HWND target = selected() ? selected()->hwnd : app.autoWindow;
    // Auto desktop HUD yields to an attached DX11 HUD, avoiding duplicate counters.
    if (!target || !IsWindow(target) || IsIconic(target) ||
        (app.hudMode != 1 && app.hook.active() && app.hookHud && app.hook.hudFresh() &&
         app.hook.pid() == app.selectedPid)) {
        ShowWindow(app.hud, SW_HIDE);
        return;
    }
    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
    if (foregroundPid != app.selectedPid) {
        ShowWindow(app.hud, SW_HIDE);
        return;
    }
    RECT r = selectedMonitor(target);
    float scale = fg::overlayScale(app.hudOptions.scale, r.bottom - r.top);
    scale = std::min(scale, std::min((r.right - r.left - 44) / fg::hudWidth(app.hudOptions),
                                     (r.bottom - r.top - 44) / fg::hudHeight(app.hudOptions)));
    app.hudRenderScale = scale;
    int w = int(fg::hudWidth(app.hudOptions) * scale),
        h = int(fg::hudHeight(app.hudOptions) * scale);
    app.hudWidth = w;
    app.hudHeight = h;
    int x = (app.corner % 2) ? r.right - w - 22 : r.left + 22;
    int y = app.corner >= 2 ? r.bottom - h - 22 : r.top + 22;
    SetWindowPos(app.hud, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(app.hud, nullptr, FALSE);
}
LRESULT CALLBACK hudProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST)
        return HTTRANSPARENT;
    if (msg == WM_MOUSEACTIVATE)
        return MA_NOACTIVATE;
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{};
        BeginPaint(window, &ps);
        EndPaint(window, &ps);
        Bitmap image(std::max(1, app.hudWidth), std::max(1, app.hudHeight), PixelFormat32bppPARGB);
        {
            Graphics g(&image);
            g.Clear(Color(0, 0, 0, 0));
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
            g.ScaleTransform(app.hudRenderScale, app.hudRenderScale);
            paintHudContent(g, app.hudOptions, app.stats, false);
        }
        HBITMAP bitmap = nullptr;
        if (image.GetHBITMAP(Color(0, 0, 0, 0), &bitmap) == Ok) {
            HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
            auto old = SelectObject(dc, bitmap);
            RECT rect{};
            GetWindowRect(window, &rect);
            POINT position{rect.left, rect.top}, source{};
            SIZE size{app.hudWidth, app.hudHeight};
            BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
            UpdateLayeredWindow(window, screen, &position, &size, dc, &source, 0, &blend,
                                ULW_ALPHA);
            SelectObject(dc, old);
            DeleteObject(bitmap);
            DeleteDC(dc);
            ReleaseDC(nullptr, screen);
        }
        return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}
void update() {
    app.contentDirty = true;
    PerfMeter perf(app.updateMilliseconds);
    ++app.tick;
    if (app.pendingDemoPid) {
        struct DemoFind {
            DWORD pid;
            HWND window;
        };
        DemoFind found{app.pendingDemoPid, nullptr};
        EnumWindows(
            [](HWND w, LPARAM data) -> BOOL {
                auto found = reinterpret_cast<DemoFind*>(data);
                DWORD pid = 0;
                GetWindowThreadProcessId(w, &pid);
                if (pid == found->pid && IsWindowVisible(w)) {
                    found->window = w;
                    return FALSE;
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&found));
        if (found.window && !SendMessageW(app.controls[Target], CB_GETDROPPEDSTATE, 0, 0)) {
            refreshTargets();
            for (size_t i = 0; i < app.targets.size(); ++i)
                if (app.targets[i].pid == found.pid) {
                    SendMessageW(app.controls[Target], CB_SETCURSEL, i + 1, 0);
                    break;
                }
            app.autoPid = found.pid;
            app.autoWindow = found.window;
            app.pendingDemoPid = 0;
        } else if (app.demoProcess && WaitForSingleObject(app.demoProcess.h, 0) != WAIT_TIMEOUT) {
            app.pendingDemoPid = 0;
            setNotice(L"The test scene could not start. Try your game directly or check the "
                      L"complete application folder.");
        }
    }
    if (!app.diagnosticPath.empty()) {
        ++app.sampleCount;
        if (GetForegroundWindow() == app.window)
            ++app.foregroundSamples;
        if (app.tick % 4 == 0) {
            FILETIME c{}, e{}, k{}, u{};
            GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
            ULONGLONG kernel = (ULONGLONG(k.dwHighDateTime) << 32) | k.dwLowDateTime,
                      user = (ULONGLONG(u.dwHighDateTime) << 32) | u.dwLowDateTime;
            app.cpuSamples.push_back(double(kernel + user) / 10000000.0);
        }
        auto gdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        app.peakGdi = std::max(app.peakGdi, gdi);
        if (!app.initialGdi)
            app.initialGdi = gdi;
        DWORD handles = 0;
        GetProcessHandleCount(GetCurrentProcess(), &handles);
        app.peakHandles = std::max(app.peakHandles, handles);
    }
    if (app.smokeSeconds > 0) {
        double elapsed = fg::clockSeconds() - app.diagnosticStart;
        if (elapsed >= app.smokeSeconds) {
            PostMessageW(app.window, WM_CLOSE, 0, 0);
            return;
        }
        if (!app.snapshotPath.empty() && !app.snapshotSaved && elapsed > 3 && !app.transitionNew) {
            auto image = captureScene();
            if (image) {
                UINT count = 0, bytes = 0;
                GetImageEncodersSize(&count, &bytes);
                std::vector<BYTE> data(bytes);
                auto encoders = reinterpret_cast<ImageCodecInfo*>(data.data());
                if (GetImageEncoders(count, bytes, encoders) == Ok)
                    for (UINT i = 0; i < count; ++i)
                        if (wcscmp(encoders[i].MimeType, L"image/png") == 0) {
                            app.snapshotSaved = image->Save(app.snapshotPath.c_str(),
                                                            &encoders[i].Clsid, nullptr) == Ok;
                            break;
                        }
            }
        }
        if (app.smokeCustomize && !app.settingsOverride.empty()) {
            if (elapsed >= 4 && app.customizationStep == 0) {
                auto choose = [&](int id, int selection, bool number) {
                    if (number)
                        selectComboValue(id, selection);
                    else
                        SendMessageW(app.controls[id], CB_SETCURSEL, selection, 0);
                    SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE),
                                 reinterpret_cast<LPARAM>(app.controls[id]));
                };
                choose(HudSize, 125, true);
                choose(HudOpacity, 75, true);
                choose(HudAccent, 1, false);
                choose(HudPosition, 3, false);
                choose(HudMode, 1, false);
                choose(HudStyle, 2, false);
                uint32_t fields =
                    fg::HudFps | fg::HudAverage | fg::HudTime | fg::HudCpu | fg::HudGraph;
                for (int i = 0; i < 10; ++i)
                    SendMessageW(app.controls[HudFirstField + i], BM_SETCHECK,
                                 (fields & hudFields[i]) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(HudFirstField, BN_CLICKED),
                             reinterpret_cast<LPARAM>(app.controls[HudFirstField]));
                app.customizationStep = 1;
            }
            if (elapsed >= 6 && app.customizationStep == 1) {
                auto exe = app.activeProfile;
                app.hudProfiles.clear();
                loadSettings();
                applyProfile(exe);
                app.customizationRoundTrip =
                    app.hudOptions.scale == 125 && app.hudOptions.opacity == 75 &&
                    app.hudOptions.accent == 1 && app.corner == 3 && app.hudMode == 1 &&
                    app.hudOptions.style == 2 && (app.hudOptions.fields & fg::HudAverage) &&
                    !(app.hudOptions.fields & fg::HudLow);
                if (app.customizationRoundTrip)
                    ++app.customizationChecks;
                app.customizationStep = 2;
            }
            if (elapsed >= 9 && app.customizationStep == 2) {
                selectComboValue(DesktopSize, 80);
                SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(DesktopSize, CBN_SELCHANGE),
                             reinterpret_cast<LPARAM>(app.controls[DesktopSize]));
                if (app.scale > .4f && app.scale <= .81f * GetDpiForWindow(app.window) / 96.f)
                    ++app.customizationChecks;
                app.customizationStep = 3;
            }
            if (elapsed >= 11 && app.customizationStep == 3) {
                selectComboValue(DesktopSize, 125);
                SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(DesktopSize, CBN_SELCHANGE),
                             reinterpret_cast<LPARAM>(app.controls[DesktopSize]));
                MONITORINFO m{};
                m.cbSize = sizeof(m);
                GetMonitorInfoW(MonitorFromWindow(app.window, MONITOR_DEFAULTTONEAREST), &m);
                RECT actual{};
                GetWindowRect(app.window, &actual);
                if (actual.right - actual.left <= m.rcWork.right - m.rcWork.left &&
                    actual.bottom - actual.top <= m.rcWork.bottom - m.rcWork.top)
                    ++app.customizationChecks;
                app.customizationStep = 4;
            }
            if (elapsed >= 13 && app.customizationStep == 4) {
                selectComboValue(DesktopSize, 0);
                SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(DesktopSize, CBN_SELCHANGE),
                             reinterpret_cast<LPARAM>(app.controls[DesktopSize]));
                syncHudControls();
                ++app.customizationChecks;
                app.customizationStep = 5;
            }
        }
        int desired = app.fixedSmokePage >= 0 ? app.fixedSmokePage : int(elapsed / 2) % 7;
        if (desired != app.page)
            switchPage(desired);
        int step = int(elapsed / 4) % 3;
        if (app.smokeResize && step != app.smokeResizeStep && app.smokeOuterWidth > 0) {
            const float sizes[]{1.0f, 0.88f, 0.78f};
            SetWindowPos(app.window, nullptr, 0, 0, int(app.smokeOuterWidth * sizes[step]),
                         int(app.smokeOuterHeight * sizes[step]),
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            app.smokeResizeStep = step;
        }
        if (elapsed >= 8 && !app.smokeDialogOpened && app.fixedSmokePage < 0) {
            app.smokeDialogOpened = true;
            showRules();
            HWND rules = GetLastActivePopup(app.window);
            if (rules && rules != app.window)
                SetTimer(rules, 91, 700, [](HWND w, UINT, UINT_PTR timer, DWORD) {
                    KillTimer(w, timer);
                    PostMessageW(w, WM_CLOSE, 0, 0);
                });
        }
    }
    DWORD foregroundPid = 0;
    HWND foreground = GetForegroundWindow();
    GetWindowThreadProcessId(foreground, &foregroundPid);
    if ((app.capture.running() || app.hook.active() || app.autoBorderless) &&
        foregroundPid != GetCurrentProcessId() && foreground &&
        !(GetWindowLongPtrW(foreground, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) &&
        (foregroundPid != app.cachedForegroundPid || foreground != app.cachedForegroundWindow ||
         app.tick % 20 == 0)) {
        app.cachedForegroundPid = foregroundPid;
        app.cachedForegroundWindow = foreground;
        auto list = fg::processes();
        auto item = list.find(foregroundPid);
        if (item != list.end() && !app.rules.blocked(item->second)) {
            app.autoPid = foregroundPid;
            app.autoWindow = foreground;
            app.autoExe = item->second;
        }
    }
    auto* t = selected();
    app.selectedPid = t ? t->pid : app.autoPid;
    HWND targetWindow = t ? t->hwnd : app.autoWindow;
    DWORD targetWindowPid = 0;
    if (targetWindow)
        GetWindowThreadProcessId(targetWindow, &targetWindowPid);
    if (!targetWindow || !IsWindow(targetWindow) || targetWindowPid != app.selectedPid)
        app.selectedPid = 0;
    auto profileName = t ? t->exe : app.autoExe;
    if (app.activeProfile != fg::lower(profileName))
        applyProfile(profileName);
    if (app.selectedPid != app.compatibilityPid || (app.page == 6 && app.tick % 20 == 0)) {
        app.compatibility = fg::inspectCompatibility(targetWindow, profileName);
        app.compatibilityPid = app.selectedPid;
        if (app.page == 6)
            app.compatibilityGuard = fg::antiCheatRunning();
    }
    app.stats = app.capture.snapshot(app.selectedPid);
    if (app.selectedPid == app.demoPid && app.stats.live)
        ++app.demoLiveSamples;
    for (auto& frame : app.hook.drainFrames(app.hookCursor))
        app.hookHistory.add(frame, fg::clockSeconds());
    app.hookSource = false;
    if (!app.stats.live && app.hook.active() && app.selectedPid == app.hook.pid()) {
        auto fallback = app.hookHistory.snapshot(app.selectedPid, fg::clockSeconds());
        if (fallback.live) {
            app.stats = std::move(fallback);
            app.hookSource = true;
        }
    }
    if (app.tick % 4 == 0) {
        updateTray();
        FILETIME idle{}, kernel{}, user{};
        if (GetSystemTimes(&idle, &kernel, &user)) {
            auto n = [](FILETIME t) {
                return (ULONGLONG(t.dwHighDateTime) << 32) | t.dwLowDateTime;
            };
            auto i = n(idle), k = n(kernel), u = n(user);
            auto elapsed = k - app.oldKernel + u - app.oldUser;
            if (app.oldKernel && elapsed) {
                app.cpu = std::clamp(100.0 * (elapsed - (i - app.oldIdle)) / elapsed, 0.0, 100.0);
                app.cpuReady = true;
            }
            app.oldIdle = i;
            app.oldKernel = k;
            app.oldUser = u;
        }
        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);
        if (GlobalMemoryStatusEx(&memory)) {
            app.ramGB = double(memory.ullTotalPhys - memory.ullAvailPhys) / (1024.0 * 1024 * 1024);
            app.totalGB = double(memory.ullTotalPhys) / (1024.0 * 1024 * 1024);
            app.memoryReady = true;
        }
        if (app.page == 1) {
            app.audio = fg::audioState();
            if (GetCapture() != app.controls[Volume])
                SendMessageW(app.controls[Volume], TBM_SETPOS, TRUE, int(app.audio.volume * 100));
            EnableWindow(app.controls[Volume], app.audio.available);
            EnableWindow(app.controls[ApplyBoost], app.audio.available && app.boost.available());
        }
        if (app.hook.targetAlive() && (app.hook.active() || app.hook.status() == 0)) {
            app.blocker = fg::antiCheatRunning();
            if (!app.blocker.empty()) {
                app.hook.disable();
                setNotice(L"Hook disabled because a known anti-cheat or inspection failure was "
                          L"detected: " +
                          app.blocker + L". Restart the hooked game to remove the DLL.");
            }
        }
        double now = fg::clockSeconds();
        auto frames = app.hook.frameCount();
        if (app.oldHookTime && now > app.oldHookTime)
            app.hookFps = double(frames - app.oldHookFrames) / (now - app.oldHookTime);
        app.oldHookTime = now;
        app.oldHookFrames = frames;
        if (app.autoBorderless) {
            HWND w = t ? t->hwnd : app.autoWindow;
            auto procs = fg::processes();
            DWORD pid = 0;
            if (w)
                GetWindowThreadProcessId(w, &pid);
            if (w && IsWindowVisible(w) && !IsIconic(w) && procs.count(pid) &&
                app.rules.hookAllowed(procs[pid])) {
                RECT current{};
                GetWindowRect(w, &current);
                RECT destination = selectedMonitor(w);
                if ((GetWindowLongPtrW(w, GWL_STYLE) & WS_CAPTION) ||
                    !EqualRect(&current, &destination)) {
                    auto err = app.borderless.apply(w, destination);
                    if (!err.empty())
                        app.notice = L"Auto borderless: " + err;
                }
            }
        }
    }
    pushHookPresentation();
    app.hook.systemMetrics(app.cpu, app.ramGB);
    app.hook.tick();
    updateAnimationState();
    auto captureMessage = app.capture.status();
    if (captureMessage != app.observedCaptureMessage) {
        bool showingCaptureStatus = app.notice == app.observedCaptureMessage;
        app.observedCaptureMessage = captureMessage;
        if (showingCaptureStatus || (!app.capture.running() && app.tick > 1))
            app.notice = captureMessage;
    }
    if (app.capture.running() != app.lastCaptureRunning) {
        app.lastCaptureRunning = app.capture.running();
        if (!app.lastCaptureRunning || GetTickCount64() > app.noticeUntil)
            app.notice = captureMessage;
        SetWindowTextW(app.controls[Capture],
                       app.lastCaptureRunning ? L"Stop capture" : L"Start capture");
    } else if (app.capture.running() && GetTickCount64() > app.noticeUntil)
        app.notice = captureMessage;
    EnableWindow(app.controls[Disable], app.hook.active() || app.hook.status() == 0);
    positionHud();
    if (!IsIconic(app.window) && (GetForegroundWindow() == app.window || app.tick % 4 == 0))
        InvalidateRect(app.window, nullptr, FALSE);
}
void launchDemo() {
    if (app.rules.blocked("FramoryxTest.exe")) {
        setNotice(
            L"The test scene is blocked by Game rules. Allow FramoryxTest.exe to use the demo.");
        return;
    }
    switchPage(0);
    if (app.demoProcess && WaitForSingleObject(app.demoProcess.h, 0) == WAIT_TIMEOUT) {
        app.pendingDemoPid = app.demoPid;
        setNotice(L"The live test scene is already running.");
        return;
    }
    fg::Handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job.h, JobObjectExtendedLimitInformation, &limits,
                                         sizeof(limits))) {
        setNotice(L"Could not create the managed demo session.");
        return;
    }
    auto executable = fg::executableDir() + L"\\tools\\FramoryxTest.exe";
    std::wstring command = L"\"" + executable + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNOACTIVATE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED, nullptr, nullptr, &startup, &process)) {
        setNotice(
            L"The test scene is missing or blocked. Extract the complete application folder.");
        return;
    }
    fg::Handle target(process.hProcess), thread(process.hThread);
    if (!AssignProcessToJobObject(job.h, target.h)) {
        TerminateProcess(target.h, 1);
        setNotice(L"Windows prevented the managed demo from starting.");
        return;
    }
    app.demoJob = std::move(job);
    app.demoProcess = std::move(target);
    app.demoPid = process.dwProcessId;
    app.pendingDemoPid = process.dwProcessId;
    app.autoPid = process.dwProcessId;
    app.autoWindow = nullptr;
    SendMessageW(app.controls[Target], CB_SETCURSEL, 0, 0);
    ResumeThread(thread.h);
    std::wstring error;
    if (!app.capture.running())
        error = app.capture.start(app.rules);
    setNotice(error.empty() ? L"Live demo started. These are real DirectX frames. Try the overlay, "
                              L"borderless mode or optional limiter."
                            : error);
}
LRESULT CALLBACK controlProc(HWND w, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR id) {
    if (msg == WM_MOUSEMOVE && app.hoverId != int(id)) {
        int previous = app.hoverId;
        app.hoverId = int(id);
        if (app.controls.count(previous))
            InvalidateRect(app.controls[previous], nullptr, FALSE);
        InvalidateRect(w, nullptr, FALSE);
        TRACKMOUSEEVENT event{sizeof(event), TME_LEAVE, w, 0};
        TrackMouseEvent(&event);
    }
    if (msg == WM_MOUSELEAVE) {
        app.hoverId = 0;
        InvalidateRect(w, nullptr, FALSE);
    }
    if (msg == WM_SETCURSOR && IsWindowEnabled(w)) {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return TRUE;
    }
    return DefSubclassProc(w, msg, wp, lp);
}
LRESULT CALLBACK comboProc(HWND w, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR id) {
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        PAINTSTRUCT ps{};
        HDC dc = msg == WM_PRINTCLIENT ? reinterpret_cast<HDC>(wp) : BeginPaint(w, &ps);
        RECT r{};
        GetClientRect(w, &r);
        {
            Graphics g(dc);
            g.Clear(id == Corner ? bg : card);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            rounded(g, RectF(0, 0, float(r.right - 1), float(r.bottom - 1)), Color(255, 25, 38, 46),
                    7, true);
            wchar_t caption[1024]{};
            int selection = int(SendMessageW(w, CB_GETCURSEL, 0, 0));
            if (selection >= 0)
                SendMessageW(w, CB_GETLBTEXT, selection, reinterpret_cast<LPARAM>(caption));
            SolidBrush b(text);
            StringFormat format;
            format.SetLineAlignment(StringAlignmentCenter);
            format.SetTrimming(StringTrimmingEllipsisCharacter);
            format.SetFormatFlags(StringFormatFlagsNoWrap);
            g.DrawString(caption, -1, uiFont(std::max(11.0f, 14 * app.scale)),
                         RectF(11, 0, float(r.right - 42), float(r.bottom)), &format, &b);
            Pen arrow(muted, 1.5f);
            float x = float(r.right - 21), y = float(r.bottom) * 0.5f;
            g.DrawLine(&arrow, x - 4, y - 2, x, y + 2);
            g.DrawLine(&arrow, x, y + 2, x + 4, y - 2);
            if (GetFocus() == w) {
                Pen focus(mint, 1);
                g.DrawLine(&focus, 10.0f, float(r.bottom - 2), float(r.right - 10),
                           float(r.bottom - 2));
            }
        }
        if (msg != WM_PRINTCLIENT)
            EndPaint(w, &ps);
        return 0;
    }
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == CB_SETCURSEL || msg == WM_ENABLE) {
        auto result = DefSubclassProc(w, msg, wp, lp);
        InvalidateRect(w, nullptr, FALSE);
        return result;
    }
    return DefSubclassProc(w, msg, wp, lp);
}
LRESULT CALLBACK toggleProc(HWND w, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR id) {
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        PAINTSTRUCT ps{};
        HDC dc = msg == WM_PRINTCLIENT ? reinterpret_cast<HDC>(wp) : BeginPaint(w, &ps);
        RECT r{};
        GetClientRect(w, &r);
        {
            Graphics g(dc);
            g.Clear(id == Motion ? bg : card);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            float z = app.scale, width = 28 * z, height = 16 * z, y = (r.bottom - height) / 2;
            bool checked = SendMessageW(w, BM_GETCHECK, 0, 0) == BST_CHECKED;
            app.toggleTargets[int(id)] = checked;
            double progress = app.motion ? app.toggleMotion[int(id)] : (checked ? 1 : 0);
            rounded(g, RectF(0, y, width, height),
                    mixColor(Color(255, 42, 55, 65), Color(255, 54, 128, 105), progress),
                    height / 2);
            SolidBrush thumb(mixColor(muted, mint, progress));
            g.FillEllipse(&thumb, float(3 * z + (width - height) * progress), y + 3 * z,
                          height - 6 * z, height - 6 * z);
            wchar_t caption[256]{};
            GetWindowTextW(w, caption, 256);
            SolidBrush b(text);
            StringFormat f;
            f.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(caption, -1, uiFont(std::max(10.0f, 13 * z)),
                         RectF(width + 9 * z, 0, r.right - width - 9 * z, float(r.bottom)), &f, &b);
            if (GetFocus() == w)
                line(g, width + 9 * z, float(r.bottom - 1), float(r.right - 2), float(r.bottom - 1),
                     mint);
        }
        if (msg != WM_PRINTCLIENT)
            EndPaint(w, &ps);
        return 0;
    }
    if (msg == BM_SETCHECK || msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_ENABLE) {
        auto result = DefSubclassProc(w, msg, wp, lp);
        InvalidateRect(w, nullptr, FALSE);
        return result;
    }
    return DefSubclassProc(w, msg, wp, lp);
}
LRESULT CALLBACK volumeProc(HWND w, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        PAINTSTRUCT ps{};
        HDC dc = msg == WM_PRINTCLIENT ? reinterpret_cast<HDC>(wp) : BeginPaint(w, &ps);
        RECT r{};
        GetClientRect(w, &r);
        {
            Graphics g(dc);
            g.Clear(card);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            float y = float(r.bottom) / 2, left = 10, right = float(r.right - 10),
                  v = float(SendMessageW(w, TBM_GETPOS, 0, 0)) / 100, x = left + (right - left) * v;
            rounded(g, RectF(left, y - 2, right - left, 4), edge, 2);
            if (x > left)
                rounded(g, RectF(left, y - 2, std::max(4.0f, x - left), 4), mint, 2);
            if (GetCapture() == w) {
                SolidBrush glow(Color(40, 125, 242, 198));
                g.FillEllipse(&glow, x - 12, y - 12, 24.0f, 24.0f);
            }
            SolidBrush knob(IsWindowEnabled(w) ? mint : muted);
            g.FillEllipse(&knob, x - 6, y - 6, 12.0f, 12.0f);
        }
        if (msg != WM_PRINTCLIENT)
            EndPaint(w, &ps);
        return 0;
    }
    return DefSubclassProc(w, msg, wp, lp);
}
HWND control(int id, const wchar_t* cls, const wchar_t* caption, DWORD style = 0) {
    if (_wcsicmp(cls, L"COMBOBOX") == 0)
        style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    HWND w =
        CreateWindowExW(0, cls, caption, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, 0, 0, 1, 1,
                        app.window, reinterpret_cast<HMENU>(INT_PTR(id)), app.instance, nullptr);
    app.controls[id] = w;
    SetWindowTheme(w, _wcsicmp(cls, L"COMBOBOX") == 0 ? L"" : L"DarkMode_Explorer", nullptr);
    if (_wcsicmp(cls, L"BUTTON") == 0 && (style & BS_TYPEMASK) == BS_OWNERDRAW)
        SetWindowSubclass(w, controlProc, 1, DWORD_PTR(id));
    if (_wcsicmp(cls, L"BUTTON") == 0 && (style & BS_TYPEMASK) == BS_AUTOCHECKBOX)
        SetWindowSubclass(w, toggleProc, 1, DWORD_PTR(id));
    if (_wcsicmp(cls, L"COMBOBOX") == 0)
        SetWindowSubclass(w, comboProc, 1, DWORD_PTR(id));
    if (_wcsicmp(cls, TRACKBAR_CLASSW) == 0)
        SetWindowSubclass(w, volumeProc, 1, DWORD_PTR(id));
    return w;
}
void createControls() {
    control(Target, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL);
    control(Refresh, L"BUTTON", L"Refresh", BS_OWNERDRAW);
    control(Capture, L"BUTTON", L"Start capture", BS_OWNERDRAW);
    control(CapValue, L"EDIT", std::to_wstring(app.cap).c_str(), ES_NUMBER | ES_CENTER | WS_BORDER);
    SendMessageW(app.controls[CapValue], EM_SETLIMITTEXT, 4, 0);
    control(Preset, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL);
    for (auto p :
         {L"Presets", L"30", L"40", L"60", L"90", L"120", L"141", L"144", L"165", L"240", L"0"})
        SendMessageW(app.controls[Preset], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p));
    SendMessageW(app.controls[Preset], CB_SETCURSEL, 0, 0);
    control(Attach, L"BUTTON", L"Enable / apply limit", BS_OWNERDRAW);
    control(Disable, L"BUTTON", L"Disable", BS_OWNERDRAW);
    control(HookHud, L"BUTTON", L"Show in-game HUD (DX11 / DX12)", BS_AUTOCHECKBOX);
    SendMessageW(app.controls[HookHud], BM_SETCHECK, BST_CHECKED, 0);
    control(Monitor, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL);
    control(Borderless, L"BUTTON", L"Make borderless", BS_OWNERDRAW);
    control(Restore, L"BUTTON", L"Restore window", BS_OWNERDRAW);
    control(AutoBorderless, L"BUTTON", L"Auto borderless for allow-listed games", BS_AUTOCHECKBOX);
    SendMessageW(app.controls[AutoBorderless], BM_SETCHECK,
                 app.autoBorderless ? BST_CHECKED : BST_UNCHECKED, 0);
    control(Overlay, L"BUTTON", L"Show overlay", BS_OWNERDRAW);
    control(Corner, L"COMBOBOX", L"", CBS_DROPDOWNLIST);
    for (auto p : {L"Top left", L"Top right", L"Bottom left", L"Bottom right"})
        SendMessageW(app.controls[Corner], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p));
    SendMessageW(app.controls[Corner], CB_SETCURSEL, app.corner, 0);
    control(CaptureDetails, L"BUTTON", L"Capture details", BS_OWNERDRAW);
    control(Rules, L"BUTTON", L"Game rules", BS_OWNERDRAW);
    control(Export, L"BUTTON", L"Export CSV", BS_OWNERDRAW);
    control(Test, L"BUTTON", L"Open test scene", BS_OWNERDRAW);
    control(Help, L"BUTTON", L"Quick guide", BS_OWNERDRAW);
    control(GamingTab, L"BUTTON", L"Overview", BS_OWNERDRAW);
    control(AudioTab, L"BUTTON", L"Audio", BS_OWNERDRAW);
    control(LimiterTab, L"BUTTON", L"Frame limiter", BS_OWNERDRAW);
    control(BorderlessTab, L"BUTTON", L"Borderless play", BS_OWNERDRAW);
    control(Motion, L"BUTTON", L"Motion effects", BS_AUTOCHECKBOX);
    SendMessageW(app.controls[Motion], BM_SETCHECK, app.motion ? BST_CHECKED : BST_UNCHECKED, 0);
    control(FullGuide, L"BUTTON", L"Open full guide", BS_OWNERDRAW);
    control(Volume, TRACKBAR_CLASSW, L"", TBS_NOTICKS | TBS_HORZ);
    SendMessageW(app.controls[Volume], TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    control(BoostLevel, L"COMBOBOX", L"", CBS_DROPDOWNLIST);
    for (auto p : {L"+3 dB · gentle", L"+6 dB · stronger", L"+9 dB · high", L"+12 dB · maximum"})
        SendMessageW(app.controls[BoostLevel], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p));
    SendMessageW(app.controls[BoostLevel], CB_SETCURSEL, 0, 0);
    control(ApplyBoost, L"BUTTON", L"Apply boost", BS_OWNERDRAW);
    control(DisableBoost, L"BUTTON", L"Disable boost", BS_OWNERDRAW);
    control(AudioSetup, L"BUTTON", L"Setup instructions", BS_OWNERDRAW);
    control(AudioRefresh, L"BUTTON", L"Refresh audio", BS_OWNERDRAW);
    control(AudioFolder, L"BUTTON", L"Find APO config", BS_OWNERDRAW);
    control(SoundSettings, L"BUTTON", L"Windows sound settings", BS_OWNERDRAW);
    control(OverlayTab, L"BUTTON", L"Overlay Studio", BS_OWNERDRAW);
    control(CompatibilityTab, L"BUTTON", L"Compatibility", BS_OWNERDRAW);
    control(HudMode, L"COMBOBOX", L"", CBS_DROPDOWNLIST);
    for (auto label : {L"Auto", L"Desktop", L"In-game"})
        SendMessageW(app.controls[HudMode], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    control(HudStyle, L"COMBOBOX", L"", CBS_DROPDOWNLIST);
    for (auto label : {L"Glass", L"Compact", L"Telemetry"})
        SendMessageW(app.controls[HudStyle], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    control(HudSize, L"COMBOBOX", L"", CBS_DROPDOWNLIST);
    for (auto label : {L"Auto", L"50%", L"75%", L"90%", L"100%", L"110%", L"125%", L"150%", L"175%",
                       L"200%", L"250%", L"300%"})
        SendMessageW(app.controls[HudSize], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    control(HudOpacity, L"COMBOBOX", L"", CBS_DROPDOWNLIST);
    for (auto label : {L"30%", L"45%", L"60%", L"75%", L"85%", L"90%", L"95%", L"100%"})
        SendMessageW(app.controls[HudOpacity], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    control(HudAccent, L"COMBOBOX", L"", CBS_DROPDOWNLIST);
    for (auto label : {L"Mint", L"Ice blue", L"Violet", L"Amber"})
        SendMessageW(app.controls[HudAccent], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    control(HudPosition, L"COMBOBOX", L"", CBS_DROPDOWNLIST);
    for (auto label : {L"Top left", L"Top right", L"Bottom left", L"Bottom right"})
        SendMessageW(app.controls[HudPosition], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    control(DesktopSize, L"COMBOBOX", L"", CBS_DROPDOWNLIST);
    for (auto label :
         {L"Auto · fit this screen", L"80%", L"90%", L"100%", L"110%", L"125%", L"150%", L"175%"})
        SendMessageW(app.controls[DesktopSize], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    for (int i = 0; i < 10; ++i)
        control(HudFirstField + i, L"BUTTON", hudTitles[i], BS_AUTOCHECKBOX);
    control(HudToggle, L"BUTTON", L"Show overlay", BS_OWNERDRAW);
    control(HudReset, L"BUTTON", L"Reset", BS_OWNERDRAW);
    control(CreateShortcut, L"BUTTON", L"Create shortcut", BS_OWNERDRAW);
    control(CompatibilityRefresh, L"BUTTON", L"Refresh compatibility", BS_OWNERDRAW);
    control(CompatibilityBorderless, L"BUTTON", L"Open borderless controls", BS_OWNERDRAW);
    syncHudControls();
    refreshTargets();
    refreshMonitors();
    layout();
    switchPage(0);
}
LRESULT CALLBACK mainProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    PerfMeter perf(app.messageMilliseconds);
    ++app.messageCount;
    if (app.trayRestartMessage && msg == app.trayRestartMessage) {
        app.trayRegistered = false;
        updateTray();
        return 0;
    }
    switch (msg) {
    case WM_CREATE:
        app.window = window;
        createControls();
        updateTray();
        SetTimer(window, 1, 250, nullptr);
        app.animationRunning = app.animation.start(window);
        if (!app.animationRunning)
            SetTimer(window, 2, 10, nullptr);
        updateAnimationState();
        return 0;
    case fg::AnimationPump::message:
        ++app.animationFrames;
        advanceMotion();
        if (!app.motion || IsIconic(window) ||
            (GetForegroundWindow() != window && !app.diagnosticAnimate))
            app.animation.painted();
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && !app.diagnosticAnimate)
            app.animation.configure(false, app.animationHz);
        else
            updateAnimationState();
        return 0;
    case WM_SHOWWINDOW:
        if (!wp)
            app.animation.configure(false, app.animationHz);
        else
            updateAnimationState();
        break;
    case WM_MOVE:
        updateAnimationState();
        break;
    case WM_TIMER:
        if (app.shuttingDown)
            return 0;
        if (wp == 1)
            update();
        else if (wp == 2)
            advanceMotion();
        return 0;
    case WM_APP + 20:
        if (lp == WM_LBUTTONDBLCLK)
            showAppWindow();
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 401, L"Open Framoryx");
            AppendMenuW(menu, MF_STRING, 402, L"Hide window");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, 403,
                        app.capture.running() ? L"Stop capture" : L"Start capture");
            AppendMenuW(menu, MF_STRING, 404, app.overlay ? L"Hide overlay" : L"Show overlay");
            AppendMenuW(menu, MF_STRING, 405, L"Overlay Studio");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, 406, L"Exit Framoryx");
            POINT position{};
            GetCursorPos(&position);
            SetForegroundWindow(window);
            int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, position.x,
                                         position.y, 0, window, nullptr);
            DestroyMenu(menu);
            if (command)
                SendMessageW(window, WM_COMMAND, command, 0);
            PostMessageW(window, WM_NULL, 0, 0);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lp) == app.controls[Volume]) {
            float v = float(SendMessageW(app.controls[Volume], TBM_GETPOS, 0, 0)) / 100;
            app.audio.volume = v;
            auto now = GetTickCount64();
            if (LOWORD(wp) != TB_THUMBTRACK || now - app.lastVolumeWrite >= 30) {
                if (!fg::setAudioVolume(v))
                    app.audioNotice = L"Windows could not change this output’s volume.";
                app.lastVolumeWrite = now;
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window, &ps);
        paintMain(dc, ps.rcPaint);
        EndPaint(window, &ps);
        app.animation.painted();
        return 0;
    }
    case WM_SIZE:
        if (app.transitionNew) {
            app.transitionOld.reset();
            app.transitionNew.reset();
            pageControls(app.page);
        }
        if (app.controls.size())
            layout();
        return 0;
    case WM_DISPLAYCHANGE:
        updateAnimationState(true);
        refreshMonitors();
        if (app.desktopPercent == 0)
            applyDesktopScale();
        positionHud();
        return 0;
    case WM_DPICHANGED: {
        auto* rect = reinterpret_cast<RECT*>(lp);
        SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left,
                     rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        applyDesktopScale();
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto p = reinterpret_cast<MINMAXINFO*>(lp);
        MONITORINFO m{};
        m.cbSize = sizeof(m);
        GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &m);
        p->ptMinTrackSize.x = std::min(860L, m.rcWork.right - m.rcWork.left - 20);
        p->ptMinTrackSize.y = std::min(630L, m.rcWork.bottom - m.rcWork.top - 20);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
        SetTextColor(reinterpret_cast<HDC>(wp), RGB(233, 242, 246));
        SetBkColor(reinterpret_cast<HDC>(wp), RGB(20, 28, 36));
        return reinterpret_cast<LRESULT>(app.brush);
    case WM_MEASUREITEM: {
        auto m = reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        m->itemHeight = std::max(18, int(27 * app.scale));
        return TRUE;
    }
    case WM_DRAWITEM: {
        auto d = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (d->CtlType == ODT_COMBOBOX) {
            HBRUSH brush =
                CreateSolidBrush((d->itemState & ODS_SELECTED) ? RGB(33, 71, 61) : RGB(25, 37, 45));
            FillRect(d->hDC, &d->rcItem, brush);
            DeleteObject(brush);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, RGB(232, 243, 243));
            wchar_t caption[1024]{};
            if (d->itemID != UINT(-1))
                SendMessageW(d->hwndItem, CB_GETLBTEXT, d->itemID,
                             reinterpret_cast<LPARAM>(caption));
            RECT r = d->rcItem;
            r.left += 10;
            r.right -= 5;
            auto originalFont = SelectObject(d->hDC, app.controlFont);
            DrawTextW(d->hDC, caption, -1, &r,
                      DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(d->hDC, originalFont);
            return TRUE;
        }
        if (d->CtlType != ODT_BUTTON)
            break;
        Graphics g(d->hDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.Clear(card);
        bool nav = d->CtlID == GamingTab || d->CtlID == AudioTab || d->CtlID == LimiterTab ||
                   d->CtlID == BorderlessTab || d->CtlID == OverlayTab ||
                   d->CtlID == CompatibilityTab || d->CtlID == Rules || d->CtlID == Help;
        bool active =
            (d->CtlID == GamingTab && app.page == 0) || (d->CtlID == AudioTab && app.page == 1) ||
            (d->CtlID == LimiterTab && app.page == 2) ||
            (d->CtlID == BorderlessTab && app.page == 3) ||
            (d->CtlID == OverlayTab && app.page == 5) ||
            (d->CtlID == CompatibilityTab && app.page == 6) || (d->CtlID == Help && app.page == 4);
        bool primary = d->CtlID == Capture || d->CtlID == ApplyBoost || d->CtlID == HudToggle ||
                       (d->CtlID == AudioSetup && !app.boost.available()),
             disabled = d->itemState & ODS_DISABLED, pressed = d->itemState & ODS_SELECTED;
        float width = float(d->rcItem.right - d->rcItem.left),
              height = float(d->rcItem.bottom - d->rcItem.top);
        double warmth = app.hoverMotion[int(d->CtlID)];
        Color base = nav       ? (active ? Color(255, 28, 59, 52) : bg)
                     : primary ? mint
                               : Color(255, 27, 40, 48);
        Color hot = nav       ? (active ? Color(255, 33, 72, 61) : Color(255, 25, 37, 44))
                    : primary ? Color(255, 161, 255, 219)
                              : Color(255, 40, 59, 64);
        Color buttonColor = disabled  ? Color(255, 22, 29, 35)
                            : pressed ? Color(255, 49, 96, 80)
                                      : mixColor(base, hot, warmth);
        g.Clear(nav ? bg : card);
        rounded(g, RectF(0, 0, width - 1, height - 1), buttonColor, 9, !nav && !primary);
        if (!nav && !disabled && warmth > .01) {
            Pen highlight(Color(BYTE(40 * warmth), 218, 255, 238), 1);
            g.DrawLine(&highlight, 11.f, 1.f, width - 12, 1.f);
        }
        if (active) {
            Pen accent(mint, 2.5f);
            g.DrawLine(&accent, 2.0f, 10.0f, 2.0f, height - 10);
        }
        wchar_t caption[128]{};
        GetWindowTextW(d->hwndItem, caption, 128);
        SolidBrush b(disabled ? muted : (active ? mint : (primary ? bg : (nav ? muted : text))));
        StringFormat f;
        f.SetAlignment(nav ? StringAlignmentNear : StringAlignmentCenter);
        f.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(caption, -1, uiFont(std::max(11.0f, 13 * app.scale), true),
                     RectF(nav ? 17.0f : 1.0f, 0, width - (nav ? 22.0f : 2.0f), height), &f, &b);
        if (d->itemState & ODS_FOCUS) {
            Pen p(mint, 1);
            g.DrawRectangle(&p, 3.0f, 3.0f, width - 7, height - 7);
        }
        return TRUE;
    }
    case WM_HOTKEY:
        if (wp == 1)
            SendMessageW(window, WM_COMMAND, Overlay, 0);
        if (wp == 2)
            makeBorderless();
        if (wp == 3) {
            app.hook.disable();
            setNotice(L"Hook disabled. Restart the game to remove the DLL completely.");
        }
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp), notification = HIWORD(wp);
        if (id >= 401 && id <= 406) {
            if (id == 401)
                showAppWindow();
            if (id == 402)
                ShowWindow(window, SW_HIDE);
            if (id == 403)
                SendMessageW(window, WM_COMMAND, Capture, 0);
            if (id == 404)
                SendMessageW(window, WM_COMMAND, Overlay, 0);
            if (id == 405) {
                showAppWindow();
                switchPage(5);
            }
            if (id == 406)
                PostMessageW(window, WM_CLOSE, 0, 0);
            return 0;
        }
        if (id == Target && notification == CBN_SELCHANGE) {
            app.stats = {};
            app.metricFps = app.metricAverage = app.metricLow = app.metricMs = 0;
            applyProfile(selected() ? selected()->exe : app.autoExe);
            if (selected()) {
                auto profile = app.gameLimits.find(fg::lower(selected()->exe));
                if (profile != app.gameLimits.end()) {
                    app.cap = profile->second;
                    SetWindowTextW(app.controls[CapValue], std::to_wstring(app.cap).c_str());
                }
            }
            update();
            return 0;
        }
        if (id == Corner && notification == CBN_SELCHANGE) {
            app.corner = int(SendMessageW(app.controls[Corner], CB_GETCURSEL, 0, 0));
            storeHudProfile();
            positionHud();
            return 0;
        }
        if (id == Preset && notification == CBN_SELCHANGE) {
            int i = int(SendMessageW(app.controls[Preset], CB_GETCURSEL, 0, 0));
            if (i > 0) {
                wchar_t val[32]{};
                SendMessageW(app.controls[Preset], CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(val));
                SetWindowTextW(app.controls[CapValue], val);
                updateCap();
            }
            return 0;
        }
        if (id == CapValue && notification == EN_KILLFOCUS) {
            updateCap();
            return 0;
        }
        if (((id >= HudSize && id <= DesktopSize) || id == HudStyle) &&
            notification == CBN_SELCHANGE) {
            if (id == DesktopSize) {
                app.desktopPercent = comboValue(id);
                saveSettings();
                applyDesktopScale();
                return 0;
            }
            if (id == HudSize)
                app.hudOptions.scale = comboValue(id);
            if (id == HudOpacity)
                app.hudOptions.opacity = comboValue(id);
            if (id == HudAccent)
                app.hudOptions.accent = int(SendMessageW(app.controls[id], CB_GETCURSEL, 0, 0));
            if (id == HudMode)
                app.hudMode = int(SendMessageW(app.controls[id], CB_GETCURSEL, 0, 0));
            if (id == HudPosition)
                app.corner = int(SendMessageW(app.controls[id], CB_GETCURSEL, 0, 0));
            if (id == HudStyle)
                app.hudOptions.style = int(SendMessageW(app.controls[id], CB_GETCURSEL, 0, 0));
            storeHudProfile();
            return 0;
        }
        if (id >= HudFirstField && id < HudFirstField + 10 && notification == BN_CLICKED) {
            app.hudOptions.fields = 0;
            for (int i = 0; i < 10; ++i)
                if (SendMessageW(app.controls[HudFirstField + i], BM_GETCHECK, 0, 0) == BST_CHECKED)
                    app.hudOptions.fields |= hudFields[i];
            storeHudProfile();
            return 0;
        }
        if (notification != BN_CLICKED)
            break;
        switch (id) {
        case CreateShortcut: {
            auto error = createDesktopShortcut();
            setNotice(error.empty()
                          ? L"Desktop shortcut created. Open it any time to return to Framoryx."
                          : error);
            break;
        }
        case OverlayTab:
            switchPage(5);
            break;
        case CompatibilityTab:
            app.compatibilityPid = ~DWORD(0);
            app.compatibilityGuard = fg::antiCheatRunning();
            update();
            switchPage(6);
            break;
        case CompatibilityRefresh:
            app.compatibilityPid = ~DWORD(0);
            app.compatibilityGuard = fg::antiCheatRunning();
            update();
            InvalidateRect(window, nullptr, FALSE);
            break;
        case CompatibilityBorderless:
            switchPage(3);
            break;
        case HudReset:
            if (app.activeProfile.empty())
                app.hudBase = {};
            else
                app.hudProfiles.erase(app.activeProfile);
            applyProfile(app.activeProfile);
            saveSettings();
            pushHookPresentation();
            InvalidateRect(window, nullptr, FALSE);
            break;
        case HudToggle:
            if (app.hudMode == 2 && !app.hook.active()) {
                attachHook();
                syncHudControls();
                break;
            }
            SendMessageW(window, WM_COMMAND, Overlay, 0);
            syncHudControls();
            break;
        case GamingTab:
            switchPage(0);
            break;
        case LimiterTab:
            switchPage(2);
            break;
        case BorderlessTab:
            switchPage(3);
            break;
        case Motion:
            app.motion = SendMessageW(app.controls[Motion], BM_GETCHECK, 0, 0) == BST_CHECKED;
            updateAnimationState();
            advanceMotion();
            saveSettings();
            InvalidateRect(window, nullptr, FALSE);
            break;
        case FullGuide: {
            auto path = fg::executableDir() + L"\\README.txt";
            ShellExecuteW(window, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        case AudioTab:
            switchPage(1);
            break;
        case AudioRefresh:
            app.audio = fg::audioState();
            SendMessageW(app.controls[Volume], TBM_SETPOS, TRUE, int(app.audio.volume * 100));
            app.audioNotice = L"Output device and volume refreshed.";
            InvalidateRect(window, nullptr, FALSE);
            break;
        case ApplyBoost: {
            app.audio = fg::audioState();
            int db = 3 * (1 + int(SendMessageW(app.controls[BoostLevel], CB_GETCURSEL, 0, 0)));
            auto err = app.boost.apply(db, app.audio.guid);
            app.audioNotice = err.empty() ? L"Saved +" + std::to_wstring(db) + L" dB for " +
                                                app.audio.device +
                                                L". Requires Equalizer APO enabled for this "
                                                L"device; restart audio playback if necessary."
                                          : err;
            InvalidateRect(window, nullptr, FALSE);
            break;
        }
        case DisableBoost: {
            auto err = app.boost.disable();
            app.audioNotice =
                err.empty() ? L"Boost disabled (0 dB). Other audio effects are unchanged." : err;
            InvalidateRect(window, nullptr, FALSE);
            break;
        }
        case AudioSetup:
            MessageBoxW(
                window,
                L"Normal volume: drag the slider to change your current Windows output.\n\nBoost "
                L"above 100%:\n1. Get Equalizer APO from its official SourceForge project. Install "
                L"it and enable your playback device in its Configurator. Restart Windows if "
                L"requested.\n2. Return here. If your installation is elsewhere, use Find APO "
                L"config and choose its config.txt.\n3. Choose +3 dB to start, then Apply boost. "
                L"The config folder may require Run as administrator. Framoryx backs up config.txt "
                L"before adding its own Include line.\n4. Click Disable boost to set its gain to 0 "
                L"dB. Boost persists after Framoryx closes.\n\nThis build does not install an "
                L"audio driver, certify your device’s APO setup, or provide a compressor. Positive "
                L"gain can clip.\n\nOfficial project: "
                L"https://sourceforge.net/projects/equalizerapo/",
                L"Framoryx audio setup", MB_OK | MB_ICONINFORMATION);
            break;
        case AudioFolder: {
            wchar_t path[32768]{};
            OPENFILENAMEW of{};
            of.lStructSize = sizeof(of);
            of.hwndOwner = window;
            of.lpstrFilter = L"Equalizer APO configuration\0config.txt\0\0";
            of.lpstrFile = path;
            of.nMaxFile = 32768;
            of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&of)) {
                fs::path config(path);
                if (_wcsicmp(config.filename().c_str(), L"config.txt") == 0) {
                    app.boost.directory(config.parent_path().wstring());
                    saveSettings();
                    app.audioNotice = L"APO config folder selected.";
                    EnableWindow(app.controls[ApplyBoost],
                                 app.audio.available && app.boost.available());
                } else
                    app.audioNotice = L"Select Equalizer APO’s config.txt file.";
                InvalidateRect(window, nullptr, FALSE);
            }
            break;
        }
        case SoundSettings:
            ShellExecuteW(window, L"open", L"ms-settings:sound", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case Capture:
            if (app.capture.running()) {
                app.capture.stop();
                setNotice(app.capture.status());
            } else {
                auto err = app.capture.start(app.rules);
                setNotice(err.empty() ? app.capture.status() : err);
            }
            break;
        case Refresh:
            refreshTargets();
            setNotice(L"Window list refreshed. Select a specific window for limiter and borderless "
                      L"controls.");
            break;
        case Overlay:
            app.overlay = !app.overlay;
            if (app.overlay && !app.capture.running()) {
                auto error = app.capture.start(app.rules);
                if (!error.empty())
                    setNotice(error);
            }
            pushHookPresentation();
            syncHudControls();
            SetWindowTextW(app.controls[Overlay], app.overlay ? L"Hide overlay" : L"Show overlay");
            positionHud();
            setNotice(app.overlay ? L"Overlay on. Ctrl+Alt+O toggles it. If hidden by fullscreen, "
                                    L"use Borderless play or opt into the DX11 HUD."
                                  : L"Desktop overlay hidden.");
            break;
        case Rules:
            showRules();
            break;
        case Export:
            exportCSV();
            break;
        case CaptureDetails: {
            std::wstring details = app.capture.status() + L"\n\n" + app.capture.details() +
                                   L"\n\nIf ETW access is denied, use Run as administrator or an "
                                   L"account in Performance Log Users. A game’s anti-cheat may "
                                   L"also restrict tracing. No bypass is attempted.";
            MessageBoxW(window, details.c_str(), L"Framoryx — Capture details",
                        MB_OK | MB_ICONINFORMATION);
            break;
        }
        case Attach:
            if (app.hook.active() && selected() && selected()->pid == app.hook.pid()) {
                if (updateCap())
                    setNotice(L"FPS limit updated for the attached process.");
            } else
                attachHook();
            break;
        case Disable:
            app.hook.disable();
            setNotice(L"Hook disabled. Restart the hooked game before reattaching or playing with "
                      L"anti-cheat.");
            break;
        case HookHud:
            app.hookHud = SendMessageW(app.controls[HookHud], BM_GETCHECK, 0, 0) == BST_CHECKED;
            app.hook.overlay(app.hookHud);
            break;
        case Borderless:
            makeBorderless();
            break;
        case Restore:
            if (selected())
                setNotice(app.borderless.restore(selected()->hwnd)
                              ? L"Original window restored."
                              : L"No saved window state, or Windows prevented restoration.");
            else
                setNotice(L"Select the window you want to restore.");
            break;
        case AutoBorderless:
            app.autoBorderless =
                SendMessageW(app.controls[AutoBorderless], BM_GETCHECK, 0, 0) == BST_CHECKED;
            saveSettings();
            setNotice(app.autoBorderless ? L"Auto borderless enabled for allow-listed games only. "
                                           L"Process rules controls that list."
                                         : L"Automatic borderless changes disabled.");
            break;
        case Test:
            launchDemo();
            break;
        case Help:
            switchPage(4);
            break;
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        app.shuttingDown = true;
        app.animation.stop();
        {
            NOTIFYICONDATAW icon{};
            icon.cbSize = sizeof(icon);
            icon.hWnd = window;
            icon.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &icon);
            app.trayRegistered = false;
        }
        KillTimer(window, 1);
        KillTimer(window, 2);
        app.hook.disable();
        app.borderless.restoreAll();
        app.capture.stop();
        app.demoJob.reset();
        app.demoProcess.reset();
        for (int i = 1; i <= 3; ++i)
            UnregisterHotKey(window, i);
        if (app.hud)
            DestroyWindow(app.hud);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    app.instance = instance;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int diagnosticCount = 0;
    auto diagnosticArgs = CommandLineToArgvW(GetCommandLineW(), &diagnosticCount);
    bool smokeArgument = false, reportArgument = false, settingsArgument = false;
    for (int i = 1; i < diagnosticCount; ++i) {
        std::wstring argument = diagnosticArgs[i];
        if (argument == L"--smoke-seconds" && i + 1 < diagnosticCount) {
            auto n = fg::number(fg::utf8(diagnosticArgs[i + 1]));
            smokeArgument = n && *n > 0 && *n <= 120;
        }
        if (argument == L"--report" && i + 1 < diagnosticCount)
            reportArgument = true;
        if (argument == L"--settings-file" && i + 1 < diagnosticCount)
            settingsArgument = true;
    }
    if (diagnosticArgs)
        LocalFree(diagnosticArgs);
    bool diagnosticInstance = smokeArgument && reportArgument && settingsArgument;
    auto mutexName = diagnosticInstance
                         ? L"Local\\Framoryx.Validation." + std::to_wstring(GetCurrentProcessId())
                         : L"Local\\Framoryx.Desktop";
    fg::Handle singleton(CreateMutexW(nullptr, TRUE, mutexName.c_str()));
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"Framoryx.Desktop", nullptr);
        if (existing) {
            ShowWindowAsync(existing, IsIconic(existing) ? SW_RESTORE : SW_SHOW);
            SetForegroundWindow(existing);
        }
        return 0;
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&common);
    GdiplusStartupInput startup;
    ULONG_PTR token = 0;
    if (GdiplusStartup(&token, &startup, nullptr) != Ok)
        return 1;
    BOOL animations = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    app.motion = animations;
    int earlyCount = 0;
    auto earlyArguments = CommandLineToArgvW(GetCommandLineW(), &earlyCount);
    for (int i = 1; i + 1 < earlyCount; ++i)
        if (std::wstring(earlyArguments[i]) == L"--settings-file")
            app.settingsOverride = earlyArguments[++i];
    if (earlyArguments)
        LocalFree(earlyArguments);
    loadSettings();
    app.hudOptions = app.hudBase.options;
    app.hudMode = app.hudBase.mode;
    app.corner = app.hudBase.corner;
    int argumentCount = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    for (int i = 1; i < argumentCount; ++i) {
        std::wstring argument = arguments[i];
        if (argument == L"--report" && i + 1 < argumentCount)
            app.diagnosticPath = arguments[++i];
        else if (argument == L"--snapshot" && i + 1 < argumentCount)
            app.snapshotPath = arguments[++i];
        else if (argument == L"--smoke-seconds" && i + 1 < argumentCount) {
            auto duration = fg::number(fg::utf8(arguments[++i]));
            if (duration && *duration >= 1 && *duration <= 120)
                app.smokeSeconds = *duration;
        } else if (argument == L"--reduced-motion")
            app.motion = false;
    }
    for (int i = 1; i + 1 < argumentCount; ++i)
        if (std::wstring(arguments[i]) == L"--smoke-page") {
            auto page = fg::number(fg::utf8(arguments[++i]));
            if (page && *page >= 0 && *page <= 6)
                app.fixedSmokePage = int(*page);
        }
    for (int i = 1; i < argumentCount; ++i) {
        if (std::wstring(arguments[i]) == L"--with-demo")
            app.smokeDemo = true;
        if (std::wstring(arguments[i]) == L"--smoke-resize")
            app.smokeResize = true;
        if (std::wstring(arguments[i]) == L"--smoke-customize" && !app.settingsOverride.empty())
            app.smokeCustomize = true;
        if (std::wstring(arguments[i]) == L"--smoke-animate" && app.smokeSeconds > 0 &&
            !app.diagnosticPath.empty())
            app.diagnosticAnimate = true;
    }
    LocalFree(arguments);
    app.diagnosticStart = fg::clockSeconds();
    app.trayRestartMessage = RegisterWindowMessageW(L"TaskbarCreated");
    app.brush = CreateSolidBrush(RGB(20, 28, 36));
    app.artwork = std::make_unique<Image>((fg::executableDir() + L"\\assets\\artwork.png").c_str());
    app.logo = std::make_unique<Image>((fg::executableDir() + L"\\assets\\icon.png").c_str());
    HICON icon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = icon;
    wc.hIconSm = icon;
    wc.lpfnWndProc = mainProc;
    auto mainClass = diagnosticInstance ? L"Framoryx.Validation" : L"Framoryx.Desktop";
    wc.lpszClassName = mainClass;
    RegisterClassExW(&wc);
    wc.lpfnWndProc = rulesProc;
    wc.lpszClassName = L"Framoryx.Rules";
    wc.hbrBackground = HBRUSH(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);
    wc.lpfnWndProc = hudProc;
    wc.lpszClassName = L"Framoryx.HUD";
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
    int availableW = mi.rcWork.right - mi.rcWork.left,
        availableH = mi.rcWork.bottom - mi.rcWork.top;
    float initialScale =
        fg::desktopScale(app.desktopPercent, float(GetDpiForSystem()), availableW, availableH);
    RECT r{0, 0, int(BW * initialScale), int(BH * initialScale)};
    AdjustWindowRectExForDpi(&r, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForSystem());
    int w = r.right - r.left, h = r.bottom - r.top;
    HWND window = CreateWindowExW(
        0, mainClass, diagnosticInstance ? L"Framoryx — Validation" : L"Framoryx — Gaming toolkit",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, mi.rcWork.left + (availableW - w) / 2,
        mi.rcWork.top + (availableH - h) / 2, w, h, nullptr, nullptr, instance, nullptr);
    if (!window)
        return 1;
    RECT initialWindow{};
    GetWindowRect(window, &initialWindow);
    app.smokeOuterWidth = initialWindow.right - initialWindow.left;
    app.smokeOuterHeight = initialWindow.bottom - initialWindow.top;
    BOOL dark = TRUE;
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    app.hud = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED |
                                  WS_EX_TRANSPARENT,
                              L"Framoryx.HUD", L"Framoryx overlay", WS_POPUP, 0, 0, 270, 134,
                              nullptr, nullptr, instance, nullptr);

    bool hotkeys = RegisterHotKey(window, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'O');
    hotkeys &= RegisterHotKey(window, 2, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'B');
    hotkeys &= RegisterHotKey(window, 3, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'L');
    if (!hotkeys)
        app.notice = L"Some hotkeys are already used by another app. All features remain available "
                     L"through the buttons.";
    ShowWindow(window, show);
    UpdateWindow(window);
    if (app.smokeDemo)
        launchDemo();
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        bool keyboard = msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST;
        HWND dialog = window;
        if (keyboard && msg.hwnd) {
            HWND top = GetAncestor(msg.hwnd, GA_ROOT);
            if (top != window && GetWindow(top, GW_OWNER) == window)
                dialog = top;
        }
        if (!keyboard || !IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (!app.diagnosticPath.empty()) {
        FILETIME created{}, exited{}, kernel{}, user{};
        GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
        auto seconds = [](FILETIME t) {
            return double((ULONGLONG(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 10000000.0;
        };
        double elapsed = fg::clockSeconds() - app.diagnosticStart;
        DWORD handles = 0;
        GetProcessHandleCount(GetCurrentProcess(), &handles);
        std::ofstream report{fs::path(app.diagnosticPath), std::ios::binary};
        report << std::fixed << std::setprecision(3) << "{\n  \"elapsedSeconds\": " << elapsed
               << ",\n  \"processCpuSeconds\": " << seconds(kernel) + seconds(user)
               << ",\n  \"kernelCpuSeconds\": " << seconds(kernel)
               << ",\n  \"userCpuSeconds\": " << seconds(user) << ",\n  \"oneCorePercent\": "
               << 100 * (seconds(kernel) + seconds(user)) / std::max(0.001, elapsed)
               << ",\n  \"messageCount\": " << app.messageCount
               << ",\n  \"messageMs\": " << app.messageMilliseconds
               << ",\n  \"updateMs\": " << app.updateMilliseconds
               << ",\n  \"paints\": " << app.paintCount
               << ",\n  \"fullPaints\": " << app.fullPaintCount << ",\n  \"averagePaintMs\": "
               << app.paintMilliseconds / std::max<ULONGLONG>(1, app.paintCount)
               << ",\n  \"maxPaintMs\": " << app.maxPaintMs
               << ",\n  \"foregroundSamples\": " << app.foregroundSamples
               << ",\n  \"samples\": " << app.sampleCount
               << ",\n  \"initialGdiObjects\": " << app.initialGdi
               << ",\n  \"peakGdiObjects\": " << app.peakGdi << ",\n  \"finalGdiObjects\": "
               << GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS)
               << ",\n  \"peakHandles\": " << app.peakHandles
               << ",\n  \"finalHandles\": " << handles
               << ",\n  \"demoLiveSamples\": " << app.demoLiveSamples
               << ",\n  \"transitionsCompleted\": " << app.transitionsCompleted
               << ",\n  \"animationFrames\": " << app.animationFrames
               << ",\n  \"animationRefreshHz\": " << app.animationHz
               << ",\n  \"customizationChecks\": " << app.customizationChecks
               << ",\n  \"customizationRoundTrip\": "
               << (app.customizationRoundTrip ? "true" : "false")
               << ",\n  \"effectiveUiScale\": " << app.scale << ",\n  \"cpuSamplesSeconds\": [";
        for (size_t i = 0; i < app.cpuSamples.size(); ++i) {
            if (i)
                report << ", ";
            report << app.cpuSamples[i];
        }
        report << "]\n}\n";
    }
    app.transitionOld.reset();
    app.transitionNew.reset();
    app.fonts.clear();
    app.headerCache.reset();
    app.logoCache.reset();
    app.logo.reset();
    app.artwork.reset();
    if (app.bufferDC) {
        SelectObject(app.bufferDC, app.bufferOriginal);
        DeleteObject(app.bufferBitmap);
        DeleteDC(app.bufferDC);
    }
    if (app.contentDC) {
        SelectObject(app.contentDC, app.contentOriginal);
        DeleteObject(app.contentBitmap);
        DeleteDC(app.contentDC);
    }
    if (app.controlFont)
        DeleteObject(app.controlFont);
    DeleteObject(app.brush);
    GdiplusShutdown(token);
    CoUninitialize();
    return (app.smokeDemo && app.smokeSeconds > 0 && !app.demoLiveSamples) ||
                   (app.smokeCustomize && app.customizationChecks < 4)
               ? 1
               : 0;
}
