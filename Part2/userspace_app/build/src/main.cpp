#include <cstdint>
#include <CLI/CLI.hpp>
#include <gpiod.h>
#include <signal.h>
#include "controller.h"
#include "P_controller.h"
#include "PI_controller.h"
#include "drive.h"

#define pwm_rev 33
#define pwm_fwd 35

double Kp_init_P = 8.21; // optimal fyrir P controller
double Kp_init_PI = 7.39; // optimal fyrir PI controller
double Ti_init_PI = 0.08517; // optimal fyrir PI controller
P_controller Pctrl(Kp_init_P);
PI_controller PIctrl(Kp_init_PI,Ti_init_PI);
controller* ctrl = &Pctrl;

Drive bridge(0,D8);


volatile sig_atomic_t run = 1;
void sig_handler(int signo) {
    (void)signo;
    run = 0;
}

int main(int argc, char** argv) {
    signal(SIGINT, sig_handler);
    
    CLI::App app{"Motor controller f project 5"};
    float speed_arg;

    app.add_option("speed",speed_arg,"CAN bus name, i.e. ""can0""")->required();
    
    CLI11_PARSE(app, argc, argv);

    int16_t target_speed = speed_arg*100;
    int16_t currspeed;
    int16_t ctrlOP;

    while(run) {
        currspeed = kernel encoder call;
        ctrlOP = ctrl->update(target_speed,currspeed); // ctrlOP = controller output, int16_t 
        bridge.run(ctrlOP);
    }

    
    return 0;
}
