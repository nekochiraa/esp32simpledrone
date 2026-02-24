# Documentation technique — ESP32 Simple Drone

## Table des matières

1. [Architecture générale](#architecture-générale)
2. [Fichiers sources](#fichiers-sources)
3. [Module PID (`pid.c` / `pid.h`)](#module-pid)
4. [Module moteur (`motor.c` / `motor.h`)](#module-moteur)
5. [Module stabilisation (`stabilization.c` / `stabilization.h`)](#module-stabilisation)
6. [Contrôleur de vol (`flight_controller.c` / `flight_controller.h`)](#contrôleur-de-vol)
7. [Récepteur iBUS (`controller.c` / `controller.h`)](#récepteur-ibus)
8. [Programme principal (`main.c`)](#programme-principal)
9. [Cartographie des broches](#cartographie-des-broches)
10. [Réglage des PID](#réglage-des-pid)

---

## Architecture générale

```
 Récepteur RC (iBUS)          IMU (MPU6050)
       │                           │
       │  UART1                    │  I2C0
       ▼                           ▼
 ┌──────────┐              ┌──────────────┐
 │ channel  │              │ stabilization│
 │ handler  │              │ (accel+gyro  │
 │          │              │  + filtre)   │
 └────┬─────┘              └──────┬───────┘
      │ consignes                 │ mesures
      │ (roll,pitch,yaw,throttle) │ (roll,pitch)
      ▼                           ▼
 ┌─────────────────────────────────────┐
 │         flight_controller           │
 │  ┌─────┐  ┌─────┐  ┌─────┐         │
 │  │ PID │  │ PID │  │ PID │         │
 │  │Roll │  │Pitch│  │ Yaw │         │
 │  └──┬──┘  └──┬──┘  └──┬──┘         │
 │     └────┬────┘────────┘            │
 │          ▼  mixage quadricopter     │
 └──────────┬──────────────────────────┘
            │ FR, FL, BR, BL (0-100 %)
            ▼
 ┌─────────────────┐
 │   motor (MCPWM) │
 │  4 × ESC 50 Hz  │
 └─────────────────┘
```

Le système fonctionne en boucle fermée à ~100 Hz (10 ms par itération).

---

## Fichiers sources

| Fichier                | Rôle                                          |
|------------------------|-----------------------------------------------|
| `main.c`               | Point d'entrée, boucle de contrôle principale |
| `pid.c` / `pid.h`      | Contrôleur PID générique réutilisable         |
| `flight_controller.c/h`| Mixage quadricopter + gestion armement        |
| `motor.c` / `motor.h`  | Pilotage ESC via MCPWM                        |
| `stabilization.c/h`    | Lecture MPU6050, filtrage, calibration         |
| `controller.c/h`       | Exemple de lecture iBUS (debug)               |
| `main.h`               | Includes communs (non utilisé directement)    |

---

## Module PID

**Fichiers :** `pid.h`, `pid.c`

### Structure `pid_controller_t`

```c
typedef struct {
    float kp;              // gain proportionnel
    float ki;              // gain intégral
    float kd;              // gain dérivé
    float integral;        // accumulation de l'erreur × dt
    float prev_error;      // erreur au pas précédent
    float integral_limit;  // anti-windup (borne |integral|)
    float output_limit;    // borne |sortie|
} pid_controller_t;
```

### API

| Fonction | Description |
|----------|-------------|
| `pid_init(pid, kp, ki, kd, i_lim, o_lim)` | Initialise les gains et remet à zéro |
| `pid_reset(pid)` | Remet intégrale et erreur précédente à 0 |
| `pid_compute(pid, setpoint, measure, dt)` | Calcule la correction PID |

### Formule

```
erreur     = consigne − mesure
P          = Kp × erreur
I         += erreur × dt           (borné à ±integral_limit)
D          = Kd × (erreur − erreur_précédente) / dt
sortie     = P + I + D             (borné à ±output_limit)
```

L'anti-windup empêche le terme intégral de croître indéfiniment quand l'erreur est saturée (ex: drone au sol avec consigne de roll élevée).

---

## Module moteur

**Fichiers :** `motor.h`, `motor.c`

### API

| Fonction | Description |
|----------|-------------|
| `pwminit(void)` | Configure MCPWM 50 Hz et arme les ESC |
| `frontright(float percent)` | Commande moteur avant droit (0-100 %) |
| `frontleft(float percent)` | Commande moteur avant gauche |
| `backright(float percent)` | Commande moteur arrière droit |
| `backleft(float percent)` | Commande moteur arrière gauche |

### Conversion pourcentage → duty cycle

```
duty = percent / 20 + 5
```

- 0 % → duty 5 % → ~1.0 ms (moteur arrêté)
- 100 % → duty 10 % → ~2.0 ms (plein gaz)

### Séquence d'armement

1. Attente de 5 s pour la stabilisation alimentation
2. Envoi de 5 % duty (1 ms) pendant 2 s
3. Envoi de 10 % duty (2 ms) pendant 2 s
4. ESC prêts

---

## Module stabilisation

**Fichiers :** `stabilization.h`, `stabilization.c`

### Capteur : MPU6050 (I2C)

- Accéléromètre : ±2 g, sensibilité 16384 LSB/g
- Gyroscope : ±250 °/s, sensibilité 131 LSB/(°/s)

### API

| Fonction | Description |
|----------|-------------|
| `stabinit(float *offset)` | Init I2C + MPU6050 + calibration gyro |
| `printaccel(data, xyz)` | Lit l'accéléromètre, calcule roll/pitch en degrés |
| `printgyro(data, xyz, dt, offset)` | Intègre le gyroscope sur dt |
| `complementaryFilter(gyro, accel)` | Fusion : 0.996 × gyro + 0.004 × accel |
| `pidcontroll(mesure, consigne, dt, pid)` | PID legacy (remplacé par `pid.c`) |

### Filtre complémentaire

Le filtre complémentaire fusionne les données accéléromètre (fiable à basse fréquence mais bruité) et gyroscope (fiable à haute fréquence mais dérive). Le coefficient 0.996 donne un poids élevé au gyroscope pour des réponses rapides.

### Calibration gyroscope

Au démarrage, `stabinit()` prend 1000 mesures gyroscope avec le drone immobile et calcule la moyenne. Cette moyenne (offset) est soustraite à chaque lecture pour compenser le biais de repos.

---

## Contrôleur de vol

**Fichiers :** `flight_controller.h`, `flight_controller.c`

### Mixage quadricopter (configuration X)

```
   FL (CCW)    FR (CW)
       \      /
        \    /
         \/
         /\
        /  \
       /    \
   BL (CW)    BR (CCW)
```

| Moteur | Formule |
|--------|---------|
| Front Right | throttle − roll − pitch − yaw |
| Front Left  | throttle + roll − pitch + yaw |
| Back Right  | throttle − roll + pitch + yaw |
| Back Left   | throttle + roll + pitch − yaw |

Les corrections roll/pitch/yaw sont ajoutées ou soustraites au throttle de base. Chaque sortie est bornée à [0, 100] %.

### Armement

Le contrôleur ne commande les moteurs que si :
1. `fc.armed == true` (CH5 iBUS > 1500)
2. `throttle > 5 %` (sécurité anti-démarrage accidentel)

Si l'une des conditions est fausse, tous les moteurs sont à 0 % et les PID sont réinitialisés.

### API

| Fonction | Description |
|----------|-------------|
| `flight_ctrl_init(fc)` | Initialise les 3 PID avec les gains par défaut |
| `ibus_to_angle(raw, max)` | Convertit iBUS [1000..2000] en [-max..+max] |
| `ibus_to_percent(raw)` | Convertit iBUS [1000..2000] en [0..100] % |
| `flight_ctrl_update(...)` | Calcule PID + mixage → 4 sorties moteur |

---

## Récepteur iBUS

**Fichiers :** `controller.h`, `controller.c`

Module utilitaire qui affiche les 14 canaux iBUS en mode debug. Utile pour vérifier le câblage avant intégration complète.

### Canaux iBUS standard

| Canal | Fonction | Plage |
|-------|----------|-------|
| CH1   | Roll (aileron) | 1000-2000, centre 1500 |
| CH2   | Pitch (profondeur) | 1000-2000, centre 1500 |
| CH3   | Throttle (gaz) | 1000-2000 |
| CH4   | Yaw (lacet) | 1000-2000, centre 1500 |
| CH5   | Armement (interrupteur) | 1000/2000 |

---

## Programme principal

**Fichier :** `main.c`

### Boucle de contrôle (`app_main`)

1. Initialisation de la stabilisation (I2C + MPU6050 + calibration)
2. Initialisation de l'iBUS (UART1)
3. Armement des ESC (MCPWM)
4. Initialisation du contrôleur de vol (3 PID)
5. **Boucle à 100 Hz :**
   - Calcul du `dt` réel via `esp_timer_get_time()`
   - Lecture accéléromètre → roll/pitch
   - Intégration gyroscope → roll/pitch
   - Fusion complémentaire
   - Lecture des consignes iBUS (throttle, roll, pitch, yaw)
   - Vérification armement (CH5)
   - Calcul PID + mixage quadricopter
   - Commande des 4 moteurs

---

## Cartographie des broches

| Périphérique | Broche | GPIO |
|-------------|--------|------|
| ESC Front Right | MCPWM0A | GPIO 17 |
| ESC Front Left  | MCPWM0B | GPIO 19 |
| ESC Back Right  | MCPWM1A | GPIO 20 |
| ESC Back Left   | MCPWM1B | GPIO 21 |
| iBUS RX | UART1 RX | GPIO 5 |
| iBUS TX | UART1 TX | GPIO 4 |
| MPU6050 SCL | I2C0 SCL | GPIO 20 |
| MPU6050 SDA | I2C0 SDA | GPIO 21 |

> ⚠️ **Conflit de broches** : GPIO 20 et 21 sont utilisés à la fois pour les ESC arrière et l'I2C du MPU6050. Modifiez les GPIOs ESC ou I2C selon votre câblage réel.

---

## Réglage des PID

### Méthode recommandée

1. **Commencer avec Kp seul** (Ki=0, Kd=0)
   - Augmenter Kp jusqu'à ce que le drone oscille légèrement
   - Réduire Kp de ~30 %

2. **Ajouter Kd**
   - Augmenter Kd pour amortir les oscillations
   - Trop de Kd → vibrations haute fréquence

3. **Ajouter Ki** (optionnel)
   - Ki corrige les erreurs statiques (dérive lente)
   - Commencer très petit (0.01) et augmenter doucement

### Gains par défaut

| Axe   | Kp  | Ki  | Kd  |
|-------|-----|-----|-----|
| Roll  | 1.0 | 0.0 | 0.0 |
| Pitch | 1.0 | 0.0 | 0.0 |
| Yaw   | 2.0 | 0.0 | 0.0 |

Ces gains sont des **valeurs de départ** conservatrices. Chaque châssis/moteur/hélice nécessite un réglage spécifique.

### Limites de sécurité

| Paramètre | Valeur | Rôle |
|-----------|--------|------|
| `PID_INTEGRAL_LIMIT` | 30.0 | Anti-windup : borne l'intégrale |
| `PID_OUTPUT_LIMIT` | 40.0 | Correction max par axe (% moteur) |
| `MAX_THROTTLE` | 100.0 | Puissance max envoyée aux ESC |
| `THROTTLE_MIN_PCT` | 5.0 | En dessous, PID désactivé |
