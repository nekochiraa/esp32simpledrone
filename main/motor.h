#ifndef MOTOR_H
#include "driver/mcpwm.h"    // Contrôleur MCPWM
#include "soc/mcpwm_periph.h"// Infos sur les broches compatibles (optionnel, utile pour tests)
#include "driver/gpio.h"     // Initialisation des GPIO
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
void pwminit(void);
void frontright(float percent);
void frontleft(float percent);
void backright(float percent);
void backleft(float percent);
#endif // !MOTOR_H
