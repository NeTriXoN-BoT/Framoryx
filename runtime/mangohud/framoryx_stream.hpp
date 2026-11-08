// SPDX-License-Identifier: MIT
// Bounded, opt-in frame telemetry. Recording never waits or allocates.
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
namespace framoryx_stream {
inline std::atomic<bool> enabled{false};
inline std::mutex mutex;
inline std::array<uint32_t,1024> samples{};
inline size_t begin=0,count=0;
inline uint64_t dropped=0;
inline void record(float milliseconds){
    if(!enabled.load(std::memory_order_relaxed)||!std::isfinite(milliseconds)||milliseconds<=0||milliseconds>=60000)return;
    std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;
    if(count==samples.size()){begin=(begin+1)%samples.size();--count;++dropped;}
    samples[(begin+count)%samples.size()]=uint32_t(milliseconds*1000.f+.5f);++count;
}
inline void reset(bool active){
    enabled.store(active,std::memory_order_relaxed);std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return;
    begin=0;count=0;dropped=0;
}
inline std::string take(){
    enabled.store(true,std::memory_order_relaxed);std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);if(!lock.owns_lock())return {};
    std::string result=std::to_string(dropped)+"|";const size_t amount=count<250?count:250;
    for(size_t i=0;i<amount;++i){if(i)result+=',';result+=std::to_string(samples[(begin+i)%samples.size()]);}
    begin=(begin+amount)%samples.size();count-=amount;return result;
}
}
