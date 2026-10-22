#include <gpiod.h>
#include <signal.h>
volatile sig_atomic_t run = 1;
void sig_handler(int signo) {
    (void)signo;
    run = 0;
}
const unsigned int inp_pin = 15;
const unsigned int outp_pin = 16;
const char* gpiod_label = "P5p1_timerRead";
int main() {
    signal(SIGINT, sig_handler);
    struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip0");
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    struct gpiod_line_config *line_cfg = gpiod_line_config_new();

    // Add input pin (15)
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_config_add_line_settings(line_cfg, &inp_pin, 1, settings);
    // Add output pin (16), initially LOW (INACTIVE)
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_config_add_line_settings(line_cfg, &outp_pin, 1, settings);

    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, gpiod_label);
    struct gpiod_line_request *request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    while (run) {
        enum gpiod_line_value read = gpiod_line_request_get_value(request, inp_pin);
        gpiod_line_request_set_value(request, outp_pin, read);
    }
    // 6. Cleanup on Ctrl+C
    gpiod_line_request_set_value(request, outp_pin, GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_request_release(request);
    gpiod_chip_close(chip);
    return 0;
}