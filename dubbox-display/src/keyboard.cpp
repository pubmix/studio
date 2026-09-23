#include "keyboard.h"
#include <Arduino.h>
#include "teensy_link.h"
#include "ui_common.h"

namespace keyboard {
namespace {

using namespace ui;

constexpr int kWhiteSemis[7] = {0, 2, 4, 5, 7, 9, 11};
constexpr uint32_t kReleaseDebounceMs = 70;  // the touch chip drops a finger for a frame or two now and then
constexpr uint32_t kKeepAliveMs = 150;
constexpr uint32_t kWhiteRgb = 0xf2f2f2;
constexpr uint32_t kWhiteHeldRgb = 0xb45cff;
constexpr uint32_t kBlackRgb = 0x141418;
constexpr uint32_t kBlackHeldRgb = 0x8a3fd6;

int whiteW(const Keyboard& k) { return k.layout.areaW / k.keyCount; }
int blackW(const Keyboard& k) { return whiteW(k) * 3 / 5; }

// MIDI note of white key `i`.
int whiteMidi(const Keyboard& k, int i) { return k.octaveBase + (i / 7) * 12 + kWhiteSemis[i % 7]; }

// Is there a black key between white key `i` and `i + 1`?
bool hasBlackAfter(const Keyboard& k, int i) {
  int w = i % 7;
  return i < k.keyCount - 1 && (w == 0 || w == 1 || w == 3 || w == 4 || w == 5);  // C D F G A
}

int blackX(const Keyboard& k, int i) { return k.layout.x0 + (i + 1) * whiteW(k) - blackW(k) / 2; }

bool isHeld(const Keyboard& k, int midi) {
  for (int i = 0; i < k.heldCount; ++i) {
    if (k.held[i] == midi) return true;
  }
  return false;
}

// True if (x, y) is on the key for `midi`, or within `margin` px of it. Used to keep a held key
// held while the finger jitters near its edge.
bool onKey(const Keyboard& k, int midi, int x, int y, int margin) {
  const Layout& L = k.layout;
  const int blackBottom = L.top + L.blackH;
  const int ww = whiteW(k);
  for (int i = 0; i < k.keyCount; ++i) {
    if (hasBlackAfter(k, i) && whiteMidi(k, i) + 1 == midi) {
      return x >= blackX(k, i) - margin && x < blackX(k, i) + blackW(k) + margin && y >= L.top - margin &&
             y <= blackBottom + margin;
    }
    if (whiteMidi(k, i) == midi) {
      const int wx = L.x0 + i * ww;
      const int right = wx + ww - 4;
      const int xl = wx + ((i > 0 && hasBlackAfter(k, i - 1)) ? blackW(k) / 2 : 0);
      const int xr = hasBlackAfter(k, i) ? right - (blackW(k) / 2 - 4) - 1 : right;
      if (y > blackBottom) return x >= wx - margin && x <= right + margin && y <= L.bottom + margin;
      return x >= xl - margin && x <= xr + margin && y >= L.top - margin;
    }
  }
  return false;
}

// A white key is drawn in two pieces so it never paints over the black keys beside it: the part
// below the black keys (full width) and the part between them. No text, so it draws fast.
void drawWhiteKey(const Keyboard& k, int i) {
  const Layout& L = k.layout;
  const int m = whiteMidi(k, i);
  const uint16_t col = isHeld(k, m) ? rawRgb565(theme::palette().accent) : rawRgb565(kWhiteRgb);
  const int ww = whiteW(k);
  const int x = L.x0 + i * ww;
  const int right = x + ww - 4;
  const int blackBottom = L.top + L.blackH;
  const int xl = x + ((i > 0 && hasBlackAfter(k, i - 1)) ? blackW(k) / 2 : 0);
  const int xr = hasBlackAfter(k, i) ? right - (blackW(k) / 2 - 4) - 1 : right;
  fillRect(xl, L.top, xr, blackBottom, col);
  fillRect(x, blackBottom + 1, right, L.bottom, col);
  if (m % 12 == 0) {  // a small mark on every C
    fillRect(x + 10, L.bottom - 24, x + 24, L.bottom - 10, rgb565(isHeld(k, m) ? 0xffffff : 0x8a8a94));
  }
}

void drawBlackKey(const Keyboard& k, int i) {
  const Layout& L = k.layout;
  const int m = whiteMidi(k, i) + 1;
  fillRect(blackX(k, i), L.top, blackX(k, i) + blackW(k) - 1, L.top + L.blackH,
           rawRgb565(isHeld(k, m) ? theme::palette().accent : kBlackRgb));
}

// Redraws the one key for `midi` (white or black).
void drawKeyNote(const Keyboard& k, int midi) {
  for (int i = 0; i < k.keyCount; ++i) {
    if (whiteMidi(k, i) == midi) {
      drawWhiteKey(k, i);
      return;
    }
    if (hasBlackAfter(k, i) && whiteMidi(k, i) + 1 == midi) {
      drawBlackKey(k, i);
      return;
    }
  }
}

}  // namespace

void draw(const Keyboard& k) {
  setCanvas(kVisible);
  for (int i = 0; i < k.keyCount; ++i) drawWhiteKey(k, i);
  for (int i = 0; i < k.keyCount; ++i) {
    if (hasBlackAfter(k, i)) drawBlackKey(k, i);
  }
}

void setKeyCount(Keyboard& k, int count) {
  releaseAll(k);
  k.keyCount = count < 1 ? 1 : count;
}

int keyAt(const Keyboard& k, int x, int y) {
  const Layout& L = k.layout;
  if (x < L.x0 || x >= L.x0 + k.keyCount * whiteW(k) || y < L.top || y > L.bottom) return -1;
  if (y <= L.top + L.blackH) {
    for (int i = 0; i < k.keyCount; ++i) {
      if (!hasBlackAfter(k, i)) continue;
      int bx = blackX(k, i);
      if (x >= bx && x < bx + blackW(k)) return whiteMidi(k, i) + 1;
    }
  }
  return whiteMidi(k, (x - L.x0) / whiteW(k));
}

void releaseAll(Keyboard& k) {
  for (int i = 0; i < k.heldCount; ++i) {
    if (g.linked) teensylink::sendLiveNote(k.inst, k.held[i], false);
  }
  k.heldCount = 0;
}

void touchPoints(Keyboard& k, const int* xs, const int* ys, int n, bool active) {
  if (!active) {
    if (k.heldCount > 0) releaseAll(k);
    return;
  }
  const uint32_t now = millis();
  int cur[5];
  int curCount = 0;
  for (int i = 0; i < n && i < 5; ++i) {
    int m = -1;
    for (int h = 0; h < k.heldCount && m < 0; ++h) {  // a finger that stays near a held key keeps it
      if (onKey(k, k.held[h], xs[i], ys[i], 10)) m = k.held[h];
    }
    if (m < 0) m = keyAt(k, xs[i], ys[i]);
    if (m < 0) continue;
    bool dup = false;
    for (int j = 0; j < curCount; ++j) dup = dup || cur[j] == m;
    if (!dup) cur[curCount++] = m;
  }

  int toRedraw[10];
  int redrawCount = 0;
  for (int j = 0; j < curCount; ++j) {
    int idx = -1;
    for (int i = 0; i < k.heldCount; ++i) {
      if (k.held[i] == cur[j]) idx = i;
    }
    if (idx >= 0) {
      k.seenMs[idx] = now;  // still down
    } else if (k.heldCount < 5) {  // a new note
      k.held[k.heldCount] = cur[j];
      k.seenMs[k.heldCount] = now;
      ++k.heldCount;
      if (g.linked) teensylink::sendLiveNote(k.inst, cur[j], true);
      if (redrawCount < 10) toRedraw[redrawCount++] = cur[j];
    }
  }
  for (int i = k.heldCount - 1; i >= 0; --i) {  // notes whose finger has been gone long enough
    bool present = false;
    for (int j = 0; j < curCount; ++j) present = present || cur[j] == k.held[i];
    if (present || now - k.seenMs[i] <= kReleaseDebounceMs) continue;
    if (g.linked) teensylink::sendLiveNote(k.inst, k.held[i], false);
    if (redrawCount < 10) toRedraw[redrawCount++] = k.held[i];
    for (int h = i; h + 1 < k.heldCount; ++h) {
      k.held[h] = k.held[h + 1];
      k.seenMs[h] = k.seenMs[h + 1];
    }
    --k.heldCount;
  }

  if (now - k.lastKeepAliveMs >= kKeepAliveMs) {
    k.lastKeepAliveMs = now;
    if (g.linked) {
      for (int i = 0; i < k.heldCount; ++i) teensylink::sendNoteRefresh(k.inst, k.held[i]);
    }
  }

  if (redrawCount > 0) {
    setCanvas(kVisible);
    for (int i = 0; i < redrawCount; ++i) drawKeyNote(k, toRedraw[i]);
  }
}

}  // namespace keyboard
