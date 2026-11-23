#include <M5Unified.h>
#include "../lib/ihm.cpp"
#include "../lib/gbf.cpp"

// Tâche IHM : boutons + affichage (core 1)
void ihmTask(void*) {
  for (;;) {
    ihmUpdate();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = false;          // libère les broches DAC (GPIO25/26) du haut-parleur
  M5.begin(cfg);

  M5.BtnB.setHoldThresh(600);        // durée de l'appui long sur B (ms)

  gbfStart();                        // tâche GBF + timer sur le core 0
  ihmInit();
  xTaskCreatePinnedToCore(ihmTask, "ihm", 8192, nullptr, 1, nullptr, 1);
}

void loop() {
  vTaskDelete(nullptr);              // loop() ne sert plus : tout est dans les tâches
}