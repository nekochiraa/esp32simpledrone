#include "motor.h"
static const char* TAG = "MyModule";
#define ESC_FRONT_RIGHT_GPIO 17
#define ESC_FRONT_LEFT_GPIO 19
#define ESC_BACK_RIGHT_GPIO 20
#define ESC_BACK_LEFT_GPIO 21
#define MAXBRIDE 100.0
void pwminit(){
mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, ESC_FRONT_RIGHT_GPIO);
mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, ESC_FRONT_LEFT_GPIO);
mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, ESC_BACK_RIGHT_GPIO);
mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1B, ESC_BACK_LEFT_GPIO);

mcpwm_config_t pwm_config;
pwm_config.frequency = 50;             // 50 Hz pour signaux ESC
pwm_config.cmpr_a = 0;                 // Duty cycle canal A
pwm_config.cmpr_b = 0;                 // Duty cycle canal B
pwm_config.counter_mode = MCPWM_UP_COUNTER;
pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
ESP_LOGI(TAG, "ARMING avant");
vTaskDelay(5000/portTICK_PERIOD_MS);
ESP_LOGI(TAG, "ARMING DES ESC");
mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);
mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 5.0);
vTaskDelay(2000/portTICK_PERIOD_MS);
mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 10.0);
vTaskDelay(2000/portTICK_PERIOD_MS);
ESP_LOGI(TAG, "ARMING DES ESC");
}
void frontright(float percent){
  float duty = percent/20.0 + 5.0;
  ESP_LOGI(TAG, "%f", duty);
  if (percent > MAXBRIDE){duty = MAXBRIDE/20.0 + 5.0;}
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, duty);
}
void frontleft(float percent){
  float duty = percent/20.0 + 5.0;
  if (percent > MAXBRIDE){duty = MAXBRIDE/20.0 + 5.0;}
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, duty);
}
void backright(float percent){
   float duty = percent/20.0 + 5.0;
  if (percent > MAXBRIDE){duty = MAXBRIDE/20.0 + 5.0;}
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, duty);
}
void backleft(float percent){
     float duty = percent/20.0 + 5.0;
  if (percent > MAXBRIDE){duty = MAXBRIDE/20.0 + 5.0;}
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_B, duty);
}
