// SPDX-License-Identifier: MIT
#pragma once
#include "presentation.hpp"
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string_view>
#ifdef FRAMORYX_HUD_RENDERER
#include <imgui.h>
#include <cstdio>
#endif
namespace framoryx_hud {
using Label = std::array<char, fg::hudLabelBytes + 1>;
struct State {
    fg::HudOptions options;
    std::array<Label, 16> labels{};
    int cap=60;
    bool ready=false,visible=false;
    int surfaceWidth=0,surfaceHeight=0;
    std::array<int,10> values{{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}};
    std::chrono::steady_clock::time_point updated;
    std::array<float,180> graph{};
    size_t count=0,next=0;
};
inline State state;
inline std::atomic<uint64_t> drawnFrames{0};
inline std::mutex mutex;
inline void hide(){std::lock_guard<std::mutex> lock(mutex);state.visible=false;state.labels={};state.options.temperatureColors=false;state.options.textGlow=false;}
inline std::array<int,2> surface(){std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);return lock.owns_lock()?std::array<int,2>{state.surfaceWidth,state.surfaceHeight}:std::array<int,2>{};}
inline bool requested(){static const bool value=[] {const auto text=std::getenv("FRAMORYX_HUD");return text&&std::strcmp(text,"1")==0;}();return value;}
template<size_t N> bool numbers(std::string_view text,std::array<int,N>& output){
    if(text.size()>512)return false;
    for(size_t i=0;i<N;++i){const auto end=text.find('|');const auto part=text.substr(0,end);if(part.empty())return false;const auto parsed=std::from_chars(part.data(),part.data()+part.size(),output[i]);if(parsed.ec!=std::errc()||parsed.ptr!=part.data()+part.size())return false;if(i+1==N)return end==std::string_view::npos;if(end==std::string_view::npos)return false;text.remove_prefix(end+1);}return false;
}
inline bool configure(std::string_view text){
    std::array<int,27> value{};if(!numbers(text,value)||value[0]!=1||value[1]<0||value[1]>65535||value[2]<0||value[2]>300||value[3]<0||value[3]>100||value[4]<0||value[4]>fg::hudRainbowAccent||value[5]<0||value[5]>4||value[6]<80||value[6]>140||value[7]<0||value[7]>10000||value[8]<0||value[8]>10000||value[9]<0||value[9]>1000||value[10]<0||value[10]>1)return false;
    fg::HudOptions options;options.fields=uint32_t(value[1]);options.scale=value[2];options.opacity=value[3];options.accent=value[4];options.style=value[5];options.fontScale=value[6];options.x=value[7];options.y=value[8];options.order=0;
    uint32_t seen=0;for(size_t i=11;i<value.size();++i){if(value[i]<0||value[i]>15||(seen&(1u<<value[i])))return false;seen|=1u<<value[i];options.order|=uint64_t(value[i])<<(4*(i-11));}options.validate();
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return false;options.temperatureColors=state.options.temperatureColors;options.textGlow=state.options.textGlow;state.options=options;state.cap=value[9];state.visible=value[10]!=0;state.ready=true;return true;
}
inline bool temperatureColors(std::string_view value) {
    if (value != "0" && value != "1") return false;
    std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    state.options.temperatureColors = value == "1";
    return true;
}
inline bool textGlow(std::string_view value) {
    if (value != "0" && value != "1") return false;
    std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    state.options.textGlow = value == "1";
    return true;
}
inline bool labelUtf8(std::string_view text) {
    while (!text.empty()) {
        const auto lead = static_cast<unsigned char>(text.front());
        if (lead >= 0x20 && lead <= 0x7e) {
            text.remove_prefix(1);
            continue;
        }
        if (lead < 0xc2 || lead > 0xf4) return false;
        const size_t width = lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        if (text.size() < width) return false;
        unsigned codepoint = lead & (0x7f >> width);
        for (size_t i = 1; i < width; ++i) {
            const auto next = static_cast<unsigned char>(text[i]);
            if ((next & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        const unsigned minimum = width == 2 ? 0xa0 : width == 3 ? 0x800 : 0x10000;
        if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
        text.remove_prefix(width);
    }
    return true;
}
inline bool decodeLabel(std::string_view encoded, Label& label) {
    static constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    if (encoded.size() > (fg::hudLabelBytes * 4 + 2) / 3) return false;
    unsigned buffer = 0, bits = 0;
    size_t bytes = 0;
    for (const char character : encoded) {
        const auto digit = alphabet.find(character);
        if (digit == std::string_view::npos) return false;
        buffer = (buffer << 6) | unsigned(digit);
        bits += 6;
        if (bits >= 8) {
            if (bytes >= fg::hudLabelBytes) return false;
            bits -= 8;
            label[bytes++] = char((buffer >> bits) & 255);
        }
    }
    return bits < 6 && !(buffer & ((1u << bits) - 1)) && labelUtf8({label.data(), bytes});
}
inline bool labels(std::string_view text) {
    std::array<Label, 16> decoded{};
    if (text.size() > 2 + decoded.size() * ((fg::hudLabelBytes * 4 + 2) / 3 + 1) || text.substr(0, 2) != "1|") return false;
    text.remove_prefix(2);
    for (size_t index = 0; index < decoded.size(); ++index) {
        const auto separator = text.find('|');
        if ((separator == std::string_view::npos) != (index + 1 == decoded.size()) ||
            !decodeLabel(text.substr(0, separator), decoded[index])) return false;
        if (separator != std::string_view::npos) text.remove_prefix(separator + 1);
    }
    std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    state.labels = decoded;
    return true;
}
inline bool metrics(std::string_view text){
    std::array<int,10> values{};if(!numbers(text,values))return false;for(const auto value:values)if(value < -1||value>100000000)return false;
    for(const auto i:{2,4,5})if(values[i]>10000)return false;
    for(const auto i:{6,7})if(values[i]>15000)return false;
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return false;state.values=values;state.updated=std::chrono::steady_clock::now();return true;
}
inline void frame(float milliseconds){
    if(!requested()||!std::isfinite(milliseconds)||milliseconds<=0||milliseconds>=60000)return;
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;state.graph[state.next]=milliseconds;state.next=(state.next+1)%state.graph.size();state.count=std::min(state.count+1,state.graph.size());
}
#ifdef FRAMORYX_HUD_RENDERER
inline void draw(float fps,float milliseconds,bool vulkan){
    const auto display=ImGui::GetIO().DisplaySize;
    thread_local State snapshot;
    {std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(lock.owns_lock()){if(std::isfinite(display.x)&&std::isfinite(display.y)&&display.x>0&&display.y>0&&display.x<=32768&&display.y<=32768){state.surfaceWidth=int(std::round(display.x));state.surfaceHeight=int(std::round(display.y));}snapshot=state;}}
    if(!snapshot.ready||!snapshot.visible||!snapshot.options.fields)return;
    const auto& options=snapshot.options;const auto layout=fg::hudLayout(options);
    const float requestedScale=fg::overlayScale(options.scale,int(display.y))*options.fontScale/100.f;
    const float scale=std::max(.1f,std::min({requestedScale,(display.x-30)/layout.width,(display.y-30)/layout.height}));
    const float width=layout.width*scale,height=layout.height*scale;
    const ImVec2 origin(std::round(options.x/10000.f*std::max(0.f,display.x-width)),std::round(options.y/10000.f*std::max(0.f,display.y-height)));
    auto list=ImGui::GetForegroundDrawList();const int initialVertices=list->VtxBuffer.Size;auto font=ImGui::GetFont();const bool compact=options.style==1||options.style==3;
    const bool rainbow = options.accent == fg::hudRainbowAccent;
    const auto rgb=fg::accentColors[size_t(options.accent)];const ImU32 accent=IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255),textColor=IM_COL32(237,245,250,255),muted=IM_COL32(150,170,187,255);
    const ImVec2 end(origin.x+width,origin.y+height);
    if(options.opacity){list->AddRectFilled(origin,end,IM_COL32(12,19,26,options.opacity*255/100),(compact?8:13)*scale);if(options.style!=3){list->AddRect(origin,end,IM_COL32(40,74,77,255),13*scale);list->AddLine(ImVec2(origin.x+10*scale,origin.y+scale),ImVec2(end.x-10*scale,origin.y+scale),accent,2*scale);}}
    const auto now = std::chrono::steady_clock::now();
    const bool fresh=now-snapshot.updated<std::chrono::seconds(3);
    const float phase = rainbow ? fg::hudRainbowPhase(uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count())) : 0.f;
    const float glowOffset = options.textGlow ? std::clamp(std::round(scale), 1.f, 2.f) : 0.f;
    const auto metric=[&](int index){return fresh&&snapshot.values[size_t(index)]>=0?snapshot.values[size_t(index)]/100.: -1.;};
    const auto gradient = [&](int first) {
        if (first == list->VtxBuffer.Size) return;
        float left = list->VtxBuffer[first].pos.x;
        float right = left;
        for (int index = first + 1; index < list->VtxBuffer.Size; ++index) {
            const float vertexX = list->VtxBuffer[index].pos.x;
            left = std::min(left, vertexX);
            right = std::max(right, vertexX);
        }
        const float inverseWidth = right > left ? 1.f / (right - left) : 0.f;
        for (int index = first; index < list->VtxBuffer.Size; ++index) {
            auto& vertex = list->VtxBuffer[index];
            const auto hue = fg::hudRainbowColor(phase + (vertex.pos.x - left) * inverseWidth);
            vertex.col = (vertex.col & IM_COL32_A_MASK) | IM_COL32((hue >> 16) & 255, (hue >> 8) & 255, hue & 255, 0);
        }
    };
    const auto label = [&](float x, float y, float size, ImU32 color, const char* value, bool keepColor = false) {
        const ImVec2 position(std::round(origin.x + x * scale), std::round(origin.y + y * scale));
        const bool useGradient = rainbow && !keepColor;
        if (glowOffset > 0.f) {
            const int first = list->VtxBuffer.Size;
            const ImU32 glow = (color & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 64);
            list->AddText(font, size * scale, ImVec2(position.x + glowOffset, position.y + glowOffset), glow, value);
            if (useGradient) gradient(first);
        }
        const int first = list->VtxBuffer.Size;
        list->AddText(font, size * scale, position, color, value);
        if (useGradient) gradient(first);
    };
    const auto clipCell = [&](const fg::HudCell& cell, float right) {
        list->PushClipRect(ImVec2(origin.x + cell.x * scale, origin.y + cell.y * scale),
                          ImVec2(origin.x + right * scale, origin.y + (cell.y + cell.height) * scale), true);
    };
    static constexpr const char* labels[]{"Live FPS","Average FPS","1% low","Frame time","Frame graph","System CPU","System RAM","Limit target","Renderer","Framoryx label","GPU usage","VRAM usage","GPU temperature","CPU temperature","GPU power","GPU clock"};
    for(size_t i=0;i<layout.count;++i){const auto& cell=layout.cells[i];unsigned index=0;for(auto bits=cell.field;bits>1;bits>>=1)++index;
        const auto custom = snapshot.labels[index].data();
        if(index==9){clipCell(cell, cell.x + cell.width);label(cell.x,cell.y,10,muted,*custom?custom:"FRAMORYX");list->PopClipRect();continue;}
        if(index==4){
            const float header = *custom ? 13.f : 0.f;
            clipCell(cell, cell.x + cell.width);
            if (*custom) label(cell.x, cell.y, 10, muted, custom);
            if (snapshot.count >= 2) {
                float maximum=20;for(size_t n=0;n<snapshot.count;++n)maximum=std::max(maximum,snapshot.graph[n]);
                ImVec2 previous;for(size_t n=0;n<snapshot.count;++n){const auto sample=snapshot.graph[(snapshot.next+snapshot.graph.size()-snapshot.count+n)%snapshot.graph.size()];const ImVec2 point(origin.x+(cell.x+cell.width*n/(snapshot.count-1))*scale,origin.y+(cell.y+header+(cell.height-header-10)*(1-sample/maximum))*scale);if(n)list->AddLine(previous,point,accent,1.5f*scale);previous=point;}
            }
            list->PopClipRect();
            continue;
        }
        char value[fg::hudLabelBytes + 48];std::strcpy(value,"--");double number=-1;
        switch(index){
        case 0:if(std::isfinite(fps)&&fps>0)std::snprintf(value,sizeof(value),"%.0f %s",double(fps),*custom?custom:"FPS");break;
        case 1:number=metric(0);if(number>=0)std::snprintf(value,sizeof(value),"%.0f FPS",number);break;
        case 2:number=metric(1);if(number>=0)std::snprintf(value,sizeof(value),"%.0f FPS",number);break;
        case 3:if(std::isfinite(milliseconds)&&milliseconds>0)std::snprintf(value,sizeof(value),"%.2f ms",double(milliseconds));break;
        case 5:number=metric(2);if(number>=0)std::snprintf(value,sizeof(value),"%.0f%%",number);break;
        case 6:number=metric(3);if(number>=0)std::snprintf(value,sizeof(value),"%.1f GB",number);break;
        case 7:if(snapshot.cap)std::snprintf(value,sizeof(value),"%d",snapshot.cap);else std::strcpy(value,"Uncapped");break;
        case 8:std::strcpy(value,vulkan?"Vulkan":"OpenGL");break;
        case 10:number=metric(4);if(number>=0)std::snprintf(value,sizeof(value),"%.0f%%",number);break;
        case 11:number=metric(5);if(number>=0)std::snprintf(value,sizeof(value),"%.0f%%",number);break;
        case 12:number=metric(6);if(number>=0)std::snprintf(value,sizeof(value),"%.0f °C",number);break;
        case 13:number=metric(7);if(number>=0)std::snprintf(value,sizeof(value),"%.0f °C",number);break;
        case 14:number=metric(8);if(number>=0)std::snprintf(value,sizeof(value),"%.1f W",number);break;
        case 15:number=metric(9);if(number>=0)std::snprintf(value,sizeof(value),"%.0f MHz",number);break;
        }
        clipCell(cell, cell.x + cell.width);
        if(index==0)label(cell.x,cell.y,compact?28:34,accent,value);
        else {
            const float size = cell.column ? 17 : compact ? 12 : 13;
            const float valueWidth = font->CalcTextSizeA(size * scale, 100000, 0, value).x / scale;
            const float labelRight = cell.x + cell.width - (cell.column ? 0.f : valueWidth + 8.f);
            if (labelRight > cell.x) {
                clipCell(cell, labelRight);
                label(cell.x, cell.y, cell.column ? 10 : compact ? 11 : 12, muted, *custom ? custom : labels[index]);
                list->PopClipRect();
            }
            const auto temperature = options.temperatureColors && (index == 12 || index == 13) ? fg::hudTemperatureColor(number) : 0;
            const ImU32 valueColor = temperature ? IM_COL32((temperature >> 16) & 255, (temperature >> 8) & 255, temperature & 255, 255) : textColor;
            label(cell.column ? cell.x : cell.x + cell.width - valueWidth, cell.y + (cell.column ? 19 : 0), size, valueColor, value, temperature != 0);
        }
        list->PopClipRect();
    }
    if(list->VtxBuffer.Size>initialVertices)drawnFrames.fetch_add(1,std::memory_order_relaxed);
}
#endif
}
