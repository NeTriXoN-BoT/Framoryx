#include "framoryx_hud.hpp"
#include <iostream>
#include <string>
int main(){
    std::string good="1|65535|0|0|9|4|140|9700|300|60|1";
    for(int i=0;i<16;++i)good+='|'+std::to_string(i);
    if(!framoryx_hud::configure(good)||!framoryx_hud::state.ready||!framoryx_hud::state.visible||framoryx_hud::state.options.opacity!=0||fg::hudLayout(framoryx_hud::state.options).count!=16)return 1;
    const auto order=framoryx_hud::state.options.order;
    if(framoryx_hud::configure(good+"|9")||framoryx_hud::configure("1|9999999999999999999999")||framoryx_hud::configure(std::string(513,'1'))||framoryx_hud::configure(good.substr(0,good.rfind('|'))+"|0")||framoryx_hud::state.options.order!=order)return 2;
    if(!framoryx_hud::metrics("14400|12000|7212|3200|8800|5000|7100|6500|12300|210000")||framoryx_hud::state.values[5]!=5000)return 3;
    if(framoryx_hud::metrics("14400|12000|20000|3200|8800|5000|7100|6500|12300|210000")||framoryx_hud::metrics("14400|12000|7212|3200|8800|5000|7100|25000|12300|210000"))return 4;
    if(!framoryx_hud::metrics("-1|-1|-1|-1|-1|-1|-1|-1|-1|-1")||framoryx_hud::state.values[7]!=-1)return 5;
    framoryx_hud::mutex.lock();const bool blocked=framoryx_hud::configure(good);framoryx_hud::mutex.unlock();if(blocked)return 6;
    std::cout<<"PASS: HUD packet validation, all readouts, bounds, unavailable values and nonblocking updates\n";
}
