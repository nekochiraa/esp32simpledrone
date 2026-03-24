# Guide de mise en vol — ESP32 Simple Drone

## 1. Câblage — résoudre le conflit GPIO 20/21

Les ESC arrière et le MPU6050 utilisent les mêmes broches (GPIO 20/21).
Change les GPIOs dans `main/motor.c` ou `main/stabilization.c` selon ton câblage réel.

## 2. Régler les gains PID

Dans `main/flight_controller.c`, les gains par défaut (Kp=1) sont des valeurs de départ.

Procédure :
- Attache le drone au sol (ou tiens-le à la main)
- Augmente `ROLL_KP` et `PITCH_KP` jusqu'à sentir des oscillations
- Réduis de ~30 %, puis ajoute un peu de `Kd`

## 3. Vérifier le sens des moteurs

Le mixage suppose une configuration en X :

```
FL (CCW)    FR (CW)
    \      /
     \    /
     /    \
    /      \
BL (CW)    BR (CCW)
```

Si un moteur tourne dans le mauvais sens → inverse 2 fils sur l'ESC.

## 4. Vérifier le sens des corrections PID

Si le drone s'emballe au lieu de se stabiliser, c'est que le signe de correction est inversé.
Dans ce cas, inverse le signe dans le mixage (`+roll` ↔ `-roll` ou `+pitch` ↔ `-pitch`)
dans `main/flight_controller.c`.

## 5. Séquence d'arming ESC

Vérifie que tes ESC arment correctement avec la séquence actuelle (5 % → 10 %).
Certains ESC ont besoin d'une séquence différente — consulte la doc de tes ESC.

## 6. Premier vol

1. Throttle à 0, armer avec CH5
2. Monter les gaz **très doucement**
3. Si ça oscille → baisser Kp
4. Si c'est mou / instable → augmenter Kp

Bon vol ! 🚁
