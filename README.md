| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

# Hello World Example

Starts a FreeRTOS task to print "Hello World".

(See the README.md file in the upper level 'examples' directory for more information about examples.)

## How to use example

Follow detailed instructions provided specifically for this example.

Select the instructions depending on Espressif chip installed on your development board:

- [ESP32 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html)
- [ESP32-S2 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html)


## Example folder contents

The project **hello_world** contains one source file in C language [hello_world_main.c](main/hello_world_main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt` files that provide set of directives and instructions describing the project's source files and targets (executable, library, or both).

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── pytest_hello_world.py      Python script used for automated testing
├── main
│   ├── CMakeLists.txt
│   └── hello_world_main.c
└── README.md                  This is the file you are currently reading
```

For more information on structure and contents of ESP-IDF projects, please refer to Section [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) of the ESP-IDF Programming Guide.

## Troubleshooting

* Program upload failure

    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-idf/issues)

We will get back to you as soon as possible.

## Documentation du code (main/)

Cette section décrit le code du dossier `main/` (contrôle ESC, lecture IBUS, primitives de stabilisation IMU).

Aperçu de l’architecture
- Entrées: IBUS (récepteur radio) sur UART1
- Traitement: boucle de contrôle dans `app_main` (main.c), primitives de stabilisation (stabilization.c)
- Sorties: ESC via MCPWM (motor.c)

Cartographie des broches (par défaut)
- ESC (MCPWM, 50 Hz):
  - Avant droit (Front Right): GPIO 17 (MCPWM0A)
  - Avant gauche (Front Left): GPIO 19 (MCPWM0B)
  - Arrière droit (Back Right): GPIO 20 (MCPWM1A)
  - Arrière gauche (Back Left): GPIO 21 (MCPWM1B)
- IBUS (UART1): RX = GPIO 5, TX = GPIO 4
- IMU MPU6050 (I2C0): SCL = GPIO 20, SDA = GPIO 21
  - Attention: 20/21 sont aussi utilisés pour des ESC dans motor.c; évitez tout conflit de broches lors du câblage.

Fichiers et responsabilités
- main.c
  - Initialise l’event loop et IBUS, configure le PWM via `pwminit()`, puis lit en boucle le canal IBUS 3 (index 2) et en déduit une puissance.
  - Conversion IBUS→ESC: (valeur − 1000) / 10 ≈ 0→100%, appliquée à `frontright()`. Exemple: 1000→0%, 1500→50%, 2000→100%.
  - Démonstration: seul le moteur avant droit est piloté par défaut; ajoutez `frontleft/backright/backleft` pour piloter les autres moteurs.
- motor.h / motor.c
  - API ESC (MCPWM à 50 Hz). Arming séquentiel au démarrage (5% puis 10%) pour initialiser les ESC.
  - Fonctions:
    - `void pwminit(void)`: configure MCPWM et effectue l’arming.
    - `void frontright(float percent)`, `frontleft`, `backright`, `backleft`: applique un duty équivalent à percent∈[0..100], saturé par `MAXBRIDE`.
  - Mappage PWM: percent→duty% = percent/20 + 5 (0%→5%≈1 ms, 100%→10%≈2 ms).
- controller.h / controller.c
  - Exemple IBUS (journalisation des canaux) via `channelget()`. Utile pour vérifier le câblage et la réception IBUS.
  - Remarque: certaines déclarations liées à la stabilisation figurent dans controller.h; consolidation future possible.
- stabilization.h / stabilization.c
  - Primitives IMU (MPU6050 via I2C) et utilitaires de filtrage/contrôle.
  - API principale:
    - `int stabinit(float *offset)`: init I2C/MPU6050, mesure les offsets gyro (moyenne sur ~1000 échantillons). 0 si OK.
    - `int printaccel(uint8_t *data, float *xyz)`: lit l’accéléromètre et calcule roll/pitch (deg) dans `xyz[0..1]`.
    - `int printgyro(uint8_t *data, float *xyz, float dt, float *offset)`: intègre le gyro (deg/s) sur dt et corrige l’offset.
    - `float complementaryFilter(float gyro, float accel)`: fusion angle gyro/accel.
    - `float pidcontroll(float mesure, float consigne, float dt, float *pid)`: PID (pid[0]=intégrale, pid[2]=Kp, pid[3]=Ki, pid[4]=Kd; l’appelant doit gérer l’erreur précédente).

Flux d’exécution typique
1) `pwminit()` arme les ESC (50 Hz). 2) IBUS met à jour `last_channels` via un handler. 3) La boucle principale convertit CH3→% et commande les moteurs.

Sécurité & limites
- Démarrer avec un throttle bas (IBUS≈1000) pour éviter un démarrage des moteurs.
- `MAXBRIDE` limite la puissance max envoyée aux ESC.
- Stabilisation (IMU/PID) non encore intégrée dans main.c par défaut; prévoir une étape d’intégration (lecture IMU→filtre→PID→mixage 4 moteurs).

