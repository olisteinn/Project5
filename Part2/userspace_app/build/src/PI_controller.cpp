#include "PI_controller.h"
#include <stdint.h>
#include "timer.h"
// þetta er sleggjan

PI_controller::PI_controller(double Kp_arg, double Ti_arg) {
    Kp = 256*Kp_arg; // int16_t*double -> double*double = double -> int16_t (Kp max 128)
    Ki = 256*Kp_arg/Ti_arg;// int16_t*double/double -> double*double/double = double -> int16_t
    KiE = 0;
    last_time = time_mus();
}

int16_t PI_controller::update(int16_t ref, int16_t actual) {
    int16_t e = ref-actual; // speed error. Max ~20,000 ef hann er í max reverse og target er max fwd (eða öfugt)
    int32_t Kpe = ((int32_t)Kp*e)>>8; // 16 bit margföldun í 32 bit trikk, bit shiftar svo niður um 1 byte og SVO breytt í 16 bit
    uint32_t curr_time = time_mus();
    uint16_t dt = curr_time-last_time;// max 65,535 mícrósek á milli, overflowar annas, samt ekkert hættulegt
    int32_t edt = (int32_t)e*dt; // (int32_t)int16_t*uint16_t sama trikk nema notar mulsu instruction
    last_time = curr_time;
    int16_t KiE_i = (int16_t)(((Ki*(edt>>12))>>16)); // staggeruð bit shift passa að ekkert overflowi en missi samt ekki nákvæmni 
    // (getur misst 4.8% utaf hann "deilir" með 1024*1024*256 ekki 1000*1000*256 eins og míkrósek myndu þurfa)
    if ((int32_t)KiE_i+KiE > 32767) { // passar overflow, allt í 16 bit margföldun ennþá
        KiE = 32767;
    } else if ((int32_t)KiE_i+KiE < -32768) {
        KiE = -32768;
    } else {
        KiE += KiE_i; // ef ekkert overflow
    }
    int32_t total = (int32_t)Kpe+KiE;
    if (total > (255*128)) { // Kemur þessu í rétt snið fyrir 8 bit pwm
        return (255*128);
    } else if (total < -(255*128)) {
        return -(255*128); // compiler reiknar öll "(255*128)" og setur bara töluna inn
    } else {
        return total;
    }
}