#ifndef MAIN_H
#define MAIN_H
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include "driver/mcpwm.h"

void test_stabilisation_simple(void);
void run_stabilization_tests(void);

#endif /* MAIN_H */
