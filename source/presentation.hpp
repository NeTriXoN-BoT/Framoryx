#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <string>

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
    HudGpu = 1024, HudVram = 2048, HudGpuTemp = 4096, HudCpuTemp = 8192, HudPower = 16384, HudClock = 32768,
    HudAll = 65535
};
inline constexpr uint32_t hudTemperatureColorsFlag = 1u << 16;
inline constexpr uint32_t hudTextGlowFlag = 1u << 17;
inline constexpr uint32_t hudValueColorsFlag = 1u << 18;
inline constexpr int hudRainbowAccent = 10;
inline constexpr uint64_t hudRainbowCycleMs = 3200;
inline constexpr std::array<uint32_t, 6> hudRainbowStops{
    0xff94ae, 0xffca83, 0x9af2b4, 0x80defb, 0xc7a5ff, 0xff94ae
};
inline constexpr float hudRainbowPhase(uint64_t milliseconds) {
    return float(milliseconds % hudRainbowCycleMs) / float(hudRainbowCycleMs);
}
inline uint32_t hudRainbowColor(float phase) {
    const float position = (phase >= 0.f && phase < 2.f ? (phase >= 1.f ? phase - 1.f : phase) : 0.f) *
                           (hudRainbowStops.size() - 1);
    const size_t index = std::min(size_t(position), hudRainbowStops.size() - 2);
    const float fraction = position - index;
    const auto from = hudRainbowStops[index], to = hudRainbowStops[index + 1];
    uint32_t result = 0;
    for (const int shift : {16, 8, 0}) {
        const int a = (from >> shift) & 255, b = (to >> shift) & 255;
        result |= uint32_t(std::lround(a + (b - a) * fraction)) << shift;
    }
    return result;
}
inline uint32_t hudTemperatureColor(double celsius) {
    if (!std::isfinite(celsius) || celsius < 0) return 0;
    if (celsius >= 80) return 0xff8b96;
    if (celsius >= 65) return 0xf7be6f;
    return 0x7df2c6;
}
inline uint32_t hudValueColor(uint32_t field, double value, bool valueColors,
                              bool temperatureColors, double totalRam = 0) {
    if (!std::isfinite(value) || value < 0)
        return 0;
    if (field == HudGpuTemp || field == HudCpuTemp)
        return valueColors || temperatureColors ? hudTemperatureColor(value) : 0;
    if (!valueColors)
        return 0;
    constexpr uint32_t mint = 0x7df2c6, amber = 0xf7be6f, coral = 0xff8b96;
    if (field == HudFps || field == HudAverage || field == HudLow || field == HudCap)
        return field == HudCap && value == 0 ? 0 : value >= 60 ? mint : value >= 30 ? amber : coral;
    if (field == HudTime)
        return value == 0 ? 0 : value <= 1000. / 60 ? mint : value <= 1000. / 30 ? amber : coral;
    if (field == HudRam) {
        if (!std::isfinite(totalRam) || totalRam <= 0)
            return 0;
        value = 100 * (value / totalRam);
    } else if (field != HudCpu && field != HudGpu && field != HudVram) {
        return 0;
    }
    return value >= 90 ? coral : value >= 65 ? amber : mint;
}
inline constexpr uint32_t desktopHudFields(uint32_t fields) {
    constexpr uint32_t hardware = HudCpu | HudRam | HudGpu | HudVram | HudGpuTemp |
                                  HudCpuTemp | HudPower | HudClock;
    const auto selected = fields & hardware;
    return (fields & HudBrand) | (selected ? selected : HudCpu | HudRam);
}
struct HudOptions {
    static constexpr uint64_t defaultOrder = 0xfedcba4876532109ull;
    uint32_t fields = HudFps | HudLow | HudTime | HudGraph | HudBrand;
    int scale = 0, opacity = 90, accent = 0, style = 0;
    uint64_t order = defaultOrder;
    int x = -1, y = -1, fontScale = 100;
    bool temperatureColors = false;
    bool textGlow = false;
    bool valueColors = false;
    void validate() {
        fields &= HudAll;
        scale = scale == 0 ? 0 : std::clamp(scale, 50, 300);
        opacity = std::clamp(opacity, 0, 100);
        accent = std::clamp(accent, 0, hudRainbowAccent);
        style = std::clamp(style, 0, 4);
        x = std::clamp(x,-1,10000);y = std::clamp(y,-1,10000);fontScale = std::clamp(fontScale,80,140);
        uint32_t seen=0;for(int i=0;i<16;++i){const auto item=uint32_t((order>>(4*i))&15);if(item>15||(seen&(1u<<item))){order=defaultOrder;break;}seen|=1u<<item;}
    }
};
inline std::array<uint32_t,16> orderedFields(const HudOptions& options){auto copy=options;copy.validate();std::array<uint32_t,16> fields{};for(int i=0;i<16;++i)fields[i]=1u<<((copy.order>>(4*i))&15);return fields;}
inline constexpr size_t hudLabelBytes = 160;
inline constexpr size_t hudFontFamilyBytes = 256;
struct HudText {
    std::array<std::string,16> labels;
    std::string fontFamily;
    bool operator==(const HudText& other) const {
        return fontFamily == other.fontFamily && labels == other.labels;
    }
};
struct HudProfile {
    HudOptions options;
    int corner = 0, mode = 0;
    HudText text;
}; // auto, desktop, attached in-game
inline float overlayScale(int percent, int renderHeight) {
    return percent ? std::clamp(percent / 100.f, .5f, 3.f)
                   : std::clamp(renderHeight / 1080.f, .85f, 2.5f);
}
inline float desktopScale(int percent, float dpi, int availableWidth, int availableHeight) {
    float fit =
        std::max(.35f, std::min((availableWidth - 48) / 1180.f, (availableHeight - 80) / 820.f));
    float density = std::max(.5f, dpi / 96.f);
    float requested =
        percent ? density * percent / 100.f
                : std::max(density, std::min(availableWidth / 1440.f, availableHeight / 1000.f));
    return std::min(fit, requested);
}
struct DesktopViewport {
    float scale = 1, x = 0, y = 0;
};
inline DesktopViewport desktopViewport(int percent, float dpi, int displayWidth, int displayHeight,
                                       int clientWidth, int clientHeight) {
    float fit = std::max(.01f, std::min(clientWidth / 1180.f, clientHeight / 820.f));
    float scale = std::min(fit, desktopScale(percent, dpi, displayWidth, displayHeight));
    return {scale, std::max(0.f, std::floor((clientWidth - 1180 * scale) / 2)),
            std::max(0.f, std::floor((clientHeight - 820 * scale) / 2))};
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
inline constexpr std::array<uint32_t,hudRainbowAccent + 1> accentColors{
    0x7df2c6,0x59cfff,0xc2a3ff,0xf7be6f,0xff8baf,0x63dfdf,0xf4e478,0xa3e56d,0xff9d72,0xd5e4f5,0x7df2c6};
inline float hudWidth(const HudOptions& options) {
    if(options.style==1||options.style==3)return 230;
    if(options.style==4)return 390;
    if(options.style!=2)return 260;
    float width=32;bool previous=false;
    for(auto field:orderedFields(options))if((options.fields&field)&&field!=HudGraph){if(previous)width+=12;width+=field==HudFps?172:104;previous=true;}
    return std::max(260.f,width);
}
struct HudCell {uint32_t field=0;float x=0,y=0,width=0,height=0;bool column=false;};
struct HudLayout {std::array<HudCell,16> cells{};size_t count=0;float width=0,height=0;};
inline HudLayout hudLayout(HudOptions options){
    options.validate();HudLayout result;result.width=hudWidth(options);
    const bool compact=options.style==1||options.style==3,bar=options.style==2,dashboard=options.style==4;
    const float padding=compact?12.f:16.f;float x=padding,y=compact?10.f:13.f;int gridColumn=0;
    for(auto field:orderedFields(options)){
        if(!(options.fields&field)||(bar&&field==HudGraph))continue;
        const bool metric=field!=HudFps&&field!=HudBrand&&field!=HudGraph;
        if(dashboard&&!metric&&gridColumn){y+=44;gridColumn=0;}
        float width=result.width-2*padding,height=compact?18.f:21.f;
        if(field==HudFps)height=compact?39.f:49.f;
        if(field==HudBrand)height=compact?16.f:21.f;
        if(field==HudGraph)height=compact?36.f:47.f;
        float cellX=padding;
        if(bar){cellX=x;width=field==HudFps?172.f:104.f;height=50;x+=width+12;}
        if(dashboard&&metric){width=(result.width-2*padding-12)/2;cellX=padding+gridColumn*(width+12);height=44;}
        result.cells[result.count++]={field,cellX,y,width,height,bar||(dashboard&&metric)};
        if(bar)continue;
        if(dashboard&&metric){if(gridColumn){y+=height;gridColumn=0;}else gridColumn=1;}
        else y+=height;
    }
    if(gridColumn)y+=44;
    if(bar){if(result.count)y+=50;if(options.fields&HudGraph){result.cells[result.count++]={HudGraph,padding,y,result.width-2*padding,42,false};y+=42;}}
    result.height=y+(compact?10:13);
    return result;
}
inline float hudHeight(const HudOptions& options){return hudLayout(options).height;}
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
