#include "motor.h"

#define TAG                  "motor"
#define ESC_FRONT_RIGHT_GPIO 17
#define ESC_FRONT_LEFT_GPIO  19
#define ESC_BACK_RIGHT_GPIO  20
#define ESC_BACK_LEFT_GPIO   21
#define MAX_THROTTLE         100.0f

static float percent_to_duty(float percent)
{
    if (percent > MAX_THROTTLE) percent = MAX_THROTTLE;
    if (percent < 0.0f)         percent = 0.0f;
    return percent / 20.0f + 5.0f;
}

void pwminit(void)
{
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, ESC_FRONT_RIGHT_GPIO);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, ESC_FRONT_LEFT_GPIO);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, ESC_BACK_RIGHT_GPIO);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1B, ESC_BACK_LEFT_GPIO);

    mcpwm_config_t pwm_config = {
        .frequency    = 50,
        .cmpr_a       = 0,
        .cmpr_b       = 0,
        .counter_mode = MCPWM_UP_COUNTER,
        .duty_mode    = MCPWM_DUTY_MODE_0,
    };

    ESP_LOGI(TAG, "Armement des ESC...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);

    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 5.0f);
    vTaskDelay(pdMS_TO_TICKS(2000));
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 10.0f);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "ESC armés");
}

void frontright(float percent)
{
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, percent_to_duty(percent));
}

void frontleft(float percent)
{
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, percent_to_duty(percent));
}

void backright(float percent)
{
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, percent_to_duty(percent));
}

void backleft(float percent)
{
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_B, percent_to_duty(percent));
}
