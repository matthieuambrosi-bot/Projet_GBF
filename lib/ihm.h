#pragma once
#include <M5Unified.h>

// Éléments sélectionnables (mode SELECTION)
// 0..3 : formes d'onde (Carré, Sinus, Triangle, Scie)
// 4 : Fréquence, 5 : Amplitude, 6 : Offset, 7 : Valider
constexpr int IHM_NB_ITEMS = 8;

void ihmInit();                          // affiche le mode 1 (sélection)
void ihmSelect(int index);               // met un élément en surbrillance
void ihmShowOutput(int wave, bool on);   // affiche le mode 2 (résumé + ON/OFF)
void ihmUpdate();                        // à appeler dans loop() : gère les boutons

// Accès aux paramètres choisis (pour le futur gbf.cpp)
int   ihmGetWave();       // 0 = Carré, 1 = Sinus, 2 = Triangle, 3 = Scie
float ihmGetFreq();       // Hz
float ihmGetAmp();        // V
float ihmGetOffset();     // V
bool  ihmIsOutputOn();    // true si la sortie est active
