#include "gbf.h"
#include <math.h>
#include "soc/soc.h"
#include "soc/rtc_io_reg.h"

// ============================================================================
//  PRINCIPE
//   - Un tableau de N échantillons (8 bits) contient 1 période du signal.
//   - Un timer matériel déclenche une interruption à la cadence N x f.
//     L'interruption envoie simplement l'échantillon suivant au DAC.
//     Aucun calcul dans l'interruption.
//   - Le tableau est rempli (une seule fois) quand on active la sortie.
//     Les fonctions mathématiques ne servent qu'à remplir le tableau.
//
//  CONVENTION DES RÉGLAGES (le DAC ne sort que du 0 -> 3.3 V)
//   - amplitude = crête à crête
//   - offset    = niveau bas du signal
//   -> signal compris entre  offset  et  offset + amplitude
//   Réglages par défaut (3.3 V / 0 V) : signal sur toute la plage 0 -> 3.3 V.
//   Si offset + amplitude > 3.3 V, le signal est écrêté.
// ============================================================================

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define CORE_ARDUINO_3
#endif

namespace {

  // ---------- Réglages ----------
  constexpr int      DAC_PIN  = 26;          // DAC2
  constexpr float    VREF     = 3.3f;        // tension du code 255 (à ajuster après mesure au multimètre)

  constexpr int      N_MIN    = 100;         // échantillons par période, à haute fréquence
  constexpr int      N_MAX    = 1000;        // échantillons par période, à basse fréquence
  constexpr float    FS_MAX   = 150000.0f;   // cadence max du DAC = 100 éch x 1500 Hz

  constexpr uint32_t TIMER_HZ = 40000000;    // horloge du timer : 40 MHz (précision ~0.1 %)

  // ---------- Données ----------
  struct Params { int wave; float freq; float amp; float offs; bool on; };

  QueueHandle_t queueParams = nullptr;

  uint8_t           tab[N_MAX];              // 1 période du signal
  volatile int      nbEch = N_MIN;           // nombre d'échantillons utilisés
  volatile int      idx   = 0;               // échantillon en cours de lecture

  hw_timer_t* timer = nullptr;

  // ---------- Interruption timer : envoie l'échantillon suivant au DAC ----------
  void IRAM_ATTR onTimer() {
    uint8_t code = tab[idx];
    SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, code, RTC_IO_PDAC2_DAC_S);
    if (++idx >= nbEch) idx = 0;
  }

  // ---------- Gestion du timer (compatible Arduino-ESP32 2.x et 3.x) ----------
  void timerInitialise() {
#ifdef CORE_ARDUINO_3
    timer = timerBegin(TIMER_HZ);
    timerAttachInterrupt(timer, &onTimer);
    timerStop(timer);
#else
    timer = timerBegin(1, 80000000 / TIMER_HZ, true);   // timer 1, prédiviseur 2 -> 40 MHz
    timerAttachInterrupt(timer, &onTimer, true);
#endif
  }

  void timerDemarre(uint32_t ticks) {
#ifdef CORE_ARDUINO_3
    timerWrite(timer, 0);
    timerAlarm(timer, ticks, true, 0);
    timerStart(timer);
#else
    timerWrite(timer, 0);
    timerAlarmWrite(timer, ticks, true);
    timerAlarmEnable(timer);
#endif
  }

  void timerArrete() {
#ifdef CORE_ARDUINO_3
    timerStop(timer);
#else
    timerAlarmDisable(timer);
#endif
  }

  // ---------- Forme d'onde normalisée : phase 0..1 -> valeur 0..1 ----------
  float forme(int wave, float ph) {
    switch (wave) {
      case 0:  return (ph < 0.5f) ? 1.0f : 0.0f;                     // Carré
      case 1:  return 0.5f + 0.5f * sinf(2.0f * (float)M_PI * ph);   // Sinus
      case 2:  return (ph < 0.5f) ? 2.0f * ph : 2.0f - 2.0f * ph;    // Triangle
      default: return ph;                                            // Scie
    }
  }

  // ---------- Remplit le tableau, renvoie le nombre d'échantillons ----------
  int remplirTableau(const Params& p) {
    // Plus la fréquence est basse, plus on utilise d'échantillons (100 à 1000)
    // pour que les marches restent peu visibles.
    int n = (int)lroundf(FS_MAX / p.freq);
    n = constrain(n, N_MIN, N_MAX);

    for (int i = 0; i < n; i++) {
      float v = p.offs + p.amp * forme(p.wave, (float)i / (float)n);   // en volts
      v = constrain(v, 0.0f, VREF);                                    // écrêtage
      int code = (int)lroundf(v / VREF * 255.0f);                      // 0..255 (toute la plage 8 bits)
      tab[i] = (uint8_t)constrain(code, 0, 255);
    }
    return n;
  }

  // ---------- Applique un nouvel état ----------
  void appliquer(const Params& p) {
    timerArrete();                                  // l'interruption ne lit plus le tableau

    if (!p.on) {
      dacWrite(DAC_PIN, 0);                         // sortie à 0 V
      return;
    }

    int n = remplirTableau(p);
    idx   = 0;
    nbEch = n;

    // période du timer : 1 / (n x f), en ticks de 40 MHz
    uint32_t ticks = (uint32_t)lroundf((float)TIMER_HZ / ((float)n * p.freq));
    timerDemarre(ticks);
  }

  // ---------- Tâche GBF (core 0) ----------
  void gbfTask(void*) {
    dacWrite(DAC_PIN, 0);                           // active le DAC2 sur GPIO26
    timerInitialise();                              // l'interruption est créée sur ce core

    Params p;
    for (;;) {
      if (xQueueReceive(queueParams, &p, portMAX_DELAY) == pdTRUE) {
        appliquer(p);
      }
    }
  }

}  // namespace

// ============================================================================
//  API
// ============================================================================
void gbfStart() {
  queueParams = xQueueCreate(1, sizeof(Params));
  xTaskCreatePinnedToCore(gbfTask, "gbf", 4096, nullptr, 2, nullptr, 0);
}

void gbfSetOutput(int wave, float freq, float amp, float offs, bool on) {
  if (!queueParams) return;
  Params p = {wave, freq, amp, offs, on};
  xQueueOverwrite(queueParams, &p);                 // le dernier état envoyé écrase l'ancien
}