#include "stabilization.h"
#include "bmi270_config.h"

#define I2C_MASTER_SCL_IO    21
#define I2C_MASTER_SDA_IO    20
#define I2C_MASTER_PORT      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_TIMEOUT_MS       2000

// BMI270 Registres
#define BMI270_ADDR          0x69
#define BMI270_CHIP_ID       0x00
#define BMI270_CHIP_ID_VAL   0x24
#define BMI270_ACC_DATA      0x0C
#define BMI270_GYR_DATA      0x12
#define BMI270_ACC_CONF      0x40
#define BMI270_ACC_RANGE     0x41
#define BMI270_GYR_CONF      0x42
#define BMI270_GYR_RANGE     0x43
#define BMI270_PWR_CONF      0x7C
#define BMI270_PWR_CTRL      0x7D
#define BMI270_CMD           0x7E
#define BMI270_INIT_CTRL     0x59
#define BMI270_INIT_ADDR_0   0x5B
#define BMI270_INIT_ADDR_1   0x5C
#define BMI270_INIT_DATA     0x5E
#define BMI270_INTERNAL_STATUS 0x21

// Sensibilités pour ±2g et ±250°/s (précision maximale)
#define ACCEL_SENSITIVITY    16384.0f   // LSB/g pour ±2g
#define GYRO_SENSITIVITY     131.072f   // LSB/(°/s) pour ±250°/s

static const char *TAG = "BMI270";

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t bmi270_dev = NULL;

static esp_err_t i2c_master_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_MASTER_PORT,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur création bus I2C: %s", esp_err_to_name(ret));
        return ret;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMI270_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &bmi270_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur ajout device I2C: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t bmi270_write_register(uint8_t reg, uint8_t value)
{
    uint8_t write_buf[2] = {reg, value};
    return i2c_master_transmit(bmi270_dev, write_buf, 2, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t bmi270_read_register(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(bmi270_dev, &reg, 1, data, len, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t bmi270_init(void)
{
    esp_err_t ret;
    uint8_t chip_id;

    // Soft reset
    ret = bmi270_write_register(BMI270_CMD, 0xB6);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur soft reset BMI270");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // Vérifier CHIP_ID
    ret = bmi270_read_register(BMI270_CHIP_ID, &chip_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Impossible de lire CHIP_ID");
        return ret;
    }
    ESP_LOGI(TAG, "CHIP_ID = 0x%02X (attendu 0x24)", chip_id);
    if (chip_id != BMI270_CHIP_ID_VAL) {
        ESP_LOGW(TAG, "CHIP_ID inattendu, continue quand même...");
    }

    // Désactiver le mode Advanced Power Save pour l'initialisation
    ret = bmi270_write_register(BMI270_PWR_CONF, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config power");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(1));

    // Préparer le chargement du firmware
    ret = bmi270_write_register(BMI270_INIT_CTRL, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur préparation init");
        return ret;
    }

    // Charger le firmware BMI270 (obligatoire pour le fonctionnement)
    ESP_LOGI(TAG, "Chargement firmware BMI270 (%d bytes)...", BMI270_CONFIG_FILE_SIZE);
    
    // Écrire le firmware par blocs via le registre INIT_DATA
    // IMPORTANT: Il faut écrire l'adresse dans INIT_ADDR avant chaque burst
    const uint16_t BURST_SIZE = 32;
    for (uint16_t i = 0; i < BMI270_CONFIG_FILE_SIZE; i += BURST_SIZE) {
        // Écrire l'adresse du burst (divisée par 2 car adressage en words)
        uint16_t addr = i / 2;
        ret = bmi270_write_register(BMI270_INIT_ADDR_0, (uint8_t)(addr & 0x0F));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erreur écriture INIT_ADDR_0");
            return ret;
        }
        ret = bmi270_write_register(BMI270_INIT_ADDR_1, (uint8_t)((addr >> 4) & 0xFF));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erreur écriture INIT_ADDR_1");
            return ret;
        }
        
        uint16_t remaining = BMI270_CONFIG_FILE_SIZE - i;
        uint16_t len = (remaining < BURST_SIZE) ? remaining : BURST_SIZE;
        
        // Construire le buffer: [reg_addr, data...]
        uint8_t write_buf[BURST_SIZE + 1];
        write_buf[0] = BMI270_INIT_DATA;
        for (uint16_t j = 0; j < len; j++) {
            write_buf[j + 1] = bmi270_config_file[i + j];
        }
        
        ret = i2c_master_transmit(bmi270_dev, write_buf, len + 1, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erreur écriture firmware à l'offset %d", i);
            return ret;
        }
    }
    ESP_LOGI(TAG, "Firmware chargé avec succès");

    // Démarrer l'initialisation
    ret = bmi270_write_register(BMI270_INIT_CTRL, 0x01);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur démarrage init");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(150));

    // Vérifier le statut d'initialisation (bit 0 = init ok)
    uint8_t status;
    ret = bmi270_read_register(BMI270_INTERNAL_STATUS, &status, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Impossible de lire le statut interne");
    } else {
        ESP_LOGI(TAG, "Status interne: 0x%02X %s", status, 
                 (status & 0x01) ? "(init OK)" : "(init FAILED!)");
        if (!(status & 0x01)) {
            ESP_LOGE(TAG, "Initialisation firmware échouée!");
            return ESP_FAIL;
        }
    }

    // Configuration accéléromètre: ODR 200Hz, normal mode
    ret = bmi270_write_register(BMI270_ACC_CONF, 0xA8); // odr=200Hz, bwp=normal, perf_mode=1
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config accéléromètre");
        return ret;
    }

    // Plage accéléromètre: ±2g
    ret = bmi270_write_register(BMI270_ACC_RANGE, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config plage accel");
        return ret;
    }

    // Configuration gyroscope: ODR 200Hz, normal mode
    ret = bmi270_write_register(BMI270_GYR_CONF, 0xA9); // odr=200Hz, bwp=normal, noise_perf=1
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config gyroscope");
        return ret;
    }

    // Plage gyroscope: ±250°/s
    ret = bmi270_write_register(BMI270_GYR_RANGE, 0x03);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config plage gyro");
        return ret;
    }

    // Activer accel et gyro
    ret = bmi270_write_register(BMI270_PWR_CTRL, 0x0E); // acc_en=1, gyr_en=1, temp_en=1
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur activation capteurs");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "BMI270 initialisé avec succès (±2g, ±250°/s @ 200Hz)");
    return ESP_OK;
}

void kalmanfilter(float gyro, float accel, float *P, float *X)
{
    // Paramètres optimisés pour BMI270 (faible drift, bon accel)
    static const float Q = 0.003f;   // Confiance élevée gyro BMI270
    static const float R = 0.030f;   // Confiance modérée accel

    *X += gyro;
    *P += Q;
    float K = *P / (*P + R);
    *X += K * (accel - *X);
    *P  = (1.0f - K) * (*P);
}

int printaccel(uint8_t *data, float *xyz, float *offset)
{
    // BMI270: données en little-endian (LSB first)
    if (bmi270_read_register(BMI270_ACC_DATA, data, 6) != ESP_OK) {
        xyz[0] = 0.0f;
        xyz[1] = 0.0f;
        ESP_LOGE(TAG, "Erreur lecture accéléromètre");
        return 1;
    }

    // Little-endian: LSB en premier
    int16_t accel_x = (int16_t)(data[0] | (data[1] << 8));
    int16_t accel_y = (int16_t)(data[2] | (data[3] << 8));
    int16_t accel_z = (int16_t)(data[4] | (data[5] << 8));

    // Remapping des axes: le capteur est monté avec Y pointant vers le haut
    // Donc on échange Y et Z pour avoir Z_drone = Y_sensor
    float ax = accel_x / ACCEL_SENSITIVITY;
    float ay = accel_z / ACCEL_SENSITIVITY;   // Y_drone = Z_sensor
    float az = (accel_y / ACCEL_SENSITIVITY);   // Z_drone = Y_sensor (gravité)

    float roll_raw  = atan2f(ay, az) * 180.0f / M_PI;
    float pitch_raw = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;

    // Soustrait l'offset de calibration pour que la position initiale = 0
    xyz[0] = roll_raw  - offset[0];
    xyz[1] = pitch_raw - offset[1];
    return 0;
}

int printgyro(uint8_t *data, float *xyz, float dt, float *offset)
{
    // BMI270: données en little-endian (LSB first)
    if (bmi270_read_register(BMI270_GYR_DATA, data, 6) != ESP_OK) {
        xyz[0] = 0.0f;
        xyz[1] = 0.0f;
        xyz[2] = 0.0f;
        return 1;
    }

    // Little-endian: LSB en premier
    int16_t gyro_x = (int16_t)(data[0] | (data[1] << 8));
    int16_t gyro_y = (int16_t)(data[2] | (data[3] << 8));
    int16_t gyro_z = (int16_t)(data[4] | (data[5] << 8));

    // Remapping des axes: même rotation que l'accéléromètre
    // X_drone = X_sensor, Y_drone = Z_sensor, Z_drone = Y_sensor
    xyz[0] += ((gyro_x - offset[0]) / GYRO_SENSITIVITY) * dt; // roll (X)
    xyz[1] += ((gyro_z - offset[2]) / GYRO_SENSITIVITY) * dt; // pitch (Y_drone = Z_sensor)
    xyz[2]  =  (gyro_y - offset[1]) / GYRO_SENSITIVITY;       // yaw rate (Z_drone = Y_sensor)
    return 0;
}

static void getgyrooffset(uint8_t *data, float *offset)
{
    float moy[3] = {0.0f, 0.0f, 0.0f};
    const int NUM_SAMPLES = 2000;

    ESP_LOGI(TAG, "Calibration gyroscope BMI270 (%d échantillons)...", NUM_SAMPLES);
    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (bmi270_read_register(BMI270_GYR_DATA, data, 6) == ESP_OK) {
            int16_t gyro_x = (int16_t)(data[0] | (data[1] << 8));
            int16_t gyro_y = (int16_t)(data[2] | (data[3] << 8));
            int16_t gyro_z = (int16_t)(data[4] | (data[5] << 8));
            moy[0] += (float)gyro_x;
            moy[1] += (float)gyro_y;
            moy[2] += (float)gyro_z;
        } else {
            ESP_LOGE(TAG, "Erreur lecture gyroscope");
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    offset[0] = moy[0] / (float)NUM_SAMPLES;
    offset[1] = moy[1] / (float)NUM_SAMPLES;
    offset[2] = moy[2] / (float)NUM_SAMPLES;
    ESP_LOGI(TAG, "Gyro offsets: X=%.2f Y=%.2f Z=%.2f", offset[0], offset[1], offset[2]);
}

static void getacceloffset(uint8_t *data, float *offset)
{
    float moy[2] = {0.0f, 0.0f};
    const int NUM_SAMPLES = 500;

    ESP_LOGI(TAG, "Calibration accéléromètre (position initiale = 0°)...");
    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (bmi270_read_register(BMI270_ACC_DATA, data, 6) == ESP_OK) {
            int16_t accel_x = (int16_t)(data[0] | (data[1] << 8));
            int16_t accel_y = (int16_t)(data[2] | (data[3] << 8));
            int16_t accel_z = (int16_t)(data[4] | (data[5] << 8));

            float ax = accel_x / ACCEL_SENSITIVITY;
            float ay = accel_z / ACCEL_SENSITIVITY;
            float az = accel_y / ACCEL_SENSITIVITY;

            moy[0] += atan2f(ay, az) * 180.0f / M_PI;                          // roll
            moy[1] += atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;  // pitch
        } else {
            ESP_LOGE(TAG, "Erreur lecture accéléromètre");
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    offset[0] = moy[0] / (float)NUM_SAMPLES;
    offset[1] = moy[1] / (float)NUM_SAMPLES;
    ESP_LOGI(TAG, "Accel offsets: Roll=%.2f° Pitch=%.2f°", offset[0], offset[1]);
}

float complementaryFilter(float gyro, float accel)
{
    // Optimisé BMI270: 98% gyro (très stable) / 2% accel
    return 0.98f * gyro + 0.02f * accel;
}

float pidcontroll(float mesure, float consigne, float dt, float *pid)
{
    float erreur = (consigne - mesure) / 90.0f;
    pid[0]      += erreur * dt;                // integral
    float derive = (erreur - pid[1]) / dt;     // dérivée
    pid[1]       = erreur;                     // sauvegarde erreur précédente
    return pid[2] * erreur + pid[3] * pid[0] + pid[4] * derive;
}

int stabinit(float *gyro_offset, float *accel_offset)
{
    uint8_t data[6] = {0};

    ESP_LOGI(TAG, "Initialisation I2C (new i2c_master driver)...");
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "Échec initialisation I2C");
        return 1;
    }

    if (bmi270_init() != ESP_OK) {
        ESP_LOGE(TAG, "Échec initialisation BMI270");
        return 1;
    }

    ESP_LOGW(TAG, "=== NE PAS BOUGER LE DRONE PENDANT LA CALIBRATION ===");
    getgyrooffset(data, gyro_offset);
    getacceloffset(data, accel_offset);
    ESP_LOGI(TAG, "Calibration terminée, position actuelle = 0°");
    return 0;
}
