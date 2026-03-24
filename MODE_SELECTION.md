# 🎮 Guide de Sélection des Modes

Ton projet est maintenant organisé avec 2 modes que tu peux facilement switcher !

## 📂 Structure

```
main.c 
  ├─ app_main()                    ← Point d'entrée
  ├─ run_drone_main()              ← Mode VOL (avec télécommande)
  └─ run_stabilization_tests()     ← Mode TEST (tests BMI270)
```

## 🔧 Comment changer de mode

### Ouvre `main/main.c` et cherche la fonction `app_main()` :

```c
void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 🎯 CHOISIS TON MODE ICI:
    
    // Mode 1: Lancer le drone avec télécommande (vol normal)
    run_drone_main();
    
    // Mode 2: Lancer les tests de stabilisation
    // run_stabilization_tests();
}
```

### Pour passer en MODE TEST :

```c
void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Mode 1: Lancer le drone avec télécommande (vol normal)
    // run_drone_main();                     ← Commente cette ligne
    
    // Mode 2: Lancer les tests de stabilisation
    run_stabilization_tests();              ← Décommente cette ligne
}
```

### Pour revenir en MODE VOL :

```c
void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Mode 1: Lancer le drone avec télécommande (vol normal)
    run_drone_main();                       ← Décommente cette ligne
    
    // Mode 2: Lancer les tests de stabilisation
    // run_stabilization_tests();            ← Commente cette ligne
}
```

## 🚀 Compilation

```bash
cd /home/nekochira/code/esp32simpledrone
idf.py build
idf.py flash monitor
```

## 📋 Détails des modes

### Mode 1: `run_drone_main()`
- ✅ Contrôle par télécommande iBUS
- ✅ PID Roll/Pitch/Yaw
- ✅ Failsafe automatique
- ✅ Contrôle 4 moteurs
- ✅ Boucle 100Hz

**Utilise quand**: Tu veux faire voler le drone

### Mode 2: `run_stabilization_tests()`
- ✅ Test initialisation BMI270
- ✅ Lecture capteurs temps réel
- ✅ Test filtre Kalman
- ✅ Test filtre complémentaire
- ✅ Test PID
- ✅ Test stabilisation complète
- ✅ Test réponse perturbation

**Utilise quand**: Tu veux vérifier que le BMI270 fonctionne bien

## 💡 Astuce

Tu peux aussi créer des defines pour switcher encore plus vite :

```c
// En haut de main.c
#define MODE_VOL    1
#define MODE_TEST   0

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    #if MODE_VOL
        run_drone_main();
    #elif MODE_TEST
        run_stabilization_tests();
    #endif
}
```

Puis tu changes juste les valeurs 0/1 !

## 🎯 Ce qui est commun (dans app_main)

- Initialisation event loop ESP-IDF
- Affichage du menu de sélection

Tout le reste est isolé dans chaque fonction !
