# ESP32 Simple Drone

Contrôleur de vol pour quadricopter basé sur un ESP32, utilisant ESP-IDF.

## Fonctionnalités

- **Stabilisation PID** sur 3 axes (roll, pitch, yaw)
- **Lecture IMU** MPU6050 (accéléromètre + gyroscope) via I2C
- **Réception iBUS** depuis un récepteur FlySky via UART
- **Pilotage de 4 ESC** via MCPWM (signal servo 50 Hz)
- **Filtre complémentaire** pour la fusion accéléromètre/gyroscope
- **Anti-windup** sur les PID et sécurités d'armement

## Matériel requis

- ESP32 (ou variante compatible)
- MPU6050 (IMU accéléromètre + gyroscope)
- 4 × ESC + moteurs brushless
- Récepteur iBUS (FlySky FS-iA6B ou similaire)
- Émetteur RC (FlySky FS-i6 ou similaire)

## Branchements par défaut

| Composant         | GPIO |
|-------------------|------|
| ESC Avant Droit   | 17   |
| ESC Avant Gauche  | 19   |
| ESC Arrière Droit | 20   |
| ESC Arrière Gauche| 21   |
| iBUS RX           | 5    |
| iBUS TX           | 4    |
| MPU6050 SCL       | 20   |
| MPU6050 SDA       | 21   |

> ⚠️ GPIO 20/21 sont partagés entre ESC arrière et I2C. Adaptez les broches à votre câblage.

## Compilation et flash

```bash
# Configurer l'environnement ESP-IDF
. $IDF_PATH/export.sh

# Compiler
idf.py build

# Flasher (remplacer PORT par votre port série)
idf.py -p PORT flash

# Moniteur série
idf.py -p PORT monitor
```

## Structure du projet

```
esp32simpledrone/
├── main/
│   ├── main.c                  # Boucle de contrôle principale
│   ├── pid.c / pid.h           # Contrôleur PID générique
│   ├── flight_controller.c/h   # Mixage quadricopter + armement
│   ├── motor.c / motor.h       # Pilotage ESC (MCPWM)
│   ├── stabilization.c/h       # IMU MPU6050 + filtrage
│   ├── controller.c/h          # Utilitaire debug iBUS
│   └── CMakeLists.txt
├── DOCUMENTATION.md            # Documentation technique détaillée
└── README.md
```

## Architecture

```
Récepteur RC (iBUS) ──► Consignes (roll, pitch, yaw, throttle)
                                │
MPU6050 (I2C) ──► Filtre ──► Mesures (roll, pitch)
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

## Réglage des PID

Les gains par défaut sont conservateurs (Kp=1, Ki=0, Kd=0). Modifiez-les dans `flight_controller.c` :

1. Augmentez `Kp` jusqu'à observer de légères oscillations, puis réduisez de 30 %
2. Ajoutez `Kd` pour amortir les oscillations
3. Ajoutez `Ki` (petit, ~0.01) pour corriger les erreurs statiques

Voir [`DOCUMENTATION.md`](DOCUMENTATION.md) pour les détails complets.

## Sécurité

- **Armement** : les moteurs ne tournent que si CH5 > 1500 ET throttle > 5 %
- **Anti-windup** : le terme intégral PID est borné
- **Saturation** : chaque sortie moteur est bornée à [0, 100] %

## Dépendances

- [ESP-IDF](https://github.com/espressif/esp-idf) >= 4.1.0
- [zorxx/ibus](https://components.espressif.com/components/zorxx/ibus) ^1.0.0

## Licence

MIT
