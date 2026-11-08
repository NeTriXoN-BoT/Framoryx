#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/imgui/imstb_truetype.h"
#include <array>
#include <algorithm>
#include <iostream>

int main(){
    int checks=0;
    const auto reject=[&](int width,int height,int stride,int padding){
        stbtt_pack_context context{};unsigned char sentinel=123;
        ++checks;return !stbtt_PackBegin(&context,&sentinel,width,height,stride,padding,nullptr)&&sentinel==123;
    };
    if(!reject(-1,8,0,0)||!reject(8,-1,0,0)||!reject(0,8,0,0)||!reject(8,0,0,0)||
       !reject(65536,8,0,0)||!reject(8,65536,0,0)||!reject(65535,65535,0,0)||
       !reject(8,8,0,-1)||!reject(8,8,0,8)||!reject(8,8,7,0))return 1;
    std::array<unsigned char,32*16> pixels;pixels.fill(123);stbtt_pack_context context{};
    ++checks;if(!stbtt_PackBegin(&context,pixels.data(),32,16,0,1,nullptr))return 1;
    ++checks;if(!std::all_of(pixels.begin(),pixels.end(),[](auto value){return value==0;}))return 1;
    stbtt_PackEnd(&context);
    unsigned char sentinel=123;
    ++checks;if(stbtt_BakeFontBitmap(nullptr,0,12,&sentinel,65535,65535,0,0,nullptr)!=-1||sentinel!=123)return 1;
    ++checks;if(stbtt_BakeFontBitmap(nullptr,0,12,&sentinel,-1,12,0,0,nullptr)!=-1||sentinel!=123)return 1;
    std::cout<<"PASS: "<<checks<<" font bitmap bounds checks\n";
}
