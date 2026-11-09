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
    const auto labelPacket = [](std::string_view first) {
        return "1|" + std::string(first) + std::string(15, '|');
    };
    const auto custom = labelPacket("wrA7PXw6"); // UTF-8 degree sign and control-channel delimiters.
    if (!framoryx_hud::labels(custom) ||
        std::string_view(framoryx_hud::state.labels[0].data()) != "\xc2\xb0;=|:") return 7;
    if (framoryx_hud::labels(custom + "|") || framoryx_hud::labels("1|QQ") ||
        framoryx_hud::labels(labelPacket("_w")) || framoryx_hud::labels(labelPacket("DQ")) ||
        framoryx_hud::labels(labelPacket("QR")) ||
        framoryx_hud::labels(labelPacket(std::string(215, 'Q'))) ||
        std::string_view(framoryx_hud::state.labels[0].data()) != "\xc2\xb0;=|:") return 8;
    if (!framoryx_hud::labels(labelPacket("")) || framoryx_hud::state.labels[0][0]) return 9;
    if (framoryx_hud::state.options.temperatureColors || !framoryx_hud::temperatureColors("1") ||
        !framoryx_hud::configure(good) || !framoryx_hud::state.options.temperatureColors ||
        framoryx_hud::temperatureColors("2") || !framoryx_hud::state.options.temperatureColors ||
        !framoryx_hud::temperatureColors("0") || framoryx_hud::state.options.temperatureColors) return 10;
    std::string rainbow = good;
    rainbow.replace(rainbow.find("|9|4|"), 5, "|10|4|");
    if (!framoryx_hud::configure(rainbow) || framoryx_hud::state.options.accent != fg::hudRainbowAccent) return 11;
    rainbow.replace(rainbow.find("|10|4|"), 6, "|11|4|");
    if (framoryx_hud::configure(rainbow) || framoryx_hud::state.options.accent != fg::hudRainbowAccent) return 12;
    if (framoryx_hud::state.options.textGlow || !framoryx_hud::textGlow("1") ||
        !framoryx_hud::configure(good) || !framoryx_hud::state.options.textGlow ||
        framoryx_hud::textGlow("2") || !framoryx_hud::state.options.textGlow ||
        !framoryx_hud::textGlow("0") || framoryx_hud::state.options.textGlow) return 13;
    if (!framoryx_hud::textGlow("1")) return 14;
    framoryx_hud::hide();
    if (framoryx_hud::state.options.textGlow) return 15;
    if (framoryx_hud::state.options.valueColors || !framoryx_hud::valueColors("1|3200") ||
        !framoryx_hud::configure(good) || !framoryx_hud::state.options.valueColors ||
        framoryx_hud::state.ramCapacity != 3200 ||
        framoryx_hud::valueColors("2|3200") || framoryx_hud::valueColors("1|-1") ||
        framoryx_hud::valueColors("1|100000001") || framoryx_hud::valueColors("1|0|1") ||
        framoryx_hud::state.ramCapacity != 3200 || !framoryx_hud::state.options.valueColors ||
        framoryx_hud::state.options.temperatureColors) return 16;
    if (!framoryx_hud::valueColors("0|0") || framoryx_hud::state.options.valueColors ||
        !framoryx_hud::valueColors("1|3200")) return 17;
    framoryx_hud::hide();
    if (framoryx_hud::state.options.valueColors || framoryx_hud::state.ramCapacity != 0) return 18;
    std::cout<<"PASS: HUD packet validation, all readouts, bounds, unavailable values and nonblocking updates\n";
}
