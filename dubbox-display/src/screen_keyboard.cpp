#include "screen_keyboard.h"
#include <Arduino.h>
#include <string.h>
#include "ui_common.h"

namespace keyboard {
namespace {

using namespace ui;

constexpr int kMaxName = 64;
int maxName = 20;
constexpr int kKeyW = 96;
constexpr int kKeyH = 78;
constexpr int kGap = 14;
constexpr int kRowTop[4] = {232, 324, 416, 508};

constexpr Rect kField = {40, 120, 1240, 190};
constexpr Rect kSpace = {90, 608, 590, 686};
constexpr Rect kCancel = {610, 608, 850, 686};
constexpr Rect kCreate = {870, 608, 1190, 686};
constexpr Rect kBackspace = {1074, 416, 1190, 494};

const char* const kRows[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM-_"};
// Row 3 and 4 are shorter; row 3 leaves room for the delete key at its right end.
constexpr int kRowLeft[4] = {90, 90, 90, 90 + 55};

char g_name[kMaxName + 1] = {0};
int g_len = 0;

Rect keyRect(int row, int col) {
  int x = kRowLeft[row] + col * (kKeyW + kGap);
  return {x, kRowTop[row], x + kKeyW, kRowTop[row] + kKeyH};
}

void drawKey(const Rect& r, const char* label, uint32_t fill = 0x333333) {
  drawButton(r, label, fontLarge(), rgb565(0xffffff), rgb565(fill));
}

void drawField() {
  setCanvas(kVisible);
  fillRect(kField.x1, kField.y1, kField.x2, kField.y2, rgb565(0xffffff));
  fillRect(kField.x1 + 3, kField.y1 + 3, kField.x2 - 3, kField.y2 - 3, rgb565(0x111111));
  char shown[kMaxName + 2];
  snprintf(shown, sizeof(shown), "%s_", g_len > 42 ? g_name + g_len - 42 : g_name);
  while(strlen(shown)>1 && textWidth(fontLarge(),shown)>kField.x2-kField.x1-40)memmove(shown,shown+1,strlen(shown));
  drawText(kField.x1 + 18, kField.y1 + 16, shown, fontLarge(), rgb565(0xffffff), rgb565(0x111111),
           kField.x2 - kField.x1 - 40);
}

}  // namespace

const char* name() { return g_name; }

void showMessage(const char* text) {
  setCanvas(kVisible);
  drawText(40, 196, text, fontSmall(), rgb565(0xff6b6b), rgb565(0x000000), 700);
}

void enter(const char* prompt, const char* submit, int maxLength) {
  maxName = maxLength > 0 && maxLength <= kMaxName ? maxLength : 20;
  g_name[0] = '\0';
  g_len = 0;
  setCanvas(kVisible);
  drawText(40, 76, prompt, fontLarge(), rgb565(0xffffff), rgb565(0x000000));
  drawField();
  for (int row = 0; row < 4; ++row) {
    const char* keys = kRows[row];
    for (int col = 0; keys[col]; ++col) {
      char label[2] = {keys[col], '\0'};
      drawKey(keyRect(row, col), label);
    }
  }
  drawKey(kBackspace, "DEL", 0x6b3030);
  drawKey(kSpace, "SPACE");
  drawKey(kCancel, "CANCEL", 0x8a2b2b);
  drawKey(kCreate, submit, 0x1f7a3d);
}

Action touchDown(int x, int y) {
  if (inRect(kCancel, x, y)) return Action::Cancel;
  if (inRect(kCreate, x, y)) return Action::Create;

  bool changed = false;
  if (inRect(kBackspace, x, y)) {
    if (g_len > 0) {
      g_name[--g_len] = '\0';
      changed = true;
    }
  } else if (inRect(kSpace, x, y)) {
    if (g_len > 0 && g_len < maxName && g_name[g_len - 1] != ' ') {
      g_name[g_len++] = ' ';
      g_name[g_len] = '\0';
      changed = true;
    }
  } else {
    for (int row = 0; row < 4 && !changed; ++row) {
      const char* keys = kRows[row];
      for (int col = 0; keys[col]; ++col) {
        if (inRect(keyRect(row, col), x, y)) {
          if (g_len < maxName) {
            g_name[g_len++] = keys[col];
            g_name[g_len] = '\0';
          }
          changed = true;
          break;
        }
      }
    }
  }
  if (changed) {
    drawField();
    showMessage("");
  }
  return Action::None;
}

}  // namespace keyboard
