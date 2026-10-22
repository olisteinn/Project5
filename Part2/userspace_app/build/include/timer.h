#pragma once
#include <cstdint>
#include <chrono>
inline uint32_t time_mus() {
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}