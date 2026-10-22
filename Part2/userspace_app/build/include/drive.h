#pragma once
#include <cstdint>


class Drive {
    private:

    public:
        Drive(uint8_t pin_fwd, uint8_t pin_rev, uint8_t Pslp_arg);
        void init(void);
        void run(int16_t speed);
        void stop(void); // .stop() stoppar snúning
        void sleep(void); // .sleep() stoppar snúning og slekkur á driver/brú
        void wake(void); // .wake() kveikir á driver/brú
};