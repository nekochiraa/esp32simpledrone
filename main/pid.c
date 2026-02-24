#include "pid.h"

void pid_init(pid_controller_t *pid, float kp, float ki, float kd,
              float integral_limit, float output_limit)
{
    pid->kp             = kp;
    pid->ki             = ki;
    pid->kd             = kd;
    pid->integral       = 0.0f;
    pid->prev_error     = 0.0f;
    pid->integral_limit = integral_limit;
    pid->output_limit   = output_limit;
}

void pid_reset(pid_controller_t *pid)
{
    pid->integral   = 0.0f;
    pid->prev_error = 0.0f;
}

static float clamp(float val, float limit)
{
    if (val >  limit) return  limit;
    if (val < -limit) return -limit;
    return val;
}

float pid_compute(pid_controller_t *pid, float setpoint, float measure, float dt)
{
    if (dt <= 0.0f) return 0.0f;

    float error = setpoint - measure;

    // Terme proportionnel
    float p_term = pid->kp * error;

    // Terme intégral avec anti-windup
    pid->integral += error * dt;
    pid->integral  = clamp(pid->integral, pid->integral_limit);
    float i_term   = pid->ki * pid->integral;

    // Terme dérivé
    float derivative = (error - pid->prev_error) / dt;
    pid->prev_error  = error;
    float d_term     = pid->kd * derivative;

    float output = p_term + i_term + d_term;
    return clamp(output, pid->output_limit);
}
