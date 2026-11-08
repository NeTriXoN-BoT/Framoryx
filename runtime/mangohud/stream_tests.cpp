#include "framoryx_stream.hpp"
#include <iostream>
#include <limits>
int main(){
    using namespace framoryx_stream;
    reset(false);record(16);if(count)return 1;
    reset(true);record(-1);record(0);record(std::numeric_limits<float>::infinity());record(std::numeric_limits<float>::quiet_NaN());if(count)return 2;
    for(int i=0;i<10000;++i)record(10+i/1000.f);
    if(count!=1024||dropped!=8976)return 3;
    const auto first=take();if(first.rfind("8976|18976,18977",0)!=0||first.size()>4090||count!=774)return 4;
    reset(false);if(count||dropped||enabled)return 5;
    mutex.lock();record(20);mutex.unlock();if(count)return 6;
    std::cout<<"PASS: disabled, malformed, overflow, FIFO, packet and nonblocking telemetry checks\n";
}
