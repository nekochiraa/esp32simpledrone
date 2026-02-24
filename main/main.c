#include <freertos/FreeRTOS.h>
#include <driver/uart.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ibus/ibus.h"
#include "motor.h"
#include "stabilization.h"
#include "flight_controller.h"

#define IBUS_UART   UART_NUM_1
#define IBUS_TX_PIN 4
#define IBUS_RX_PIN 5
#define MAX_ROLL_ANGLE   30.0f  // consigne max roll (degrés)
#define MAX_PITCH_ANGLE  30.0f  // consigne max pitch (degrés)
#define MAX_YAW_RATE    180.0f  // consigne max yaw (deg/s)
#define LOOP_PERIOD_MS    10    // période de la boucle de contrôle

static const char *TAG = "drone";
static volatile ibus_channel_t last_channels[IBUS_CHANNEL_COUNT];
static volatile bool channels_updated = false;

static void channel_handler(ibus_channel_t *channels, void *cookie)
{
    memcpy((void *)last_channels, channels, sizeof(last_channels));
    channels_updated = true;
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32 Simple Drone ===");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialisation IMU + calibration gyroscope */
    float gyro_offset[2] = {0.0f, 0.0f};
    if (stabinit(gyro_offset) != 0) {
        ESP_LOGE(TAG, "Échec init stabilisation, arrêt.");
        return;
    }

    /* Initialisation iBUS */
    uart_lowlevel_config ibus_config;
    ibus_config.port   = IBUS_UART;
    ibus_config.rx_pin = IBUS_RX_PIN;
    ibus_config.tx_pin = IBUS_TX_PIN;

    ibus_context_t *ctx = ibus_init(&ibus_config);
    if (ctx != NULL) {
        ibus_set_channel_handler(ctx, channel_handler, NULL);
    }

    /* Initialisation moteurs (arming ESC) */
    pwminit();

    /* Initialisation contrôleur de vol */
    flight_ctrl_t fc;
    flight_ctrl_init(&fc);

    /* Variables de la boucle */
    uint8_t imu_data[6]    = {0};
    float accel_rp[2]      = {0.0f, 0.0f}; // roll, pitch accéléromètre
    float gyro_rp[2]       = {0.0f, 0.0f}; // roll, pitch gyroscope intégré
    float angle_roll       = 0.0f;
    float angle_pitch      = 0.0f;
    motor_output_t motors  = {0};

    int64_t prev_time = esp_timer_get_time();

    ESP_LOGI(TAG, "Boucle de contrôle démarrée (%d ms)", LOOP_PERIOD_MS);

    for (;;) {
        /* Calcul du dt réel */
        int64_t now = esp_timer_get_time();
        float dt = (float)(now - prev_time) / 1000000.0f;
        prev_time = now;

        /* Lecture IMU */
        printaccel(imu_data, accel_rp);
        printgyro(imu_data, gyro_rp, dt, gyro_offset);

        /* Fusion complémentaire */
        angle_roll  = complementaryFilter(gyro_rp[0], accel_rp[0]);
        angle_pitch = complementaryFilter(gyro_rp[1], accel_rp[1]);

        /* Lecture des consignes iBUS */
        float throttle = 0.0f;
        float roll_sp  = 0.0f;
        float pitch_sp = 0.0f;
        float yaw_sp   = 0.0f;

        if (channels_updated) {
            channels_updated = false;

            throttle = ibus_to_percent(last_channels[IBUS_CH_THROTTLE].value);
            roll_sp  = ibus_to_angle(last_channels[IBUS_CH_ROLL].value, MAX_ROLL_ANGLE);
            pitch_sp = ibus_to_angle(last_channels[IBUS_CH_PITCH].value, MAX_PITCH_ANGLE);
            yaw_sp   = ibus_to_angle(last_channels[IBUS_CH_YAW].value, MAX_YAW_RATE);

            fc.armed = (last_channels[IBUS_CH_ARM].value > ARM_THRESHOLD);
        }

        /* Calcul PID + mixage */
        flight_ctrl_update(&fc, throttle,
                           roll_sp, pitch_sp, yaw_sp,
                           angle_roll, angle_pitch, 0.0f,
                           dt, &motors);

        /* Commande moteurs */
        frontright(motors.front_right);
        frontleft(motors.front_left);
        backright(motors.back_right);
        backleft(motors.back_left);

        ESP_LOGD(TAG, "R=%.1f P=%.1f T=%.0f%% | FR=%.0f FL=%.0f BR=%.0f BL=%.0f",
                 angle_roll, angle_pitch, throttle,
                 motors.front_right, motors.front_left,
                 motors.back_right, motors.back_left);

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }

    ibus_deinit(ctx);
}
