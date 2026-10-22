#include "drive.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

Drive::Drive(uint8_t pin_fwd_arg, uint8_t pin_rev_arg, uint8_t sleep_pin_arg)
    : pin_fwd(pin_fwd_arg), pin_rev(pin_rev_arg), sleep_pin(sleep_pin_arg) {}

Drive::~Drive() {
    sleep();
    if (fd_duty_fwd >= 0) close(fd_duty_fwd);
    if (fd_duty_rev >= 0) close(fd_duty_rev);
    if (request) gpiod_line_request_release(request);
    if (chip) gpiod_chip_close(chip);
}

void Drive::init() {
    // Stilla upp slp pinna:
    chip = gpiod_chip_open("/dev/gpiochip0");
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    struct gpiod_line_config *line_cfg = gpiod_line_config_new();

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_config_add_line_settings(line_cfg, &sleep_pin, 1, settings);

    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "P5p2_drive");

    request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    // pwm config:
    fd_duty_fwd = open("/sys/class/pwm/pwmchip0/pwm0/duty_cycle", O_WRONLY);
    fd_duty_rev = open("/sys/class/pwm/pwmchip0/pwm1/duty_cycle", O_WRONLY);

    stop(); // pwm = 0
    wake(); // slp pin = hi
}

void Drive::set_pwm_duty(int fd, uint32_t duty_ns) {
    if (fd < 0) return;
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%u", duty_ns);
    write(fd, buf, len);
}

void Drive::fwd(uint8_t speed) {
    uint32_t duty_ns = (uint32_t)speed * pwm_period_ns / 255;
    set_pwm_duty(fd_duty_rev, 0);
    set_pwm_duty(fd_duty_fwd, duty_ns);
}

void Drive::rev(uint8_t speed) {
    uint32_t duty_ns = (uint32_t)speed * pwm_period_ns / 255;
    set_pwm_duty(fd_duty_fwd, 0);
    set_pwm_duty(fd_duty_rev, duty_ns);
}

void Drive::run(int16_t speed) {
    int16_t pwm_val = speed >> 7;
    if (pwm_val > 255) pwm_val = 255;
    if (pwm_val < -255) pwm_val = -255;

    if (pwm_val > 0) {
        fwd((uint8_t)pwm_val);
    } else if (pwm_val < 0) {
        rev((uint8_t)(-pwm_val));
    } else {
        stop();
    }
}

void Drive::stop(void) {
    set_pwm_duty(fd_duty_fwd, 0);
    set_pwm_duty(fd_duty_rev, 0);
}

void Drive::sleep() {
    stop();
    if (request) {
        gpiod_line_request_set_value(request, sleep_pin, GPIOD_LINE_VALUE_INACTIVE);
    }
}

void Drive::wake() {
    if (request) {
        gpiod_line_request_set_value(request, sleep_pin, GPIOD_LINE_VALUE_ACTIVE);
    }
}