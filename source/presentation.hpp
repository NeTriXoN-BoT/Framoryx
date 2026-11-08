#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>

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
    static constexpr uint64_t defaultOrder = 0x4876532109ull;
    uint32_t fields = HudFps | HudLow | HudTime | HudGraph | HudBrand;
    int scale = 0, opacity = 90, accent = 0, style = 0;
    uint64_t order = defaultOrder;
    int x = -1, y = -1, fontScale = 100;
    void validate() {
        fields &= HudAll;
        scale = scale == 0 ? 0 : std::clamp(scale, 50, 300);
        opacity = std::clamp(opacity, 0, 100);
        accent = std::clamp(accent, 0, 9);
        style = std::clamp(style, 0, 4);
        x = std::clamp(x,-1,10000);y = std::clamp(y,-1,10000);fontScale = std::clamp(fontScale,80,140);
        uint32_t seen=0;for(int i=0;i<10;++i){const auto item=uint32_t((order>>(4*i))&15);if(item>9||(seen&(1u<<item))){order=defaultOrder;break;}seen|=1u<<item;}
    }
};
inline std::array<uint32_t,10> orderedFields(const HudOptions& options){auto copy=options;copy.validate();std::array<uint32_t,10> fields{};for(int i=0;i<10;++i)fields[i]=1u<<((copy.order>>(4*i))&15);return fields;}
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
inline constexpr std::array<uint32_t,10> accentColors{
    0x7df2c6,0x59cfff,0xc2a3ff,0xf7be6f,0xff8baf,0x63dfdf,0xf4e478,0xa3e56d,0xff9d72,0xd5e4f5};
inline float hudWidth(const HudOptions& options) {
    if(options.style==1||options.style==3)return 230;
    if(options.style==4)return 390;
    if(options.style!=2)return 260;
    float width=28;
    for(auto field:orderedFields(options))if((options.fields&field)&&field!=HudGraph)width+=field==HudFps?152:104;
    return std::max(260.f,width);
}
struct HudCell {uint32_t field=0;float x=0,y=0,width=0,height=0;bool column=false;};
struct HudLayout {std::array<HudCell,10> cells{};size_t count=0;float width=0,height=0;};
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
        if(bar){cellX=x;width=field==HudFps?152.f:104.f;height=50;x+=width;}
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
