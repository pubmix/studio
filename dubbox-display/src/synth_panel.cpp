#include "synth_panel.h"
#include <Arduino.h>
#include <stdio.h>
#include "keyboard.h"
#include "teensy_link.h"
#include "ui_common.h"

namespace synthpanel {
namespace {

using namespace ui;

constexpr int kParams = teensylink::kSynthParams;

// Parameter indexes (same order as the Teensy's SynthPatch).
enum Param {
  kWave1, kWave2, kOct2, kDetune, kMix, kAttack, kDecay, kSustain, kRelease,
  kCutoff, kResonance, kChorus, kReverb, kLevel
};

constexpr int kColX[3] = {16, 440, 864};
constexpr int kColW = 404;
constexpr int kLabelW = 126;
constexpr int kBarOffset = 132;
constexpr int kBarW = 206;
constexpr int kRowH = 40;

struct SliderDef {
  int param;
  const char* label;
  int col;
  int y;
  uint32_t rgb;
};
constexpr SliderDef kSliders[] = {
    {kOct2, "OSC2 OCT", 0, 346, 0xff9f3d},
    {kDetune, "DETUNE", 0, 390, 0xff9f3d},
    {kMix, "OSC MIX", 0, 434, 0xff9f3d},
    {kAttack, "ATTACK", 1, 252, 0x3d7fff},
    {kDecay, "DECAY", 1, 296, 0x3d7fff},
    {kSustain, "SUSTAIN", 1, 340, 0x3d7fff},
    {kRelease, "RELEASE", 1, 384, 0x3d7fff},
    {kCutoff, "CUTOFF", 2, 252, 0x4dd0e1},
    {kResonance, "RESO", 2, 296, 0x4dd0e1},
    {kChorus, "CHORUS", 2, 340, 0x66bb6a},
    {kReverb, "REVERB", 2, 384, 0x66bb6a},
    {kLevel, "LEVEL", 2, 428, 0xec407a},
};
constexpr int kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);

constexpr int kPresetY = 204;
constexpr int kWaveRowY[2] = {252, 296};
const char* const kWaveNames[4] = {"SIN", "TRI", "SAW", "SQR"};

struct Preset {
  const char* name;
  int v[kParams];
};
// wave1, wave2, oct2, detune, mix, attack, decay, sustain, release, cutoff, reso, chorus, reverb, level
constexpr Preset kPresets[] = {
    {"PIANO", {1, 0, 1, 0, 30, 3, 59, 18, 27, 100, 0, 0, 0, 80}},
    {"LEAD", {2, 2, 0, 12, 50, 3, 30, 80, 20, 75, 25, 40, 15, 70}},
    {"PAD", {2, 1, 0, 8, 50, 60, 50, 70, 70, 55, 10, 60, 50, 70}},
    {"BASS", {3, 2, -1, 0, 50, 1, 40, 60, 15, 40, 30, 0, 0, 90}},
    {"PLUCK", {2, 3, 1, -7, 30, 0, 22, 0, 20, 60, 35, 20, 20, 75}},
    {"ORGAN", {0, 0, 1, 0, 50, 2, 10, 100, 12, 100, 0, 40, 10, 70}},
};
constexpr int kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]);

constexpr keyboard::Layout kKeysLayout = {16, 486, 636, 1248, 96};
constexpr uint32_t kSendIntervalMs = 40;

keyboard::Keyboard kb;
int dragSlider = -1;     // slider being dragged (index into kSliders), or -1
int dragInst = 0;
uint32_t lastSendMs = 0;

int value(int inst, int param) { return teensylink::state().synth[inst].v[param]; }

void paramRange(int param, int& lo, int& hi) {
  lo = 0;
  hi = 100;
  if (param == kWave1 || param == kWave2) hi = 3;
  if (param == kOct2) {
    lo = -2;
    hi = 2;
  }
  if (param == kDetune) {
    lo = -50;
    hi = 50;
  }
}

Rect presetRect(int i) { return {16 + i * 152, kPresetY, 16 + i * 152 + 146, kPresetY + 38}; }

Rect waveRect(int row, int w) {
  const int x = kColX[0] + kBarOffset + w * 70;
  return {x, kWaveRowY[row], x + 66, kWaveRowY[row] + 36};
}

int barX1(const SliderDef& s) { return kColX[s.col] + kBarOffset; }

// ---- Drawing ----

void drawSliderBar(int inst, const SliderDef& s) {
  int lo, hi;
  paramRange(s.param, lo, hi);
  const int x1 = barX1(s);
  const int x2 = x1 + kBarW;
  int v = value(inst, s.param);
  v = v < lo ? lo : (v > hi ? hi : v);
  const int fx = x1 + (int)((long)(v - lo) * kBarW / (hi - lo));
  setCanvas(kVisible);
  fillRect(x1, s.y + 4, x2, s.y + 32, rgb565(0x2a2a33));
  if (fx > x1) fillRect(x1, s.y + 4, fx, s.y + 32, rgb565(s.rgb, 0.8f));
  fillRect(max(x1, fx - 3), s.y + 2, min(x2, fx + 3), s.y + 34, rgb565(0xffffff));
}

void drawSliderValue(int inst, const SliderDef& s) {
  char buf[8];
  int v = value(inst, s.param);
  if (s.param == kOct2 || s.param == kDetune) {
    snprintf(buf, sizeof(buf), "%+d", v);
  } else {
    snprintf(buf, sizeof(buf), "%d", v);
  }
  setCanvas(kVisible);
  drawText(barX1(s) + kBarW + 12, s.y + 6, buf, fontSmall(), rgb565(0xffffff), rgb565(0x000000), 52);
}

void drawSliderStatic(const SliderDef& s) {
  setCanvas(kVisible);
  drawText(kColX[s.col] + 4, s.y + 6, s.label, fontSmall(), rgb565(0xdddddd), rgb565(0x000000), kLabelW - 8);
}

void drawWaveRow(int inst, int row) {
  const int param = row == 0 ? kWave1 : kWave2;
  setCanvas(kVisible);
  drawText(kColX[0] + 4, kWaveRowY[row] + 4, row == 0 ? "OSC 1" : "OSC 2", fontSmall(), rgb565(0xdddddd),
           rgb565(0x000000), kLabelW - 8);
  for (int w = 0; w < 4; ++w) {
    const bool on = value(inst, param) == w;
    drawButton(waveRect(row, w), kWaveNames[w], fontSmall(), rgb565(on ? 0x000000 : 0xffffff),
               rgb565(on ? 0xff9f3d : 0x3a3a48));
  }
}

// Every value on the page (after a preset or a change reported by the Teensy).
void drawValues(int inst) {
  drawWaveRow(inst, 0);
  drawWaveRow(inst, 1);
  for (int i = 0; i < kNumSliders; ++i) {
    drawSliderBar(inst, kSliders[i]);
    drawSliderValue(inst, kSliders[i]);
  }
}

// The preset the current settings match, or -1.
int matchingPreset(int inst) {
  for (int p = 0; p < kNumPresets; ++p) {
    bool same = true;
    for (int i = 0; i < kParams && same; ++i) same = kPresets[p].v[i] == value(inst, i);
    if (same) return p;
  }
  return -1;
}

void drawPresets(int inst) {
  setCanvas(kVisible);
  const int active = matchingPreset(inst);
  for (int p = 0; p < kNumPresets; ++p) {
    drawButton(presetRect(p), kPresets[p].name, fontSmall(), rgb565(p == active ? 0x000000 : 0xffffff),
               rgb565(p == active ? 0x66bb6a : 0x2f4a36));
  }
}

// ---- Slider dragging ----

// Value for a touch x on slider `s`.
int valueForX(const SliderDef& s, int x) {
  int lo, hi;
  paramRange(s.param, lo, hi);
  int px = x - barX1(s);
  px = px < 0 ? 0 : (px > kBarW ? kBarW : px);
  return lo + (int)(((long)px * (hi - lo) + kBarW / 2) / kBarW);
}

// Moves the dragged slider to the finger: the bar follows at once, the Teensy is told at most
// every kSendIntervalMs (and the final value on release).
void dragTo(int x) {
  if (dragSlider < 0) return;
  const SliderDef& s = kSliders[dragSlider];
  const int v = valueForX(s, x);
  if (v == value(dragInst, s.param)) return;
  const uint32_t now = millis();
  if (now - lastSendMs >= kSendIntervalMs) {
    teensylink::setSynthParam(dragInst, s.param, v);
    lastSendMs = now;
  } else {
    teensylink::setSynthParamLocal(dragInst, s.param, v);
  }
  drawSliderBar(dragInst, s);
}

}  // namespace

void draw(int inst, int octaveBase) {
  kb.layout = kKeysLayout;
  kb.inst = inst;
  kb.octaveBase = octaveBase;
  kb.heldCount = 0;
  dragSlider = -1;
  setCanvas(kVisible);
  drawPresets(inst);
  for (int i = 0; i < kNumSliders; ++i) drawSliderStatic(kSliders[i]);
  drawValues(inst);
  keyboard::draw(kb);
}

void setOctave(int octaveBase) {
  keyboard::releaseAll(kb);
  kb.octaveBase = octaveBase;
  keyboard::draw(kb);
}

bool touchDown(int inst, int x, int y) {
  dragSlider = -1;
  for (int i = 0; i < kNumPresets; ++i) {
    if (inRect(presetRect(i), x, y)) {
      teensylink::setSynthAll(inst, kPresets[i].v);
      drawPresets(inst);
      drawValues(inst);
      return true;
    }
  }
  for (int row = 0; row < 2; ++row) {
    for (int w = 0; w < 4; ++w) {
      if (inRect(waveRect(row, w), x, y)) {
        teensylink::setSynthParam(inst, row == 0 ? kWave1 : kWave2, w);
        drawWaveRow(inst, row);
        drawPresets(inst);
        return true;
      }
    }
  }
  for (int i = 0; i < kNumSliders; ++i) {
    const SliderDef& s = kSliders[i];
    if (x >= kColX[s.col] && x <= kColX[s.col] + kColW && y >= s.y && y < s.y + kRowH) {
      dragSlider = i;
      dragInst = inst;
      lastSendMs = 0;
      dragTo(x);
      return true;
    }
  }
  return false;
}

void touchMove(int x) { dragTo(x); }

void touchUp(int inst) {
  if (dragSlider >= 0) {
    const SliderDef& s = kSliders[dragSlider];
    // Send the final value (the last move may have been held back) and show it as text.
    teensylink::setSynthParam(dragInst, s.param, value(dragInst, s.param));
    drawSliderValue(dragInst, s);
    drawPresets(inst);
  }
  dragSlider = -1;
}

void touchPoints(int inst, const int* xs, const int* ys, int n, bool active) {
  if (kb.inst != inst) {
    keyboard::releaseAll(kb);
    kb.inst = inst;
  }
  keyboard::touchPoints(kb, xs, ys, n, active);
}

void refresh(int inst) {
  if (dragSlider >= 0) return;
  drawPresets(inst);
  drawValues(inst);
}

void releaseKeys() { keyboard::releaseAll(kb); }

}  // namespace synthpanel
