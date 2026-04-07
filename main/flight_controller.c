#include "flight_controller.h"

/*
 * Gains PID — réglés pour environnement avec vibrations.
 * Kp modéré = correction progressive
 * Ki faible = évite l'accumulation d'erreurs dues aux vibrations
 * Kd très faible = CRUCIAL - le terme dérivé amplifie les vibrations !
 */
#define ROLL_KP   0.25f
#define ROLL_KI   0.01f
#define ROLL_KD   0.001f   // Très faible pour ignorer les vibrations

#define PITCH_KP  0.25f
#define PITCH_KI  0.01f
#define PITCH_KD  0.001f   // Très faible pour ignorer les vibrations

#define YAW_KP    0.4f
#define YAW_KI    0.005f
#define YAW_KD    0.01f   // Très faible

#define PID_INTEGRAL_LIMIT  8.0f    // anti-windup réduit
#define PID_OUTPUT_LIMIT    8.0f    // correction max douce

static float clampf(float val, float lo, float hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

void flight_ctrl_init(flight_ctrl_t *fc)
{
    pid_init(&fc->pid_roll,  ROLL_KP,  ROLL_KI,  ROLL_KD,
             PID_INTEGRAL_LIMIT, PID_OUTPUT_LIMIT);
    pid_init(&fc->pid_pitch, PITCH_KP, PITCH_KI, PITCH_KD,
             PID_INTEGRAL_LIMIT, PID_OUTPUT_LIMIT);
    pid_init(&fc->pid_yaw,   YAW_KP,   YAW_KI,   YAW_KD,
             PID_INTEGRAL_LIMIT, PID_OUTPUT_LIMIT);
    fc->armed = false;
}

float ibus_to_angle(uint16_t raw, float max_angle)
{
    /* 1000 → -max_angle,  1500 → 0,  2000 → +max_angle */
    return ((float)raw - (float)IBUS_MID) / ((float)IBUS_MAX - (float)IBUS_MID) * max_angle;
}

float ibus_to_percent(uint16_t raw)
{
    /* Limite à THROTTLE_MAX_PCT pour garder marge de stabilisation */
    float pct = ((float)raw - (float)IBUS_MIN) / ((float)IBUS_MAX - (float)IBUS_MIN) * THROTTLE_MAX_PCT;
    return clampf(pct, 0.0f, THROTTLE_MAX_PCT);
}

/*
 * Mixage quadricopter en configuration X (vue de dessus) :
 *
 *   FL (CCW)    FR (CW)
 *       \      /
 *        \    /
 *         \/
 *         /\
 *        /  \
 *       /    \
 *   BL (CW)    BR (CCW)
 *
 * FR = throttle - roll - pitch - yaw
 * FL = throttle + roll - pitch + yaw
 * BR = throttle - roll + pitch + yaw
 * BL = throttle + roll + pitch - yaw
 */
void flight_ctrl_update(flight_ctrl_t *fc,
                        float throttle,
                        float roll_sp, float pitch_sp, float yaw_sp,
                        float roll_m,  float pitch_m,  float yaw_rate,
                        float dt,
                        motor_output_t *out)
{
    /* Si désarmé ou gaz trop bas, tout couper */
    if (!fc->armed || throttle < THROTTLE_MIN_PCT) {
        pid_reset(&fc->pid_roll);
        pid_reset(&fc->pid_pitch);
        pid_reset(&fc->pid_yaw);
        out->front_right = 0.0f;
        out->front_left  = 0.0f;
        out->back_right  = 0.0f;
        out->back_left   = 0.0f;
        return;
    }

    float roll_corr  = pid_compute(&fc->pid_roll,  roll_sp,  roll_m,   dt);
    float pitch_corr = pid_compute(&fc->pid_pitch, pitch_sp, pitch_m,  dt);
    float yaw_corr   = pid_compute(&fc->pid_yaw,   yaw_sp,   yaw_rate, dt);

    out->front_right = clampf(throttle - roll_corr - pitch_corr - yaw_corr, 0.0f, 100.0f);
    out->front_left  = clampf(throttle + roll_corr - pitch_corr + yaw_corr, 0.0f, 100.0f);
    out->back_right  = clampf(throttle - roll_corr + pitch_corr + yaw_corr, 0.0f, 100.0f);
    out->back_left   = clampf(throttle + roll_corr + pitch_corr - yaw_corr, 0.0f, 100.0f);
}
