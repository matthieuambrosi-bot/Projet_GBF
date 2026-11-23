#include "ihm.h"
#include "gbf.h"
#include <math.h>

#define INTENSITE_VIBREUR 255
#define DELAY_SHORT_VIBRATION  80
#define DELAY_LONG_VIBRATION  240

namespace {

  // ======================================================================
  //  POLICES : mets ici celles que tu utilises (légères = programme léger)
  //  Si ta police n'a pas les accents, écris les libellés sans accents.
  // ======================================================================
  const lgfx::IFont* const FONT_PETIT = &fonts::FreeSans9pt7b;      // libellés, aides
  const lgfx::IFont* const FONT_MOYEN = &fonts::FreeSansBold12pt7b; // titre, résumé
  const lgfx::IFont* const FONT_GROS  = &fonts::Font4;              // ON / OFF (taille x2)

  // ======================================================================
  //  COULEURS (RGB565)
  // ======================================================================
  constexpr uint16_t COL_FOND     = TFT_WHITE;
  constexpr uint16_t COL_TRAIT    = TFT_BLACK;
  constexpr uint16_t COL_TITRE    = 0x001F;   // bleu
  constexpr uint16_t COL_SEL_BG   = 0xFFF0;   // jaune très clair  (curseur sur une forme / édition)
  constexpr uint16_t COL_SEL      = 0xFD20;   // orange            (cadre du curseur)
  constexpr uint16_t COL_BAR_BG   = 0xDF7F;   // bleu très clair   (barre sélectionnée / forme choisie)
  constexpr uint16_t COL_BAR_SEL  = 0x041F;   // bleu vif
  constexpr uint16_t COL_VALEUR   = 0x0010;   // bleu foncé
  constexpr uint16_t COL_VALID    = 0x05E0;   // vert
  constexpr uint16_t COL_VALID_BG = 0xDFFB;   // vert très clair
  const uint16_t COL_WAVE[4] = {TFT_RED, 0x001F, 0x03E0, 0xFC00};  // rouge, bleu, vert, orange

  // ======================================================================
  //  GÉOMÉTRIE
  // ======================================================================
  constexpr int WAVE_X = 29;
  constexpr int WAVE_Y = 28;
  constexpr int WAVE_W = 68;
  constexpr int WAVE_H = 105;
  const char* WAVE_LABELS[4] = {"Carre", "Sinus", "Triangle", "Scie"};

  constexpr int BAR_X   = 36;
  constexpr int BAR_Y0  = 139;
  constexpr int BAR_W   = 254;
  constexpr int BAR_H   = 28;
  constexpr int BAR_GAP = 3;
  const char* BAR_LABELS[3] = {"Frequence :", "Amplitude :", "Offset :"};

  constexpr int SEP_X = BAR_X + 110;

  constexpr int VAL_X = BAR_X + 190;
  constexpr int VAL_W = BAR_X + BAR_W - VAL_X;
  constexpr int VAL_Y = BAR_Y0;
  constexpr int VAL_H = 3 * BAR_H + 2 * BAR_GAP;

  // ======================================================================
  //  ÉTAT DE L'APPLICATION
  // ======================================================================
  enum Mode { SELECTION, EDITION, SORTIE };
  Mode mode = SELECTION;

  int  cursor     = 0;      // élément où se trouve le curseur (0..7)
  int  current    = -1;     // élément actuellement dessiné en surbrillance
  int  chosenWave = 0;      // forme d'onde choisie (reste en bleu)

  float freq = 1000;        // Hz
  float amp  = 3.3;         // V
  float offs = 0;           // V
  bool  outputOn = false;

  // Pas de modification (un clic sur A = -pas, un clic sur C = +pas)
  constexpr float STEP_FREQ = 100;
  constexpr float STEP_AMP  = 0.1;
  constexpr float STEP_OFFS = 0.1;

  // Limites
  constexpr float FREQ_MIN = 1, FREQ_MAX = 1500;
  constexpr float AMP_MIN  = 0,   AMP_MAX  = 3.3;
  constexpr float OFFS_MIN = 0,   OFFS_MAX = 3.3;

  // Texte affiché pour chaque valeur (i : 0 = freq, 1 = amp, 2 = offset)
  String valueText(int i) {
    if (i == 0) return String((int)freq) + " Hz";
    if (i == 1) return String(amp, 1) + " V";
    return String(offs, 1) + " V";
  }

  // Pas de fréquence : on ne change que le chiffre le plus à gauche
  // sens = +1 (augmenter) ou -1 (diminuer)
  float freqPas(float f, int sens) {
    if (sens > 0) {
      if (f < 10)  return 1;      // 1 -> 10 : de 1 en 1
      if (f < 100) return 10;     // 10 -> 100 : de 10 en 10
      return 100;                 // 100 -> 1500 : de 100 en 100
    } else {
      if (f <= 10)  return 1;     // en descendant, 10 -> 9 : on repasse à 1 en 1
      if (f <= 100) return 10;    // en descendant, 100 -> 90 : on repasse à 10 en 10
      return 100;
    }
  }

  float arrondi1(float v) { return roundf(v * 10.0f) / 10.0f; }

  // ======================================================================
  //  SORTIE RÉELLE : à compléter plus tard (génération du signal)
  // ======================================================================
  void appliquerSortie() {
    gbfSetOutput(chosenWave, freq, amp, offs, outputOn);
  }

  // ======================================================================
  //  OUTILS DE DESSIN
  // ======================================================================
  // Rectangle avec épaisseur (vers l'intérieur)
  void thickRect(int x, int y, int w, int h, int t, uint16_t c) {
    for (int k = 0; k < t; k++) M5.Display.drawRect(x + k, y + k, w - 2 * k, h - 2 * k, c);
  }

  // Trait épais (2 px)
  void tline(int x0, int y0, int x1, int y1, uint16_t c) {
    M5.Display.drawLine(x0, y0, x1, y1, c);
    M5.Display.drawLine(x0, y0 + 1, x1, y1 + 1, c);
  }

  // ---------- Courbes modèles ----------
  void drawCurve(int i, uint16_t col) {
    int bx  = WAVE_X + i * WAVE_W;
    int x0  = bx + 6;
    int w   = WAVE_W - 12;
    int top = WAVE_Y + 10;
    int bot = WAVE_Y + WAVE_H - 28;
    int mid = (top + bot) / 2;
    int P   = w / 2;                       // 2 périodes

    switch (i) {
      case 0: { // Carré
        int x = x0;
        for (int p = 0; p < 2; p++) {
          tline(x, top, x + P / 2, top, col);
          tline(x + P / 2, top, x + P / 2, bot, col);
          tline(x + P / 2, bot, x + P, bot, col);
          if (p == 0) tline(x + P, bot, x + P, top, col);
          x += P;
        }
        break;
      }
      case 1: { // Sinus
        int amp2 = (bot - top) / 2;
        int px = x0, py = mid;
        for (int k = 1; k <= w; k++) {
          int cx = x0 + k;
          int cy = mid - (int)(amp2 * sinf(2.0f * M_PI * 2.0f * k / w));
          tline(px, py, cx, cy, col);
          px = cx; py = cy;
        }
        break;
      }
      case 2: { // Triangle
        int x = x0;
        for (int p = 0; p < 2; p++) {
          tline(x,             mid, x + P / 4,     top, col);
          tline(x + P / 4,     top, x + 3 * P / 4, bot, col);
          tline(x + 3 * P / 4, bot, x + P,         mid, col);
          x += P;
        }
        break;
      }
      case 3: { // Dent de scie
        int x = x0;
        for (int p = 0; p < 2; p++) {
          tline(x,     bot, x + P, top, col);
          tline(x + P, top, x + P, bot, col);
          x += P;
        }
        break;
      }
    }
  }

  // ---------- Forme d'onde ----------
  // sel    : le curseur est dessus        -> jaune / orange
  // chosen : forme choisie, curseur ailleurs -> bleu
  void drawWave(int i, bool sel) {
    auto& d = M5.Display;
    int x = WAVE_X + i * WAVE_W;
    bool chosen = (!sel && i == chosenWave);

    uint16_t bg    = sel ? COL_SEL_BG : (chosen ? COL_BAR_BG : COL_FOND);
    uint16_t frame = sel ? COL_SEL    : COL_BAR_SEL;
    uint16_t txt   = sel ? COL_SEL    : (chosen ? COL_BAR_SEL : COL_TRAIT);

    d.fillRect(x, WAVE_Y, WAVE_W, WAVE_H, bg);
    d.drawRect(x, WAVE_Y, WAVE_W, WAVE_H, COL_TRAIT);
    if (sel || chosen) thickRect(x, WAVE_Y, WAVE_W, WAVE_H, 3, frame);

    drawCurve(i, COL_WAVE[i]);

    d.setFont(FONT_PETIT);
    d.setTextDatum(bottom_center);
    d.setTextColor(txt, bg);
    d.drawString(WAVE_LABELS[i], x + WAVE_W / 2, WAVE_Y + WAVE_H - 4);
  }

  // ---------- Barre Fréquence / Amplitude / Offset ----------
  // sel = curseur dessus : bleu en sélection, jaune/orange en édition
  void drawBar(int i, bool sel) {
    auto& d = M5.Display;
    int y = BAR_Y0 + i * (BAR_H + BAR_GAP);
    int w = VAL_X - BAR_X - 4;
    bool editing = sel && (mode == EDITION);

    uint16_t bg    = editing ? COL_SEL_BG : (sel ? COL_BAR_BG : COL_FOND);
    uint16_t frame = editing ? COL_SEL    : COL_BAR_SEL;

    d.fillRect(BAR_X, y, w, BAR_H, bg);
    d.drawRect(BAR_X, y, w, BAR_H, COL_TRAIT);
    if (sel) thickRect(BAR_X, y, w, BAR_H, 3, frame);

    d.setFont(FONT_PETIT);
    d.setTextDatum(middle_left);
    d.setTextColor(sel ? frame : COL_TRAIT, bg);
    d.drawString(BAR_LABELS[i], BAR_X + 6, y + BAR_H / 2);

    d.drawFastVLine(SEP_X, y + 1, BAR_H - 2, sel ? frame : COL_TRAIT);

    d.setTextColor(COL_VALEUR, bg);
    d.drawString(valueText(i), SEP_X + 6, y + BAR_H / 2);
  }

  // ---------- Bouton Valider ----------
  void drawValider(bool sel) {
    auto& d = M5.Display;
    uint16_t bg = sel ? COL_VALID : COL_VALID_BG;

    d.fillRect(VAL_X, VAL_Y, VAL_W, VAL_H, bg);
    thickRect(VAL_X, VAL_Y, VAL_W, VAL_H, sel ? 3 : 2, COL_VALID);

    d.setFont(FONT_PETIT);
    d.setTextDatum(middle_center);
    d.setTextColor(sel ? TFT_WHITE : COL_VALID, bg);
    d.drawString("Valider", VAL_X + VAL_W / 2, VAL_Y + VAL_H / 2);
  }

  void drawItem(int idx, bool sel) {
    if (idx >= 0 && idx <= 3)      drawWave(idx, sel);
    else if (idx >= 4 && idx <= 6) drawBar(idx - 4, sel);
    else if (idx == 7)             drawValider(sel);
  }

  // ---------- Déplace la surbrillance (sans scintillement) ----------
  void moveHighlight(int index) {
    int old = current;
    current = index;
    if (old >= 0) drawItem(old, false);        // l'ancien repasse en normal (ou bleu si c'est la forme choisie)
    if (current >= 0) drawItem(current, true);
}
  
}  // namespace

void shortVibration(uint8_t intensite, uint32_t duree)
{
  M5.Power.setVibration(intensite);
  delay(duree);
  M5.Power.setVibration(0);
} 

// ==========================================================================
//  MODE 1 : SÉLECTION
// ==========================================================================
void ihmInit() {
  auto& d = M5.Display;

  mode = SELECTION;

  d.fillScreen(COL_FOND);
  d.drawRect(0, 0, d.width(), d.height(), COL_TRAIT);

  d.setFont(FONT_MOYEN);
  d.setTextDatum(top_center);
  d.setTextColor(COL_TITRE, COL_FOND);
  d.drawString("GBF", d.width() / 2, 1);

  for (int i = 0; i < IHM_NB_ITEMS; i++) drawItem(i, false);

  current = -1;
  cursor  = chosenWave;           // le curseur repart sur la forme choisie
  moveHighlight(cursor);          // toujours un élément en surbrillance
}

void ihmSelect(int index) {
  cursor = index;
  moveHighlight(index);
}

// ==========================================================================
//  MODE 2 : RÉSUMÉ DES PARAMÈTRES + SORTIE ON/OFF
// ==========================================================================
void ihmShowOutput(int wave, bool on) {
  auto& d = M5.Display;
  const int W = d.width();
  const int H = d.height();

  d.fillScreen(COL_FOND);
  d.drawRect(0, 0, W, H, COL_TRAIT);

  // --- Moitié haute : paramètres choisis ---
  d.setFont(FONT_MOYEN);
  d.setTextDatum(middle_left);

  const char* names[4] = {"Forme", "Freq.", "Ampl.", "Offset"};
  String vals[4] = {WAVE_LABELS[wave], valueText(0), valueText(1), valueText(2)};
  const uint16_t valCol[4] = {COL_WAVE[wave], COL_VALEUR, COL_VALEUR, COL_VALEUR};

  for (int i = 0; i < 4; i++) {
    int y = 18 + i * 28;
    d.setTextColor(COL_TRAIT, COL_FOND);
    d.drawString(String(names[i]) + " :", 30, y);
    d.setTextColor(valCol[i], COL_FOND);
    d.drawString(vals[i], 150, y);
  }

  // Séparation haut / bas
  d.drawFastHLine(10, H / 2, W - 20, COL_TRAIT);

  // --- Moitié basse : ON / OFF centré ---
  uint16_t c = on ? COL_VALID : TFT_RED;
  int bw = 140, bh = 56;
  int bx = (W - bw) / 2;
  int by = H / 2 + 12;
  d.fillRoundRect(bx, by, bw, bh, 10, c);

  d.setFont(FONT_GROS);
  d.setTextSize(2);
  d.setTextDatum(middle_center);
  d.setTextColor(TFT_WHITE, c);
  d.drawString(on ? "ON" : "OFF", W / 2, by + bh / 2);
  d.setTextSize(1);               // important : remettre à 1

  // Aides boutons
  d.setFont(FONT_PETIT);
  d.setTextColor(COL_TRAIT, COL_FOND);
  d.setTextDatum(middle_center);
  d.drawString("push B : ON/OFF", W / 4, H - 22);
  d.drawString("hold B : retour", 3 * W / 4, H - 22);
}

// ==========================================================================
//  BOUTONS
//   SELECTION : A = précédent, C = suivant, B = action
//   EDITION   : A = -, C = +, B = terminer
//   SORTIE    : push B = ON/OFF, hold B = retour à la sélection
// ==========================================================================
void ihmUpdate() {
  M5.update();

  switch (mode) {

    // ------------------------------------------------------------------
    case SELECTION:
      if (M5.BtnA.wasClicked()) {
        shortVibration(INTENSITE_VIBREUR, DELAY_SHORT_VIBRATION);
        cursor = (cursor + IHM_NB_ITEMS - 1) % IHM_NB_ITEMS;
        moveHighlight(cursor);
      }
      if (M5.BtnC.wasClicked()) {
        shortVibration(INTENSITE_VIBREUR, DELAY_SHORT_VIBRATION);
        cursor = (cursor + 1) % IHM_NB_ITEMS;
        moveHighlight(cursor);
      }
      if (M5.BtnB.wasClicked()) {
      shortVibration(INTENSITE_VIBREUR, DELAY_SHORT_VIBRATION);

      if (cursor >= 0 && cursor <= 3) {            // Courbe : on la choisit, puis on passe à Fréquence
        int ancienne = chosenWave;
        chosenWave = cursor;                       // nouvelle forme choisie
        if (ancienne != chosenWave) drawItem(ancienne, false);   // l'ancienne bleue repasse en blanc
        cursor = 4;
        moveHighlight(cursor);                     // la courbe choisie passe en bleu, Fréquence s'allume
      } else if (cursor >= 4 && cursor <= 6) {   // Fréq / Ampl / Offset -> édition
        mode = EDITION;
        drawItem(cursor, true);                  // redessine en style "édition"
      } else if (cursor == 7) {                  // Valider -> mode 2
        mode = SORTIE;
        outputOn = false;
        ihmShowOutput(chosenWave, outputOn);
        appliquerSortie();
      }
    }
        // sur une forme d'onde : rien à faire, elle est déjà choisie
      break;

    // ------------------------------------------------------------------
    case EDITION: {
      int step = 0;
      if (M5.BtnA.wasClicked()){
        shortVibration(INTENSITE_VIBREUR, DELAY_SHORT_VIBRATION);
        step = -1;
      } 
      if (M5.BtnC.wasClicked()){
        shortVibration(INTENSITE_VIBREUR, DELAY_SHORT_VIBRATION);
        step = +1;
      } 
      


      if (step != 0) {
      if (cursor == 4) freq = constrain(freq + step * freqPas(freq, step), FREQ_MIN, FREQ_MAX);        
      if (cursor == 5) amp  = constrain(arrondi1(amp  + step * STEP_AMP),  AMP_MIN,  AMP_MAX);
      if (cursor == 6) offs = constrain(arrondi1(offs + step * STEP_OFFS), OFFS_MIN, OFFS_MAX);
        drawItem(cursor, true);                  // redessine la barre avec la nouvelle valeur
      }
      if (M5.BtnB.wasClicked()) {
        shortVibration(INTENSITE_VIBREUR, DELAY_SHORT_VIBRATION);
        mode = SELECTION;
        drawItem(cursor, true);                  // retour au style "sélection"
      }
      break;
    }

    // ------------------------------------------------------------------
    case SORTIE:
      if (M5.BtnB.wasClicked()) {                // push B : ON/OFF
        shortVibration(INTENSITE_VIBREUR, DELAY_LONG_VIBRATION);
        outputOn = !outputOn;
        ihmShowOutput(chosenWave, outputOn);
        appliquerSortie();
      }
      if (M5.BtnB.wasHold()) {                   // hold B : retour
        shortVibration(INTENSITE_VIBREUR, DELAY_LONG_VIBRATION);
        outputOn = false;
        appliquerSortie();                       // coupe la sortie
        ihmInit();                               // revient au mode 1
      }
      break;
  }
}


// ==========================================================================
//  GETTERS (pour le futur gbf.cpp)
// ==========================================================================
int   ihmGetWave()    { return chosenWave; }
float ihmGetFreq()    { return freq; }
float ihmGetAmp()     { return amp; }
float ihmGetOffset()  { return offs; }
bool  ihmIsOutputOn() { return outputOn; }
