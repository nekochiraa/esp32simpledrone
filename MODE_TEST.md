# Mode Test de Stabilisation

## Comment lancer les tests facilement

### Activation du mode test

1. Ouvrez le fichier `main/main.c`
2. Trouvez les lignes au début du fichier (ligne ~13):
```c
// ========================================
// MODE TEST: Décommentez la ligne ci-dessous pour lancer les tests
// au lieu du mode drone normal
// ========================================
// #define MODE_TEST
// ========================================
```

3. **Décommentez** la ligne `#define MODE_TEST`:
```c
#define MODE_TEST
```

4. Compilez et flashez:
```bash
idf.py build flash monitor
```

### Désactivation du mode test (retour au mode drone)

1. **Recommentez** la ligne dans `main/main.c`:
```c
// #define MODE_TEST
```

2. Compilez et flashez à nouveau

## Ce que fait le mode test

Le mode test exécute automatiquement une série de tests sur le système de stabilisation:

1. ✓ **Test 1**: Initialisation du capteur IMU et calibration du gyroscope
2. ✓ **Test 2**: Lecture des capteurs (gyroscope et accéléromètre)
3. ✓ **Test 3**: Test du filtre complémentaire
4. ✓ **Test 4**: Test du filtre de Kalman
5. ✓ **Test 5**: Test du contrôleur PID
6. ✓ **Test 6**: Boucle de stabilisation complète avec capteurs réels

## Avantages

- **Pas besoin de moteurs**: Les tests fonctionnent sans avoir à brancher les ESC/moteurs
- **Pas besoin de radio**: Les tests ne nécessitent pas de récepteur iBUS
- **Ultra rapide**: Il suffit de changer une seule ligne!
- **Pas de risque**: Le drone ne va pas décoller pendant les tests

## Fichiers de test

- `main/test_stabilisation.c` - Tests simples et rapides (utilisé par MODE_TEST)
- `main/test_stabilization.c` - Suite de tests complète et détaillée

## Notes

En mode test, le drone:
- ✓ Initialise l'IMU (MPU6050/BMI270)
- ✓ Calibre le gyroscope
- ✓ Lance tous les tests de stabilisation
- ✓ Affiche les résultats dans le moniteur série
- ✗ N'initialise PAS les moteurs
- ✗ N'initialise PAS le récepteur iBUS
