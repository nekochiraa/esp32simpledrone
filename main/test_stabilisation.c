#include "stabilization.h"
#include "pid.h"
#include "main.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "TEST_STAB";

/**
 * Affiche les angles actuels en continu (Roll, Pitch, Yaw)
 * Utilise le filtre de Kalman pour la fusion des données
 */
void test_stabilisation_simple(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   AFFICHAGE ANGLES EN TEMPS REEL");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // Initialisation du capteur
    float offset[3] = {0};
    
    if (stabinit(offset) != 0) {
        ESP_LOGE(TAG, "Echec initialisation capteur!");
        return;
    }
    
    ESP_LOGI(TAG, "Capteur initialise");
    ESP_LOGI(TAG, "Offsets gyro: X=%.2f Y=%.2f Z=%.2f", offset[0], offset[1], offset[2]);
    ESP_LOGI(TAG, "");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Variables pour la lecture
    uint8_t data[6];
    float gyro[3] = {0, 0, 0};
    float accel[2] = {0, 0};
    float dt = 0.01f;  // 10ms = 100Hz
    
    // Variables Kalman pour Roll et Pitch
    float P_roll = 1.0f, P_pitch = 1.0f;
    float angle_roll = 0.0f, angle_pitch = 0.0f;
    float yaw = 0.0f;
    
    ESP_LOGI(TAG, "Lecture angles en continu (Ctrl+C pour arreter)");
    ESP_LOGI(TAG, "----------------------------------------");
    
    // Boucle infinie d'affichage des angles
    while (1) {
        // Lecture des capteurs
        if (printgyro(data, gyro, dt, offset) != 0 || 
            printaccel(data, accel) != 0) {
            ESP_LOGE(TAG, "Erreur lecture capteurs");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // Fusion Kalman pour Roll et Pitch
        kalmanfilter(gyro[0], accel[0], &P_roll, &angle_roll);
        kalmanfilter(gyro[1], accel[1], &P_pitch, &angle_pitch);
        
        // Integration du Yaw (pas d'accel pour le yaw)
        yaw += gyro[2] * dt;
        
        // Limite yaw entre -180 et 180
        if (yaw > 180.0f) yaw -= 360.0f;
        if (yaw < -180.0f) yaw += 360.0f;
        
        // Affichage des angles actuels
        ESP_LOGI(TAG, "Roll: %7.2f°  |  Pitch: %7.2f°  |  Yaw: %7.2f°", 
                 angle_roll, angle_pitch, yaw);
        
        vTaskDelay(pdMS_TO_TICKS(100));  // 10Hz affichage
    }
}
