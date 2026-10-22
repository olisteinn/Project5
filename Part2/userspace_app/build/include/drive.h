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
};#pragma once
#include <cstdint>
#include <gpiod.h>

class Drive {
    private:
        unsigned int pin_fwd;
        unsigned int pin_rev;
        unsigned int sleep_pin;

        struct gpiod_chip *chip = nullptr;
        struct gpiod_line_request *request = nullptr;

        int fd_duty_fwd = -1;
        int fd_duty_rev = -1;
        uint32_t pwm_period_ns = 50000; // 20 kHz default (50,000 ns)

        void set_pwm_duty(int fd, uint32_t duty_ns);

    public:
        Drive(uint8_t pin_fwd, uint8_t pin_rev, uint8_t Pslp_arg);
        ~Drive();
        void init(void);
        void fwd(uint8_t speed);
        void rev(uint8_t speed);
        void run(int16_t speed);
        void stop(void);  // .stop() stoppar snúning
        void sleep(void); // .sleep() stoppar snúning og slekkur á driver/brú
        void wake(void);  // .wake() kveikir á driver/brú
};