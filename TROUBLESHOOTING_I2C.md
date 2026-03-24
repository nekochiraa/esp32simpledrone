# 🔍 Guide de Diagnostic I2C - BMI270

## ⚠️ Erreur "NACK detected" - Que faire ?

Cette erreur signifie que **le BMI270 ne répond pas** sur le bus I2C.

### ✅ Checklist de Diagnostic

#### 1. Vérifier les Connexions Physiques

```
BMI270          ESP32
──────────────────────
VCC      →      3.3V  ✓ PAS 5V!
GND      →      GND
SDA      →      GPIO 21
SCL      →      GPIO 20
SDO      →      GND (pour adresse 0x68)
```

**Test**: Vérifier chaque fil avec un multimètre en mode continuité

#### 2. Vérifier l'Adresse I2C

Le code inclut maintenant un **scanner I2C automatique** qui s'exécute en cas d'erreur.

**Adresses possibles**:
- `0x68` si SDO → GND (par défaut)
- `0x69` si SDO → VCC

**Si le scan trouve le BMI270 à 0x69**: Modifier dans `stabilization.c`:
```c
#define BMI270_ADDR  0x69  // au lieu de 0x68
```

#### 3. Vérifier l'Alimentation

```bash
# Avec multimètre:
- VCC = 3.3V (±0.3V)
- Courant: ~5mA au repos
```

**Si tension incorrecte**:
- Vérifier régulateur 3.3V de l'ESP32
- Module BMI270 a-t-il son propre régulateur?

#### 4. Pull-ups Résistances

Les pull-ups sont activés en interne par le code:
```c
.flags.enable_internal_pullup = true
```

**Si problème persiste**, ajouter pull-ups externes:
- 4.7kΩ entre SDA et 3.3V
- 4.7kΩ entre SCL et 3.3V

#### 5. Vérifier le Module BMI270

**Module cassé ou variante?**
- Certains modules utilisent BMI160 ou MPU6050
- Vérifier l'inscription sur la puce
- Essayer avec adresse 0x68 ET 0x69

#### 6. Test avec Scanner I2C Simple

Le code exécute automatiquement un scan si le BMI270 n'est pas trouvé.

**Résultats possibles**:

```
Cas 1: "Device trouvé à l'adresse 0x68"
  → BMI270 présent, bon câblage
  → Problème: mauvais CHIP_ID ou initialisation

Cas 2: "Device trouvé à l'adresse 0x69"  
  → SDO est à VCC au lieu de GND
  → Modifier BMI270_ADDR à 0x69

Cas 3: "Device trouvé à l'adresse 0x??"
  → Autre capteur I2C sur le bus
  → Vérifier le module

Cas 4: "Aucun device I2C trouvé"
  → Problème de câblage
  → Vérifier SDA/SCL, alimentations, pull-ups
```

## 🛠️ Solutions par Type d'Erreur

### Erreur: "NACK detected"
```
Cause: Device ne répond pas
Solutions:
  1. Vérifier connexions SDA/SCL
  2. Vérifier alimentation 3.3V
  3. Tester adresse 0x69 si SDO≠GND
  4. Ajouter pull-ups externes
```

### Erreur: "CHIP_ID incorrect"
```
Cause: Mauvais capteur ou variante
Solutions:
  1. Vérifier inscription sur puce
  2. Tester avec code MPU6050 si c'est un MPU
  3. Contacter vendeur pour datasheet
```

### Erreur: "Bus déjà acquis"
```
Cause: Double initialisation I2C
Solutions:
  → Déjà corrigé dans le code (flag i2c_initialized)
```

### Erreur: "Timeout I2C"
```
Cause: Communication trop lente ou interrompue
Solutions:
  1. Réduire fréquence: 100kHz au lieu de 400kHz
  2. Câbles plus courts (<10cm)
  3. Ajouter condensateur 100nF sur VCC
```

## 🔧 Modification de l'Adresse

Si le scan trouve le BMI270 à **0x69** au lieu de 0x68:

```c
// Dans main/stabilization.c ligne ~9
#define BMI270_ADDR  0x69  // Changer de 0x68 à 0x69
```

Puis recompiler:
```bash
cd /home/nekochira/code/esp32simpledrone
idf.py build flash monitor
```

## 🧪 Test de Bon Fonctionnement

Après correction, tu devrais voir:
```
I (xxx) BMI270: I2C initialisé: bus=0, addr=0x68, freq=400000Hz
I (xxx) BMI270: Tentative lecture CHIP_ID à l'adresse 0x68...
I (xxx) BMI270: CHIP_ID = 0x24 (attendu 0x24)
I (xxx) BMI270: BMI270 initialisé avec succès
I (xxx) BMI270: Config: Accel=±2g@200Hz, Gyro=±250°/s@200Hz
I (xxx) BMI270: Calibration gyroscope (gardez le drone immobile)...
I (xxx) BMI270: Progression: 0%
...
I (xxx) BMI270: Calibration terminée - Offsets: X=xx.xx Y=xx.xx Z=xx.xx
```

## 📞 Debug Avancé

### Activer logs I2C détaillés

Dans `sdkconfig` ou via menuconfig:
```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
CONFIG_I2C_LOG_LEVEL_DEBUG=y
```

### Mesurer avec oscilloscope/analyseur logique

- **SDA/SCL**: Doivent avoir transitions propres
- **Fréquence**: 400 kHz (période ~2.5µs)
- **Pull-up**: Vérifier temps de montée <1µs

## 📚 Références

- BMI270 Datasheet: https://www.bosch-sensortec.com/
- ESP32-S3 I2C: ESP-IDF documentation
- Code source: `main/stabilization.c`

---
**Le scanner I2C s'exécute automatiquement en cas d'erreur!**
