# ESP32 Simple Drone — Documentation

Contrôleur de vol pour quadricopter basé sur ESP32, utilisant ESP-IDF.

## Table des matières

1. [Fonctionnalités](#fonctionnalités)
2. [Matériel requis](#matériel-requis)
3. [Cartographie des broches](#cartographie-des-broches)
4. [Compilation et flash](#compilation-et-flash)
5. [Architecture](#architecture)
6. [Fichiers sources](#fichiers-sources)
7. [Modules détaillés](#modules-détaillés)
8. [Réglage des PID](#réglage-des-pid)
9. [Mode test](#mode-test)
10. [Configuration BMI270](#configuration-bmi270)
11. [Dépannage I2C](#dépannage-i2c)
12. [Mise en vol](#mise-en-vol)

---

## Fonctionnalités

- **Stabilisation PID** sur 3 axes (roll, pitch, yaw)
- **Lecture IMU** BMI270 (accéléromètre + gyroscope) via I2C
- **Réception iBUS** depuis un récepteur FlySky via UART
- **Pilotage de 4 ESC** via MCPWM (signal servo 50 Hz)
- **Filtre de Kalman** pour la fusion accéléromètre/gyroscope
- **Anti-windup** sur les PID et sécurités d'armement
- **Affichage des inputs télécommande** dans les logs série

---

## Matériel requis

- ESP32-S3 (ou variante compatible)
- BMI270 (IMU accéléromètre + gyroscope)
- 4 × ESC + moteurs brushless
- Récepteur iBUS (FlySky FS-iA6B ou similaire)
- Émetteur RC (FlySky FS-i6 ou similaire)

---

## Cartographie des broches

| Périphérique      | GPIO  | Notes                        |
|-------------------|-------|------------------------------|
| **ESC Front Right** | 15  | MCPWM0A                      |
| **ESC Front Left**  | 16  | MCPWM0B                      |
| **ESC Back Right**  | 17  | MCPWM1A                      |
| **ESC Back Left**   | 18  | MCPWM1B                      |
| **iBUS RX**         | 5   | UART1 RX                     |
| **iBUS TX**         | 4   | UART1 TX                     |
| **BMI270 SDA**      | 20  | I2C0 SDA                     |
| **BMI270 SCL**      | 21  | I2C0 SCL                     |

> **Note** : Les GPIOs sont maintenant distincts entre ESC et I2C.

---

## Compilation et flash

```bash
# Configurer l'environnement ESP-IDF
. $IDF_PATH/export.sh

# Compiler
idf.py build

# Flasher (remplacer PORT par votre port série)
idf.py -p PORT flash

# Moniteur série (affiche les inputs télécommande)
idf.py -p PORT monitor
```

---

## Architecture

```
Récepteur RC (iBUS) ──► Consignes (roll, pitch, yaw, throttle)
                                │
BMI270 (I2C) ──► Filtre ──► Mesures (roll, pitch)
                                │
                    ┌───────────▼──────────┐
                    │   3 × PID            │
                    │   (roll, pitch, yaw)  │
                    └───────────┬──────────┘
                                │
                    ┌───────────▼──────────┐
                    │  Mixage quadricopter │
                    │  (configuration X)   │
                    └───────────┬──────────┘
                                │
                    4 × ESC (MCPWM 50 Hz)
```

Le système fonctionne en boucle fermée à ~100 Hz (10 ms par itération).

---

## Fichiers sources

| Fichier                 | Rôle                                          |
|-------------------------|-----------------------------------------------|
| `main.c`                | Point d'entrée, boucle de contrôle principale |
| `pid.c` / `pid.h`       | Contrôleur PID générique réutilisable         |
| `flight_controller.c/h` | Mixage quadricopter + gestion armement        |
| `motor.c` / `motor.h`   | Pilotage ESC via MCPWM                        |
| `stabilization.c/h`     | Lecture BMI270, filtrage Kalman, calibration  |
| `controller.c/h`        | Utilitaire debug iBUS                         |
| `bmi270_config.h`       | Firmware BMI270 (données de configuration)    |
| `test_stabilisation.c`  | Tests de stabilisation                        |

---

## Modules détaillés

### Module PID (`pid.c` / `pid.h`)

```c
typedef struct {
    float kp, ki, kd;
    float integral, prev_error;
    float integral_limit, output_limit;
} pid_controller_t;
```

| Fonction | Description |
|----------|-------------|
| `pid_init(...)` | Initialise les gains et limites |
| `pid_reset(pid)` | Remet intégrale et erreur à 0 |
| `pid_compute(pid, setpoint, measure, dt)` | Calcule la correction |

### Module moteur (`motor.c` / `motor.h`)

| Fonction | Description |
|----------|-------------|
| `pwminit(void)` | Configure MCPWM 50 Hz et arme les ESC |
| `frontright(percent)` | Commande moteur avant droit (0-100 %) |
| `frontleft(percent)` | Commande moteur avant gauche |
| `backright(percent)` | Commande moteur arrière droit |
| `backleft(percent)` | Commande moteur arrière gauche |

**Conversion :** `duty = percent / 20 + 5` (0% → 1ms, 100% → 2ms)

### Module stabilisation (`stabilization.c/h`)

**Capteur BMI270 :**
- Accéléromètre : ±2g, sensibilité 16384 LSB/g
- Gyroscope : ±250°/s, sensibilité 131.072 LSB/(°/s)
- ODR : 200 Hz

| Fonction | Description |
|----------|-------------|
| `stabinit(offset)` | Init I2C + BMI270 + calibration gyro |
| `printaccel(data, xyz)` | Lit accéléromètre → roll/pitch (°) |
| `printgyro(data, xyz, dt, offset)` | Intègre gyroscope |
| `kalmanfilter(gyro, accel, P, X)` | Fusion Kalman |

### Contrôleur de vol (`flight_controller.c/h`)

**Mixage quadricopter (configuration X) :**
```
   FL (CCW)    FR (CW)
       \      /
        \    /
         \/
         /\
        /  \
   BL (CW)    BR (CCW)
```

| Moteur | Formule |
|--------|---------|
| Front Right | throttle − roll − pitch − yaw |
| Front Left  | throttle + roll − pitch + yaw |
| Back Right  | throttle − roll + pitch + yaw |
| Back Left   | throttle + roll + pitch − yaw |

**Armement :** CH5 > 1500 ET throttle > 5%

### Canaux iBUS

| Canal | Fonction | Plage |
|-------|----------|-------|
| CH1 | Roll (aileron) | 1000-2000 (centre 1500) |
| CH2 | Pitch (profondeur) | 1000-2000 (centre 1500) |
| CH3 | Throttle (gaz) | 1000-2000 |
| CH4 | Yaw (lacet) | 1000-2000 (centre 1500) |
| CH5 | Armement | 1000/2000 |

---

## Réglage des PID

### Méthode

1. **Kp seul** : augmenter jusqu'à oscillations légères, puis réduire de 30%
2. **Ajouter Kd** : amortir les oscillations
3. **Ajouter Ki** (optionnel) : corriger erreurs statiques (commencer à 0.01)

### Gains par défaut

| Axe   | Kp  | Ki  | Kd  |
|-------|-----|-----|-----|
| Roll  | 1.0 | 0.0 | 0.0 |
| Pitch | 1.0 | 0.0 | 0.0 |
| Yaw   | 2.0 | 0.0 | 0.0 |

### Limites

| Paramètre | Valeur | Rôle |
|-----------|--------|------|
| `PID_INTEGRAL_LIMIT` | 30.0 | Anti-windup |
| `PID_OUTPUT_LIMIT` | 40.0 | Correction max (%) |
| `THROTTLE_MIN_PCT` | 5.0 | Seuil activation PID |

---

## Mode test

Pour tester la stabilisation sans moteurs ni télécommande :

1. Ouvrir `main/main.c`
2. Décommenter `#define MODE_TEST`
3. Compiler et flasher

Le mode test affiche les angles en temps réel et vérifie le bon fonctionnement du BMI270.

---

## Configuration BMI270

### Paramètres optimaux

| Capteur | Plage | Sensibilité | ODR |
|---------|-------|-------------|-----|
| Accel | ±2g | 16384 LSB/g | 200 Hz |
| Gyro | ±250°/s | 131.072 LSB/(°/s) | 200 Hz |

### Filtre de Kalman

```c
Q = 0.003  // Confiance gyro (faible drift BMI270)
R = 0.030  // Confiance accel
```

### Adresse I2C

- `0x69` si SDO → GND (par défaut dans le code)
- `0x68` si SDO → VCC

Modifier `BMI270_ADDR` dans `stabilization.c` si nécessaire.

---

## Dépannage I2C

### Erreur "NACK detected"

1. Vérifier connexions SDA/SCL
2. Vérifier alimentation 3.3V
3. Tester adresse 0x68 si 0x69 ne répond pas
4. Ajouter pull-ups externes 4.7kΩ si nécessaire

### CHIP_ID incorrect

- Attendu : `0x24`
- Vérifier que c'est bien un BMI270 et pas un MPU6050

### Timeout I2C

- Réduire fréquence à 100 kHz
- Câbles plus courts (<10 cm)
- Ajouter condensateur 100nF sur VCC

---

## Mise en vol

### Checklist

1. **Vérifier le sens des moteurs** (configuration X)
2. **Vérifier le sens des corrections PID** (inverser si le drone s'emballe)
3. **Vérifier la séquence d'armement ESC**

### Premier vol

1. Throttle à 0, armer avec CH5
2. Monter les gaz **très doucement**
3. Si oscillations → baisser Kp
4. Si instable → augmenter Kp

---

## Dépendances

- [ESP-IDF](https://github.com/espressif/esp-idf) >= 5.0
- [zorxx/ibus](https://components.espressif.com/components/zorxx/ibus) ^1.0.0

---

## Licence

MIT
