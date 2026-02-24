#ifndef PID_H
#define PID_H

typedef struct {
    float kp;
    float ki;
    float kd;
    float integral;
    float prev_error;
    float integral_limit; // anti-windup : borne sur le terme intégral
    float output_limit;   // borne sur la sortie PID
} pid_controller_t;

/**
 * Initialise un contrôleur PID avec les gains et limites donnés.
 */
void pid_init(pid_controller_t *pid, float kp, float ki, float kd,
              float integral_limit, float output_limit);

/**
 * Remet à zéro l'état interne (intégrale et erreur précédente).
 */
void pid_reset(pid_controller_t *pid);

/**
 * Calcule la sortie PID.
 * @param pid      contrôleur
 * @param setpoint consigne (degrés)
 * @param measure  mesure actuelle (degrés)
 * @param dt       pas de temps en secondes
 * @return correction à appliquer (bornée par output_limit)
 */
float pid_compute(pid_controller_t *pid, float setpoint, float measure, float dt);

#endif // PID_H
