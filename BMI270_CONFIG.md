# Configuration BMI270 - Paramètres Optimaux pour Précision Maximale

## 📊 Paramètres Capteur

### Accéléromètre
- **Plage**: ±2g (meilleure résolution possible)
- **Sensibilité**: 16384 LSB/g
- **Fréquence d'échantillonnage (ODR)**: 200Hz
- **Mode de filtrage**: Normal averaging mode
- **Résolution**: ~0.00006g par LSB

### Gyroscope
- **Plage**: ±250°/s (meilleure précision angulaire)
- **Sensibilité**: 131.072 LSB/(°/s)
- **Fréquence d'échantillonnage (ODR)**: 200Hz
- **Mode de filtrage**: Normal filter
- **Résolution**: ~0.0076°/s par LSB

## 🎯 Pourquoi ces paramètres ?

### ±2g pour l'accéléromètre
- Drones légers = faibles accélérations (rarement >2g en vol normal)
- Résolution 8x meilleure que ±16g
- Bruit réduit par la plage étroite
- Parfait pour mesurer l'orientation horizontale

### ±250°/s pour le gyroscope
- Vitesses angulaires typiques d'un drone: 50-150°/s
- Résolution 8x meilleure que ±2000°/s
- Drift réduit grâce à la plage étroite
- Suffisant même pour manœuvres agressives

### 200Hz ODR
- Compromis optimal précision/performance
- Nyquist: peut capturer mouvements jusqu'à 100Hz
- Pas de surcharge CPU
- Synchronisation parfaite avec boucle de contrôle 100Hz

## 🔧 Filtres Optimisés

### Filtre de Kalman
```c
Q = 0.003  // Confiance élevée dans le gyro BMI270 (faible drift)
R = 0.030  // Confiance modérée dans l'accel (moins de bruit)
```

**Explications**:
- Q réduit car BMI270 a un excellent gyroscope (faible drift)
- R ajusté pour le bruit réduit de l'accel ±2g
- Convergence rapide et estimation stable

### Filtre Complémentaire
```c
filtered = 0.98 * gyro + 0.02 * accel
```

**Explications**:
- 98% gyro: exploite la haute précision du BMI270
- 2% accel: correction lente du drift
- Meilleur que 99.6/0.4 car l'accel est plus précis en ±2g

## 📈 Calibration

### Gyroscope
- **Durée**: 2000 échantillons (~2 secondes à 200Hz)
- **Conditions**: Drone parfaitement immobile
- **Objectif**: Mesurer le biais/offset statique
- **Précision attendue**: <0.01°/s de drift

### Accéléromètre
- Pas de calibration automatique nécessaire
- Le calcul d'angle Roll/Pitch par atan2 est auto-normalisé
- Pour calibration avancée: mesurer sur 6 faces (optionnel)

## 🎮 Gains PID Recommandés

Pour un drone léger (~250g) avec BMI270:

### Roll & Pitch
```c
Kp = 1.2 - 1.5  // Réponse rapide grâce à la faible latence
Ki = 0.05 - 0.1 // Anti-windup réduit (peu de drift)
Kd = 0.05 - 0.08 // Amortissement efficace
```

### Yaw
```c
Kp = 1.5 - 2.0  // Plus agressif (pas de couplage gyro/accel)
Ki = 0.1 - 0.15
Kd = 0.0        // Souvent inutile sur yaw
```

## ⚡ Performance Attendue

### Latence totale
- Lecture I2C: ~0.5ms
- Calcul filtre Kalman: ~0.1ms
- Calcul PID: ~0.1ms
- **Total: ~0.7ms** ✓ Excellent pour contrôle en temps réel

### Précision angulaire
- **Statique**: ±0.1° (excellente stabilité)
- **Dynamique**: ±0.5° (en mouvement)
- **Drift**: <0.5°/minute (avec Kalman)

### Stabilité
- Temps de réponse: ~50-100ms
- Dépassement: <5%
- Erreur statique: <0.2°

## 🔍 Diagnostic et Validation

### Vérifier la précision
```c
// En vol stationnaire
if (fabs(roll) > 0.5° || fabs(pitch) > 0.5°) {
    // Recalibrer ou vérifier montage capteur
}
```

### Vérifier le bruit
```c
// Échantillonner 1000 points statiques
float std_dev_gyro = ...;  // Devrait être <0.02°/s
float std_dev_accel = ...; // Devrait être <0.01g
```

### Vérifier le drift
```c
// Laisser tourner 5 minutes sans bouger
float total_drift = final_angle - initial_angle;
// Devrait être <2.5° sur 5 minutes
```

## 🚀 Tests Recommandés

1. **Test statique**: Drone immobile, vérifier angles stables
2. **Test dynamique**: Incliner manuellement, vérifier suivi
3. **Test de choc**: Léger coup, vérifier récupération rapide
4. **Test de drift**: 5 min statique, mesurer dérive
5. **Test en vol**: Hovering, observer stabilité

## 📚 Avantages du BMI270 vs MPU6050

| Caractéristique | MPU6050 | BMI270 |
|----------------|---------|--------|
| Gyro noise | 0.01°/s/√Hz | 0.007°/s/√Hz |
| Accel noise | 400 µg/√Hz | 180 µg/√Hz |
| Drift gyro | 20°/h | 10°/h |
| Consommation | 3.9mA | 0.68mA |
| Latence | 2-4ms | 0.5-1ms |

**Résultat**: Le BMI270 est ~2x plus précis et 5x plus économe en énergie!

## ⚙️ Registres I2C Utilisés

```
BMI270_ADDR = 0x68
BMI270_CHIP_ID = 0x00 (lecture: 0x24)
BMI270_ACC_DATA = 0x0C (6 bytes, little-endian)
BMI270_GYR_DATA = 0x12 (6 bytes, little-endian)
BMI270_ACC_CONF = 0x40 (ODR + filter)
BMI270_ACC_RANGE = 0x41 (plage)
BMI270_GYR_CONF = 0x42 (ODR + filter)
BMI270_GYR_RANGE = 0x43 (plage)
BMI270_PWR_CONF = 0x7C (mode power)
BMI270_PWR_CTRL = 0x7D (enable sensors)
BMI270_CMD = 0x7E (commandes)
```

## 🎯 Conclusion

Cette configuration offre le meilleur compromis:
- ✅ Précision maximale pour la stabilisation
- ✅ Latence minimale pour réactivité
- ✅ Consommation optimisée
- ✅ Plages adaptées au vol de drone

**Le BMI270 configuré ainsi dépasse largement le MPU6050!**
