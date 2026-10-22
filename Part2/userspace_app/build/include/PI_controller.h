#pragma once
#include "controller.h"
#include <stdint.h>

class PI_controller : public controller {
    private:
        int16_t Kp;
        int16_t Ki;
        int16_t KiE;
        uint32_t last_time;
    public:
        PI_controller(double Kp_arg, double Ti);
        int16_t update(int16_t ref, int16_t actual);
};