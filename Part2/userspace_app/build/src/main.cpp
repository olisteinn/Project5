#include <cstdint>
#include <CLI/CLI.hpp>
#include <gpiod.h>
#include <signal.h>
#include "controller.h"
#include "P_controller.h"
#include "PI_controller.h"
#include "drive.h"
#include "timer.h"
#include <fcntl.h>  // f. open() and O_RDONLY
#include <unistd.h> // f. read() and usleep()

#define pwm_rev 13 // pin 33
#define pwm_fwd 12 // pin 32
#define slp_pin 25 // pin 22

double Kp_init_P = 8.21; // optimal fyrir P controller
double Kp_init_PI = 7.39; // optimal fyrir PI controller
double Ti_init_PI = 0.08517; // optimal fyrir PI controller
P_controller Pctrl(Kp_init_P);
PI_controller PIctrl(Kp_init_PI,Ti_init_PI);
controller* ctrl = &PIctrl;

Drive bridge(pwm_fwd,pwm_rev,slp_pin);

struct History {
    // Þetta er til að reikna hraðann, hægt að averagea yfir seinustu X mörg counter steps
    volatile int8_t counter_mem;
    volatile uint32_t timestamps;
};
const uint32_t timeout = 100000; // microsek, fyrir .speed()
int16_t rpm; // Reiknuð rpm
const int16_t rev_res = 1050;

volatile sig_atomic_t run = 1;
void sig_handler(int signo) { // signal handler, til að stoppa applicationið
    (void)signo;
    run = 0;
}

int16_t speed(History read_struct[2]) { // Bara gamla speed(functionið úr encoder.cpp)
    uint32_t new_time = read_struct[0].timestamps;
    uint32_t old_time = read_struct[1].timestamps;
    int8_t delta_pos = read_struct[0].counter_mem - read_struct[1].counter_mem;
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

    app.add_option("speed",speed_arg,"Set target speed (float)")->required();
    
    CLI11_PARSE(app, argc, argv);

    int enc_fd = open("/dev/encoder", O_RDONLY); // opnar counter kernel module
    History kernel_read[2]; // Til að lesa úr kernel module
    
    int16_t target_speed = speed_arg*100;
    int16_t currspeed;
    int16_t ctrlOP;
    bridge.init();

    while(run) {
        read(enc_fd, &kernel_read, sizeof(kernel_read)); // les counter úr kernel module
        currspeed = speed(kernel_read);// reiknar hraða miðað við history struct 
        ctrlOP = ctrl->update(target_speed,currspeed); // ctrlOP = controller output, int16_t 
        bridge.run(ctrlOP); // stillir PWM
        std::cout << "[" << "Project 5" << "] " << "current speed: " << currspeed << "\n"; // UI fyrir terminal, sýnir actual hraða
        usleep(5000);
    }

    bridge.sleep();
    return 0;
}
