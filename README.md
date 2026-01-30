# ESP32 Simple Drone

Projet de contrôle de drone basé sur ESP32 avec support IBUS et contrôle ESC.

## Cibles Supportées

| ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-S2 | ESP32-S3 |
| ----- | -------- | -------- | -------- | -------- | -------- | -------- |

## Aperçu du Projet

Ce projet implémente un contrôleur de drone simple utilisant:
- **ESP32** comme microcontrôleur principal
- **Protocole IBUS** pour la réception des commandes radio
- **MCPWM** pour le contrôle des ESC (Electronic Speed Controllers)
- **4 moteurs** en configuration quadricoptère

## Architecture

### Entrées
- **IBUS** (récepteur radio) sur UART1
  - RX: GPIO 5
  - TX: GPIO 4

### Sorties
- **4 ESC** via MCPWM (50 Hz):
  - Moteur avant droit (Front Right): GPIO 17 (MCPWM0A)
  - Moteur avant gauche (Front Left): GPIO 19 (MCPWM0B)
  - Moteur arrière droit (Back Right): GPIO 20 (MCPWM1A)
  - Moteur arrière gauche (Back Left): GPIO 21 (MCPWM1B)

## Structure du Code (dossier `main/`)

### 📄 `main.c`

Fichier principal contenant la fonction `app_main()`.

**Responsabilités:**
- Initialise l'event loop ESP-IDF
- Configure la communication IBUS (UART1)
- Initialise les moteurs via `pwminit()`
- Boucle principale de lecture des commandes IBUS

**Fonctionnement:**
1. Configure le handler IBUS pour recevoir les valeurs des canaux
2. Lit le canal 3 (index 2) qui correspond au throttle
3. Convertit la valeur IBUS en pourcentage pour l'ESC:
   - Formule: `(valeur - 1000) / 10`
   - IBUS 1000 → 0% (moteur arrêté)
   - IBUS 1500 → 50% (mi-puissance)
   - IBUS 2000 → 100% (pleine puissance)
4. Applique la commande au moteur avant droit

**Note:** Par défaut, seul le moteur avant droit est contrôlé. Pour un vol réel, il faudrait implémenter le mixage des 4 moteurs avec stabilisation.

### 📄 `motor.h` / `motor.c`

Module de contrôle des ESC via MCPWM.

**API Publique:**

```c
void pwminit(void);                  // Initialise MCPWM et effectue l'arming des ESC
void frontright(float percent);      // Contrôle moteur avant droit (0-100%)
void frontleft(float percent);       // Contrôle moteur avant gauche (0-100%)
void backright(float percent);       // Contrôle moteur arrière droit (0-100%)
void backleft(float percent);        // Contrôle moteur arrière gauche (0-100%)
```

**Détails Techniques:**

- **Fréquence PWM:** 50 Hz (standard ESC)
- **Plage de duty cycle:** 5% à 10%
  - 5% ≈ 1 ms → vitesse minimale
  - 10% ≈ 2 ms → vitesse maximale
- **Conversion:** `duty = percent/20 + 5`
- **Protection:** `MAXBRIDE` limite la puissance maximale à 100%

**Séquence d'Arming:**
1. Attente de 5 secondes au démarrage
2. Envoi de 5% duty pendant 2 secondes
3. Envoi de 10% duty pendant 2 secondes
4. ESC prêts à recevoir des commandes

### 📄 `idf_component.yml`

Configuration des dépendances du projet.

**Dépendances:**
- `zorxx__ibus`: Librairie de gestion du protocole IBUS

## Compilation et Flash

### Prérequis

- ESP-IDF v5.0 ou supérieur
- Python 3.8+
- Câble USB pour programmer l'ESP32

### Commandes

```bash
# Configuration du projet
idf.py menuconfig

# Compilation
idf.py build

# Flash sur l'ESP32
idf.py -p /dev/ttyUSB0 flash

# Moniteur série
idf.py -p /dev/ttyUSB0 monitor

# Flash + Monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Câblage Matériel

### Récepteur IBUS
- **RX** → GPIO 5
- **TX** → GPIO 4
- **VCC** → 3.3V ou 5V (selon le récepteur)
- **GND** → GND

### ESC
Connecter le signal PWM de chaque ESC au GPIO correspondant:
- ESC avant droit → GPIO 17
- ESC avant gauche → GPIO 19
- ESC arrière droit → GPIO 20
- ESC arrière gauche → GPIO 21

**⚠️ Attention:** Alimenter les ESC avec une batterie appropriée (2S-4S LiPo selon les moteurs). Ne **JAMAIS** alimenter les moteurs depuis l'ESP32.

## Sécurité

### Points Critiques

1. **Toujours démarrer avec throttle bas** (IBUS ≈ 1000) pour éviter un démarrage intempestif des moteurs
2. **`MAXBRIDE = 100%`** limite la puissance maximale envoyée aux ESC
3. **Tester sans hélices** lors des premiers essais
4. **Débrancher la batterie** avant toute manipulation du câblage

### Limites Actuelles

- ❌ Pas de stabilisation IMU implémentée
- ❌ Pas de contrôle PID intégré
- ❌ Un seul moteur contrôlé par défaut
- ❌ Pas de détection de perte de signal radio
- ❌ Pas de mode failsafe

## Développement Futur

Pour transformer ce projet en un drone fonctionnel, il faudrait ajouter:

1. **Capteur IMU** (ex: MPU6050)
   - Lecture de l'accéléromètre et gyroscope
   - Calcul des angles (roll, pitch, yaw)
   
2. **Stabilisation PID**
   - Contrôleurs PID pour roll/pitch/yaw
   - Filtres complémentaires pour fusion des données

3. **Mixage des moteurs**
   - Distribution des commandes sur les 4 moteurs
   - Gestion des modes de vol (acro, angle, etc.)

4. **Sécurité avancée**
   - Détection de perte de signal
   - Mode failsafe (atterrissage automatique)
   - Alarme batterie faible

## Troubleshooting

### Échec du téléversement
- Vérifier la connexion USB
- Maintenir le bouton BOOT pendant le flash
- Réduire le baud rate dans menuconfig

### Moteurs ne répondent pas
- Vérifier la séquence d'arming des ESC
- S'assurer que le throttle IBUS est autour de 1000 au démarrage
- Vérifier le câblage des signaux PWM

### Pas de signal IBUS
- Vérifier les connexions RX/TX
- Vérifier que le récepteur est bien bindé à l'émetteur
- Utiliser `idf.py monitor` pour voir les logs

## Ressources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [MCPWM Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/mcpwm.html)
- [IBUS Protocol](https://github.com/zorxx/ibus)

## Licence

Ce projet est fourni tel quel, sans garantie. Utilisation à vos risques et périls.

**⚠️ AVERTISSEMENT:** Les drones peuvent être dangereux. Respectez les lois locales et prenez toutes les précautions de sécurité nécessaires.
