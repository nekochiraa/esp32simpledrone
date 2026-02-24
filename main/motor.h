#ifndef MOTOR_H
#define MOTOR_H

#include "driver/mcpwm.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

void pwminit(void);
void frontright(float percent);
void frontleft(float percent);
void backright(float percent);
void backleft(float percent);

#endif // MOTOR_H
