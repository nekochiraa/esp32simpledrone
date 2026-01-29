#include 'stabilisation.h'
#define I2C_MASTER_SCL_IO 20
#define I2C_MASTER_SDA_IO 21
#define ESC0_GPIO GPIO_NUM_17
  
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000
#define MPU6050_ADDR 0x68
//
#define MPU6050_WHO_AM_I 0x75
#define MPU6050_PWR_MGMT_1 0x6B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_GYRO_CONFIG 0x1B
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_GYRO_XOUT_H 0x43

static const char *TAG = "MPU6050";

static void i2c_master_init() {
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .scl_io_num = I2C_MASTER_SCL_IO,

    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,

    .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };
  i2c_param_config(I2C_MASTER_NUM, &conf);
  i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}
//mcpwm
static void esc_mcpwm_init(void) {
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, ESC0_GPIO);
  mcpwm_config_t pwm_config = {
    .frequency = 50,
    .cmpr_a = 0.0,
    .cmpr_b = 0.0,
    .duty_mode = MCPWM_DUTY_MODE_0,
    .counter_mode = MCPWM_UP_COUNTER
  };
        /* initialize MCPWM unit 0, timer 0 with the config */
  esp_err_t err = mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
    if (err != ESP_OK) {
    ESP_LOGI(TAG, "erreur esc");     
  }
  ESP_LOGI(TAG, "PMW init sucess");
}

static esp_err_t mpu6050_write_register(uint8_t reg, uint8_t value) {
  uint8_t write_buf[2] = {reg, value};
  return i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_ADDR, 
                                    write_buf, 2, pdMS_TO_TICKS(1000));
}

static esp_err_t mpu6050_read_register(uint8_t reg, uint8_t *data, size_t len) {
  return i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_ADDR, 
                                      &reg, 1, data, len, pdMS_TO_TICKS(1000));
}
static esp_err_t mpu6050_init() {
  esp_err_t ret;

  // Vérifier WHO_AM_I
  uint8_t who_am_i;
  ret = mpu6050_read_register(MPU6050_WHO_AM_I, &who_am_i, 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Impossible de lire WHO_AM_I");
    return ret;
  }
  ESP_LOGI(TAG, "WHO_AM_I = 0x%02X (attendu 0x68)", who_am_i);

  // Réveiller le MPU6050 (sortir du mode sleep)
  ret = mpu6050_write_register(MPU6050_PWR_MGMT_1, 0x00);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Erreur réveil MPU6050");
    return ret;
  }
  vTaskDelay(pdMS_TO_TICKS(100)); // Attendre la stabilisation

  // Configurer l'accéléromètre (±2g)
  ret = mpu6050_write_register(MPU6050_ACCEL_CONFIG, 0x00);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Erreur config accéléromètre");
    return ret;
  }
  ret = mpu6050_write_register(MPU6050_GYRO_CONFIG, 0x00);
  if (ret != ESP_OK){
    ESP_LOGE(TAG, "Erreur config Gyro");
    return ret;
  }
  ESP_LOGI(TAG, "MPU6050 initialisé avec succès");
  return ESP_OK;
}
static void kalmanfilter(float gyro, float accel, float *P, float *X){
  *X = *X+ gyro;
  float Q = 0.009571;
  float R = 0.036904;
  *P = *P + Q;
  float K = *P / (*P + R);      // R = 0.158280
  *X = *X + K * (accel - *X);  // z = angle from accel
  *P = (1 - K) * (*P); 
}
int printaccel(uint8_t *data, float *xyz){
  float temp[3];  
  if (mpu6050_read_register(MPU6050_ACCEL_XOUT_H, data, 6) == ESP_OK) {
    // Combiner les octets (big-endian)
    int16_t accel_x = (int16_t)((data[0] << 8) | data[1]);
    int16_t accel_y = (int16_t)((data[2] << 8) | data[3]);
    int16_t accel_z = (int16_t)((data[4] << 8) | data[5]);

    // Convertir en g (pour ±2g, sensibilité = 16384 LSB/g)
    temp[0] = accel_x / 16384.0;
    temp[1] = accel_y / 16384.0;
    temp[2] = accel_z / 16384.0;

    //roll
    xyz[0] = atan2(temp[1],temp[2])*180.0 / M_PI ;
    //pitch
    xyz[1] = atan2(-temp[0], sqrt(temp[1]*temp[1]+temp[2]*temp[2]))*180.0 / M_PI;
  } else {
    xyz[0] = 0.0;
    xyz[1] = 0.0;
    ESP_LOGE(TAG, "Erreur lecture accéléromètre");
    return 1;
  }
  return 0;
}
int printgyro(uint8_t *data, float *xyz, float dt, float *offset){
  if (mpu6050_read_register(MPU6050_GYRO_XOUT_H, data, 6) == ESP_OK) {

    // Combiner les octets (big-endian)
    int16_t gyro_x = (int16_t)((data[0] << 8) | data[1]);
    int16_t gyro_y = (int16_t)((data[2] << 8) | data[3]);
    int16_t gyro_z = (int16_t)((data[4] << 8) | data[5]);

    // Convertir en g (pour ±2g, sensibilité = 16384 LSB/g)
    //roll
    xyz[0] += ((gyro_x - offset[0]) / 131.0)*dt;
    //pitch
    xyz[1] += ((gyro_y - offset[1])/ 131.0)*dt;
  } else {
    xyz[0] = 0.0;
    xyz[1] = 0.0;
    return 1;
  }
  return 0;
}
static void getgyrooffset(uint8_t *data, float *xyzgyro, float *offset){
  float moy[2] = {0}; 
  ESP_LOGI(TAG, "offsetinit");
  for (uint16_t i=0;i < 1000; i++ ){
      if (mpu6050_read_register(MPU6050_GYRO_XOUT_H, data, 6) == ESP_OK) {

    // Combiner les octets (big-endian)
    int16_t gyro_x = (int16_t)((data[0] << 8) | data[1]);
    int16_t gyro_y = (int16_t)((data[2] << 8) | data[3]);
    int16_t gyro_z = (int16_t)((data[4] << 8) | data[5]);
  moy[0] = moy[0] + (float)gyro_x;
  moy[1] = moy[1] + (float)gyro_y;
    //ESP_LOGI(TAG, "%i / 1000", i);
    //ESP_LOGI(TAG, "%i", gyro_y);

    }
    else{
      ESP_LOGE(TAG, "error");
    }}
  offset[0] = moy[0]/1000;
  offset[1] = moy[1]/1000;
  

  }
float complementaryFilter(float gyro, float accel){
  return 0.996 * gyro + 0.004* accel;
}
float pidcontroll(float mesure, float consigne, float dt, float *pid){
  float erreur = (consigne - mesure)/90;
  pid[0] = pid[0] + erreur*dt;
  float derive = (pid[1]- erreur)/dt;
  return pid[2] * erreur + pid[3] * pid[0] + pid[4] * derive;
}
int stabinit(float *offset){
  uint8_t data[6] = 0;
  float xyzgyro[3] = 0;
  ESP_LOGI(TAG, "Initialisation I2C...");
  i2c_master_init();
  if (mpu6050_init() != ESP_OK) {
    ESP_LOGE(TAG, "Échec initialisation MPU6050");
    return 1;
  }
  getgyrooffset(&data, &xyzgyro, &offset)
}

