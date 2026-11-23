#pragma once
#include <Arduino.h>

// Crée la tâche GBF (sur le core 0) : à appeler une fois dans setup().
void gbfStart();

// Envoie les paramètres au GBF (utilisable depuis n'importe quelle tâche).
//  wave : 0 = Carré, 1 = Sinus, 2 = Triangle, 3 = Scie
//  freq : Hz   (1 à 1500)
//  amp  : V    amplitude crête à crête
//  offs : V    niveau bas du signal
//  on   : true = signal sur GPIO26, false = sortie à 0 V
void gbfSetOutput(int wave, float freq, float amp, float offs, bool on);