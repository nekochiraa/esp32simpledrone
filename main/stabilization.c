#include "stabilization.h"

#define I2C_MASTER_SCL_IO    20
#define I2C_MASTER_SDA_IO    21
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   400000
#define MPU6050_ADDR         0x68
#define MPU6050_WHO_AM_I     0x75
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_GYRO_CONFIG  0x1B
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_GYRO_XOUT_H  0x43

static const char *TAG = "MPU6050";

static void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_MASTER_SDA_IO,
        .scl_io_num       = I2C_MASTER_SCL_IO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static esp_err_t mpu6050_write_register(uint8_t reg, uint8_t value)
{
    uint8_t write_buf[2] = {reg, value};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_ADDR,
                                      write_buf, 2, pdMS_TO_TICKS(1000));
}

static esp_err_t mpu6050_read_register(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_ADDR,
                                        &reg, 1, data, len, pdMS_TO_TICKS(1000));
}

static esp_err_t mpu6050_init(void)
{
    esp_err_t ret;
    uint8_t who_am_i;

    ret = mpu6050_read_register(MPU6050_WHO_AM_I, &who_am_i, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Impossible de lire WHO_AM_I");
        return ret;
    }
    ESP_LOGI(TAG, "WHO_AM_I = 0x%02X (attendu 0x68)", who_am_i);

    ret = mpu6050_write_register(MPU6050_PWR_MGMT_1, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur réveil MPU6050");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    ret = mpu6050_write_register(MPU6050_ACCEL_CONFIG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config accéléromètre");
        return ret;
    }

    ret = mpu6050_write_register(MPU6050_GYRO_CONFIG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config gyroscope");
        return ret;
    }

    ESP_LOGI(TAG, "MPU6050 initialisé avec succès");
    return ESP_OK;
}

void kalmanfilter(float gyro, float accel, float *P, float *X)
{
    static const float Q = 0.009571f;
    static const float R = 0.036904f;

    *X += gyro;
    *P += Q;
    float K = *P / (*P + R);
    *X += K * (accel - *X);
    *P  = (1.0f - K) * (*P);
}

int printaccel(uint8_t *data, float *xyz)
{
    if (mpu6050_read_register(MPU6050_ACCEL_XOUT_H, data, 6) != ESP_OK) {
        xyz[0] = 0.0f;
        xyz[1] = 0.0f;
        ESP_LOGE(TAG, "Erreur lecture accéléromètre");
        return 1;
    }

    int16_t accel_x = (int16_t)((data[0] << 8) | data[1]);
    int16_t accel_y = (int16_t)((data[2] << 8) | data[3]);
    int16_t accel_z = (int16_t)((data[4] << 8) | data[5]);

    float ax = accel_x / 16384.0f;
    float ay = accel_y / 16384.0f;
    float az = accel_z / 16384.0f;

    xyz[0] = atan2f(ay, az) * 180.0f / M_PI;                         // roll
    xyz[1] = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI; // pitch
    return 0;
}

int printgyro(uint8_t *data, float *xyz, float dt, float *offset)
{
    if (mpu6050_read_register(MPU6050_GYRO_XOUT_H, data, 6) != ESP_OK) {
        xyz[0] = 0.0f;
        xyz[1] = 0.0f;
        xyz[2] = 0.0f;
        return 1;
    }

    int16_t gyro_x = (int16_t)((data[0] << 8) | data[1]);
    int16_t gyro_y = (int16_t)((data[2] << 8) | data[3]);
    int16_t gyro_z = (int16_t)((data[4] << 8) | data[5]);

    xyz[0] += ((gyro_x - offset[0]) / 131.0f) * dt; // roll
    xyz[1] += ((gyro_y - offset[1]) / 131.0f) * dt; // pitch
    xyz[2]  =  (gyro_z - offset[2]) / 131.0f;       // yaw rate (deg/s)
    return 0;
}

static void getgyrooffset(uint8_t *data, float *xyzgyro, float *offset)
{
    float moy[3] = {0.0f, 0.0f, 0.0f};

    ESP_LOGI(TAG, "Calibration gyroscope...");
    for (uint16_t i = 0; i < 1000; i++) {
        if (mpu6050_read_register(MPU6050_GYRO_XOUT_H, data, 6) == ESP_OK) {
            int16_t gyro_x = (int16_t)((data[0] << 8) | data[1]);
            int16_t gyro_y = (int16_t)((data[2] << 8) | data[3]);
            int16_t gyro_z = (int16_t)((data[4] << 8) | data[5]);
            moy[0] += (float)gyro_x;
            moy[1] += (float)gyro_y;
            moy[2] += (float)gyro_z;
        } else {
            ESP_LOGE(TAG, "Erreur lecture gyroscope");
        }
    }
    offset[0] = moy[0] / 1000.0f;
    offset[1] = moy[1] / 1000.0f;
    offset[2] = moy[2] / 1000.0f;
}

float complementaryFilter(float gyro, float accel)
{
    return 0.996f * gyro + 0.004f * accel;
}

float pidcontroll(float mesure, float consigne, float dt, float *pid)
{
    float erreur = (consigne - mesure) / 90.0f;
    pid[0]      += erreur * dt;                // integral
    float derive = (erreur - pid[1]) / dt;     // dérivée
    pid[1]       = erreur;                     // sauvegarde erreur précédente
    return pid[2] * erreur + pid[3] * pid[0] + pid[4] * derive;
}

int stabinit(float *offset)
{
    uint8_t data[6]   = {0};
    float xyzgyro[3]  = {0.0f, 0.0f, 0.0f};

    ESP_LOGI(TAG, "Initialisation I2C...");
    i2c_master_init();

    if (mpu6050_init() != ESP_OK) {
        ESP_LOGE(TAG, "Échec initialisation MPU6050");
        return 1;
    }

    getgyrooffset(data, xyzgyro, offset);
    return 0;
}
