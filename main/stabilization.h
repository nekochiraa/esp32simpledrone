#ifndef MAIN_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
int stabinit(float *offset);
float pidcontroll(float mesure, float consigne, float dt, float *pid);
float complementaryFilter(float gyro, float accel);
int printgyro(uint8_t *data, float *xyz, float dt, float *offset);
int printaccel(uint8_t *data, float *xyz);
#endif /* MAIN_H */
