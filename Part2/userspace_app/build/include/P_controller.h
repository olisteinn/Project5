#pragma once
#include "controller.h"
#include <stdint.h>

class P_controller : public controller {
    private:
        int16_t Kp;
    public:
        P_controller(double Kp_arg);
        int16_t update(int16_t ref,int16_t actual);
};