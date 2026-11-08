#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fg {
enum HudField : uint32_t {
    HudFps = 1,
    HudAverage = 2,
    HudLow = 4,
    HudTime = 8,
    HudGraph = 16,
    HudCpu = 32,
    HudRam = 64,
    HudCap = 128,
    HudApi = 256,
    HudBrand = 512,
    HudAll = 1023
};
struct HudOptions {
    uint32_t fields = HudFps | HudLow | HudTime | HudGraph | HudBrand;
    int scale = 0, opacity = 90, accent = 0, style = 0;
    void validate() {
        fields &= HudAll;
        scale = scale == 0 ? 0 : std::clamp(scale, 50, 300);
        opacity = std::clamp(opacity, 30, 100);
        accent = std::clamp(accent, 0, 3);
        style = std::clamp(style, 0, 2);
    }
};
struct HudProfile {
    HudOptions options;
    int corner = 0, mode = 0;
}; // auto, desktop, attached in-game
inline float overlayScale(int percent, int renderHeight) {
    return percent ? std::clamp(percent / 100.f, .5f, 3.f)
                   : std::clamp(renderHeight / 1080.f, .85f, 2.5f);
}
inline float desktopScale(int percent, float dpi, int availableWidth, int availableHeight) {
    float fit =
        std::max(.35f, std::min((availableWidth - 48) / 1180.f, (availableHeight - 80) / 820.f));
    return std::min(fit, std::max(.5f, dpi / 96.f) * (percent ? percent / 100.f : 1.f));
}
inline float hudHeight(uint32_t fields) {
    float height = 26;
    if (fields & HudBrand)
        height += 21;
    if (fields & HudFps)
        height += 49;
    for (auto field : {HudAverage, HudLow, HudTime, HudCpu, HudRam, HudCap, HudApi})
        if (fields & field)
            height += 21;
    if (fields & HudGraph)
        height += 47;
    return height;
}
inline float hudWidth(const HudOptions& options) {
    if (options.style == 1)
        return 230;
    if (options.style != 2)
        return 260;
    float width = 28;
    if (options.fields & HudFps)
        width += 152;
    for (auto field : {HudAverage, HudLow, HudTime, HudCpu, HudRam, HudCap, HudApi})
        if (options.fields & field)
            width += 104;
    return std::max(260.f, width);
}
inline float hudHeight(const HudOptions& options) {
    if (options.style == 0)
        return hudHeight(options.fields);
    if (options.style == 2)
        return 26 + ((options.fields & HudBrand) ? 18 : 0) +
               ((options.fields & ~uint32_t(HudBrand | HudGraph)) ? 50 : 0) +
               ((options.fields & HudGraph) ? 42 : 0);
    float height = 20;
    if (options.fields & HudBrand)
        height += 16;
    if (options.fields & HudFps)
        height += 39;
    for (auto field : {HudAverage, HudLow, HudTime, HudCpu, HudRam, HudCap, HudApi})
        if (options.fields & field)
            height += 18;
    if (options.fields & HudGraph)
        height += 36;
    return height;
}
inline double easeOut(double t) {
    t = std::clamp(t, 0., 1.);
    return 1 - std::pow(1 - t, 3);
}
inline double approach(double current, double target, double delta, double rate = 18) {
    if (!std::isfinite(current))
        return target;
    double next = target + (current - target) * std::exp(-rate * std::clamp(delta, 0., .1));
    return std::abs(next - target) < .001 ? target : next;
}
} // namespace fg
