#ifndef STABILIZATION_H
#define STABILIZATION_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

int   stabinit(float *gyro_offset, float *accel_offset);
float pidcontroll(float mesure, float consigne, float dt, float *pid);
float complementaryFilter(float gyro, float accel);
void  kalmanfilter(float gyro, float accel, float *P, float *X);
int   printgyro(uint8_t *data, float *xyz, float dt, float *offset);
int   printaccel(uint8_t *data, float *xyz, float *offset);

#endif /* STABILIZATION_H */
