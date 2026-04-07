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
#include "main.h"

// ========================================
// MODE TEST: Décommentez la ligne ci-dessous pour lancer les tests
// au lieu du mode drone normal
// ========================================
//#define MODE_TEST
// ========================================

#define IBUS_UART   UART_NUM_1
#define IBUS_TX_PIN 4
#define IBUS_RX_PIN 5
#define MAX_ROLL_ANGLE   15.0f  // consigne max roll (degrés)
#define MAX_PITCH_ANGLE  15.0f  // consigne max pitch (degrés)
#define MAX_YAW_RATE    90.0f  // consigne max yaw (deg/s)
#define LOOP_PERIOD_MS    10    // période de la boucle de contrôle
#define FAILSAFE_TIMEOUT_US 500000 // 500 ms sans signal iBUS → arrêt moteurs

static const char *TAG = "drone";
static volatile ibus_channel_t last_channels[IBUS_CHANNEL_COUNT];
static volatile bool channels_updated = false;
static volatile int64_t last_ibus_time = 0;

static void channel_handler(ibus_channel_t *channels, void *cookie)
{
    memcpy((void *)last_channels, channels, sizeof(last_channels));
    channels_updated = true;
    last_ibus_time = esp_timer_get_time();
}

void app_main(void)
{
#ifdef MODE_TEST
    ESP_LOGI(TAG, "=== MODE TEST ACTIVÉ ===");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Lancer les tests de stabilisation
    test_stabilisation_simple();
    
    ESP_LOGI(TAG, "Tests terminés. Le programme va boucler indéfiniment.");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
#else
    ESP_LOGI(TAG, "=== ESP32 Simple Drone ===");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialisation IMU + calibration gyroscope et accéléromètre */
    float gyro_offset[3]  = {0.0f, 0.0f, 0.0f};
    float accel_offset[2] = {0.0f, 0.0f};  // roll, pitch offset
    if (stabinit(gyro_offset, accel_offset) != 0) {
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
    fc.armed = true;  // toujours armé, la bride CH5 contrôle la puissance

    /* Variables de la boucle */
    uint8_t imu_data[6]    = {0};
    float accel_rp[2]      = {0.0f, 0.0f};         // roll, pitch accéléromètre
    float gyro_rp[3]       = {0.0f, 0.0f, 0.0f};  // roll, pitch intégrés + yaw rate
    float angle_roll       = 0.0f;
    float angle_pitch      = 0.0f;
    float kalman_P[2]      = {1.0f, 1.0f};         // covariance Kalman (roll, pitch)
    motor_output_t motors  = {0};

    /* Consignes iBUS persistantes (gardent leur valeur entre les trames) */
    float throttle = 0.0f;
    float roll_sp  = 0.0f;
    float pitch_sp = 0.0f;
    float yaw_sp   = 0.0f;

    int64_t prev_time = esp_timer_get_time();

    ESP_LOGI(TAG, "Boucle de contrôle démarrée (%d ms)", LOOP_PERIOD_MS);

    for (;;) {
        /* Calcul du dt réel */
        int64_t now = esp_timer_get_time();
        float dt = (float)(now - prev_time) / 1000000.0f;
        prev_time = now;

        /* Lecture IMU */
        printaccel(imu_data, accel_rp, accel_offset);
        printgyro(imu_data, gyro_rp, dt, gyro_offset);

        /* Fusion Kalman (remplace le filtre complémentaire) */
        kalmanfilter(gyro_rp[0], accel_rp[0], &kalman_P[0], &angle_roll);
        kalmanfilter(gyro_rp[1], accel_rp[1], &kalman_P[1], &angle_pitch);

        /* Failsafe : si pas de signal iBUS depuis 500 ms → couper */
        bool signal_lost = (now - last_ibus_time) > FAILSAFE_TIMEOUT_US;
        if (signal_lost) {
            fc.armed = false;
            throttle = 0.0f;
            if ((now - last_ibus_time) < FAILSAFE_TIMEOUT_US + 1000000) {
                ESP_LOGW(TAG, "FAILSAFE: signal perdu, moteurs coupés");
            }
        }

        /* Mise à jour des consignes iBUS (seulement quand nouvelle trame reçue) */
        if (channels_updated) {
            channels_updated = false;

            /* Calcul de la bride : CH5 définit le throttle max autorisé */
            float limiter = ibus_to_percent(last_channels[IBUS_CH_LIMITER].value);
            float raw_throttle = ibus_to_percent(last_channels[IBUS_CH_THROTTLE].value);
            throttle = raw_throttle * limiter / 100.0f;

            roll_sp  = ibus_to_angle(last_channels[IBUS_CH_ROLL].value, MAX_ROLL_ANGLE);
            pitch_sp = ibus_to_angle(last_channels[IBUS_CH_PITCH].value, MAX_PITCH_ANGLE);
            yaw_sp   = ibus_to_angle(last_channels[IBUS_CH_YAW].value, MAX_YAW_RATE);
        }

        /* Calcul PID + mixage */
        flight_ctrl_update(&fc, throttle,
                           roll_sp, pitch_sp, yaw_sp,
                           angle_roll, angle_pitch, gyro_rp[2],
                           dt, &motors);

        /* Commande moteurs */
        frontright(motors.front_right);
        frontleft(motors.front_left);
        backright(motors.back_right);
        backleft(motors.back_left);

        /* Affichage du % envoyé aux moteurs */
        ESP_LOGI(TAG, "MOTEURS: FR=%.1f%% FL=%.1f%% BR=%.1f%% BL=%.1f%% | Armed=%d",
                 motors.front_right, motors.front_left,
                 motors.back_right, motors.back_left, fc.armed);

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }

    ibus_deinit(ctx);
#endif // MODE_TEST
}
