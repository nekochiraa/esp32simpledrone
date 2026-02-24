#ifndef FLIGHT_CONTROLLER_H
#define FLIGHT_CONTROLLER_H

#include "pid.h"
#include <stdbool.h>

/* Mapping des canaux iBUS standard */
#define IBUS_CH_ROLL     0  // CH1 : aileron
#define IBUS_CH_PITCH    1  // CH2 : profondeur
#define IBUS_CH_THROTTLE 2  // CH3 : gaz
#define IBUS_CH_YAW      3  // CH4 : lacet
#define IBUS_CH_ARM      4  // CH5 : armement (interrupteur aux)

/* Plage iBUS brute */
#define IBUS_MIN  1000
#define IBUS_MAX  2000
#define IBUS_MID  1500

/* Seuils */
#define ARM_THRESHOLD     1500  // CH5 > seuil → armé
#define THROTTLE_MIN_PCT  5.0f  // gaz minimum pour activer le PID

/* Sortie moteur */
typedef struct {
    float front_right;  // 0-100 %
    float front_left;
    float back_right;
    float back_left;
} motor_output_t;

/* État complet du contrôleur de vol */
typedef struct {
    pid_controller_t pid_roll;
    pid_controller_t pid_pitch;
    pid_controller_t pid_yaw;
    bool armed;
} flight_ctrl_t;

/**
 * Initialise le contrôleur de vol et les 3 PID (roll, pitch, yaw).
 */
void flight_ctrl_init(flight_ctrl_t *fc);

/**
 * Convertit une valeur iBUS brute [1000..2000] en angle de consigne [-max_angle..+max_angle].
 */
float ibus_to_angle(uint16_t raw, float max_angle);

/**
 * Convertit une valeur iBUS brute [1000..2000] en pourcentage [0..100].
 */
float ibus_to_percent(uint16_t raw);

/**
 * Calcule les corrections PID et mixe les sorties moteurs.
 *
 * @param fc        état du contrôleur
 * @param throttle  gaz en % [0..100]
 * @param roll_sp   consigne roll (degrés)
 * @param pitch_sp  consigne pitch (degrés)
 * @param yaw_sp    consigne yaw (deg/s)
 * @param roll_m    mesure roll (degrés)
 * @param pitch_m   mesure pitch (degrés)
 * @param yaw_rate  vitesse yaw mesurée (deg/s)
 * @param dt        pas de temps (secondes)
 * @param out       sorties moteurs résultantes
 */
void flight_ctrl_update(flight_ctrl_t *fc,
                        float throttle,
                        float roll_sp, float pitch_sp, float yaw_sp,
                        float roll_m,  float pitch_m,  float yaw_rate,
                        float dt,
                        motor_output_t *out);

#endif // FLIGHT_CONTROLLER_H
