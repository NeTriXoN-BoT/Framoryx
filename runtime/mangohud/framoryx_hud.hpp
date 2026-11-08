// SPDX-License-Identifier: MIT
#pragma once
#include "presentation.hpp"
#include <array>
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
struct State {
    fg::HudOptions options;
    int cap=60;
    bool ready=false,visible=false;
    int surfaceWidth=0,surfaceHeight=0;
    std::array<int,10> values{{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}};
    std::chrono::steady_clock::time_point updated;
    std::array<float,180> graph{};
    size_t count=0,next=0;
};
inline State state;
inline std::mutex mutex;
inline void hide(){std::lock_guard<std::mutex> lock(mutex);state.visible=false;}
inline std::array<int,2> surface(){std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);return lock.owns_lock()?std::array<int,2>{state.surfaceWidth,state.surfaceHeight}:std::array<int,2>{};}
inline bool requested(){static const bool value=[] {const auto text=std::getenv("FRAMORYX_HUD");return text&&std::strcmp(text,"1")==0;}();return value;}
template<size_t N> bool numbers(std::string_view text,std::array<int,N>& output){
    if(text.size()>512)return false;
    for(size_t i=0;i<N;++i){const auto end=text.find('|');const auto part=text.substr(0,end);if(part.empty())return false;const auto parsed=std::from_chars(part.data(),part.data()+part.size(),output[i]);if(parsed.ec!=std::errc()||parsed.ptr!=part.data()+part.size())return false;if(i+1==N)return end==std::string_view::npos;if(end==std::string_view::npos)return false;text.remove_prefix(end+1);}return false;
}
inline bool configure(std::string_view text){
    std::array<int,27> value{};if(!numbers(text,value)||value[0]!=1||value[1]<0||value[1]>65535||value[2]<0||value[2]>300||value[3]<0||value[3]>100||value[4]<0||value[4]>9||value[5]<0||value[5]>4||value[6]<80||value[6]>140||value[7]<0||value[7]>10000||value[8]<0||value[8]>10000||value[9]<0||value[9]>1000||value[10]<0||value[10]>1)return false;
    fg::HudOptions options;options.fields=uint32_t(value[1]);options.scale=value[2];options.opacity=value[3];options.accent=value[4];options.style=value[5];options.fontScale=value[6];options.x=value[7];options.y=value[8];options.order=0;
    uint32_t seen=0;for(size_t i=11;i<value.size();++i){if(value[i]<0||value[i]>15||(seen&(1u<<value[i])))return false;seen|=1u<<value[i];options.order|=uint64_t(value[i])<<(4*(i-11));}options.validate();
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return false;state.options=options;state.cap=value[9];state.visible=value[10]!=0;state.ready=true;return true;
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
    auto list=ImGui::GetForegroundDrawList();auto font=ImGui::GetFont();const bool compact=options.style==1||options.style==3;
    const auto rgb=fg::accentColors[size_t(options.accent)];const ImU32 accent=IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255),textColor=IM_COL32(237,245,250,255),muted=IM_COL32(150,170,187,255);
    const ImVec2 end(origin.x+width,origin.y+height);
    if(options.opacity){list->AddRectFilled(origin,end,IM_COL32(12,19,26,options.opacity*255/100),(compact?8:13)*scale);if(options.style!=3){list->AddRect(origin,end,IM_COL32(40,74,77,255),13*scale);list->AddLine(ImVec2(origin.x+10*scale,origin.y+scale),ImVec2(end.x-10*scale,origin.y+scale),accent,2*scale);}}
    const bool fresh=std::chrono::steady_clock::now()-snapshot.updated<std::chrono::seconds(3);
    const auto metric=[&](int index){return fresh&&snapshot.values[size_t(index)]>=0?snapshot.values[size_t(index)]/100.: -1.;};
    const auto label=[&](float x,float y,float size,ImU32 color,const char* value){list->AddText(font,size*scale,ImVec2(std::round(origin.x+x*scale),std::round(origin.y+y*scale)),color,value);};
    static constexpr const char* labels[]{"Live FPS","Average FPS","1% low","Frame time","Frame graph","System CPU","System RAM","Limit target","Renderer","Framoryx label","GPU usage","VRAM usage","GPU temperature","CPU temperature","GPU power","GPU clock"};
    for(size_t i=0;i<layout.count;++i){const auto& cell=layout.cells[i];unsigned index=0;for(auto bits=cell.field;bits>1;bits>>=1)++index;
        if(index==9){label(cell.x,cell.y,10,muted,"FRAMORYX");continue;}
        if(index==4){
            if(snapshot.count<2)continue;
            float maximum=20;for(size_t n=0;n<snapshot.count;++n)maximum=std::max(maximum,snapshot.graph[n]);
            ImVec2 previous;for(size_t n=0;n<snapshot.count;++n){const auto sample=snapshot.graph[(snapshot.next+snapshot.graph.size()-snapshot.count+n)%snapshot.graph.size()];const ImVec2 point(origin.x+(cell.x+cell.width*n/(snapshot.count-1))*scale,origin.y+(cell.y+(cell.height-10)*(1-sample/maximum))*scale);if(n)list->AddLine(previous,point,accent,1.5f*scale);previous=point;}continue;
        }
        char value[96];std::strcpy(value,"--");double number=-1;
        switch(index){
        case 0:if(std::isfinite(fps)&&fps>0)std::snprintf(value,sizeof(value),"%.0f FPS",double(fps));break;
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
        list->PushClipRect(ImVec2(origin.x+cell.x*scale,origin.y+cell.y*scale),ImVec2(origin.x+(cell.x+cell.width)*scale,origin.y+(cell.y+cell.height)*scale),true);
        if(index==0)label(cell.x,cell.y,compact?28:34,accent,value);
        else {label(cell.x,cell.y,cell.column?10:compact?11:12,muted,labels[index]);const float size=cell.column?17:compact?12:13;const float valueWidth=font->CalcTextSizeA(size*scale,100000,0,value).x/scale;label(cell.column?cell.x:cell.x+cell.width-valueWidth,cell.y+(cell.column?19:0),size,textColor,value);}
        list->PopClipRect();
    }
}
#endif
}
