#include <cstdint>
#include <CLI/CLI.hpp>
#include <gpiod.h>
#include <signal.h>
#include "controller.h"
#include "P_controller.h"
#include "PI_controller.h"
#include "drive.h"
#include "timer.h"

#define pwm_rev 33
#define pwm_fwd 35
#define slp_pin 25

double Kp_init_P = 8.21; // optimal fyrir P controller
double Kp_init_PI = 7.39; // optimal fyrir PI controller
double Ti_init_PI = 0.08517; // optimal fyrir PI controller
P_controller Pctrl(Kp_init_P);
PI_controller PIctrl(Kp_init_PI,Ti_init_PI);
controller* ctrl = &PIctrl;

Drive bridge(pwm_fwd,pwm_rev,slp_pin);

const int16_t rev_res = 1050;
struct History {
    // Þetta er til að reikna hraðann, hægt að averagea yfir seinustu X mörg counter steps
    volatile int32_t counter_mem;
    volatile uint32_t timestamps;
};
static const uint8_t history_length = 40; // Averagear hraðann yfir þetta mörg steps, max 254 eða 255, annaðhvort idk
History enc_memory[history_length];
volatile uint8_t history_head = 0; 
const uint32_t timeout = 100000; // microsek, fyrir .speed()
int16_t rpm; // Reiknuð rpm


volatile sig_atomic_t run = 1;
void sig_handler(int signo) {
    (void)signo;
    run = 0;
}
void track(int32_t count) {
    history_head++;
    if (history_head >= history_length) {
        history_head=0;
    }
    enc_memory[history_head].timestamps = time_mus();
    enc_memory[history_head].counter_mem = count;
}
int16_t speed(void) {
    uint8_t head = history_head;
    uint8_t tail = head + 1;
    if (tail >= history_length) {
        tail = 0;
    }
     //stoppa interrupts til að copya >8 bit vals
    uint32_t new_time = enc_memory[head].timestamps;
    uint32_t old_time = enc_memory[tail].timestamps;
    int8_t delta_pos = enc_memory[head].counter_mem - enc_memory[tail].counter_mem;
    if ((time_mus() - new_time) > timeout) {
        rpm = 0;
        return rpm;
    }
    int32_t delta_time = new_time - old_time;
    if (delta_time == 0) {
        return rpm; 
    }
    rpm = 100 * (60000000L/rev_res) * delta_pos / delta_time;
    return rpm;
}

int main(int argc, char** argv) {
    signal(SIGINT, sig_handler);
    
    CLI::App app{"Motor controller f project 5"};
    float speed_arg;

    app.add_option("speed",speed_arg,"CAN bus name, i.e. ""can0""")->required();
    
    CLI11_PARSE(app, argc, argv);

    int enc_fd = open("/dev/encoder", O_RDONLY);
    int32_t curr_ticks;
    
    int16_t target_speed = speed_arg*100;
    int16_t currspeed;
    int16_t ctrlOP;
    bridge.init();

    while(run) {
        read(enc_fd, &curr_ticks, sizeof(curr_ticks));
        track(curr_ticks);
        currspeed = speed();// kernel encoder call;
        ctrlOP = ctrl->update(target_speed,currspeed); // ctrlOP = controller output, int16_t 
        bridge.run(ctrlOP);
        usleep(5); // Þarf að laga hafa polling loop og control loop
    }

    bridge.sleep();
    return 0;
}
