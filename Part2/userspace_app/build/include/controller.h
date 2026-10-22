#pragma once
#include <stdint.h>

class controller {
    public:
        virtual int16_t update(int16_t ref, int16_t actual) = 0;
};