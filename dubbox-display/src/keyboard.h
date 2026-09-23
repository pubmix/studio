#pragma once
#include <stdint.h>

// An on-screen piano keyboard that plays the Teensy's synth by touch (several fingers at once).
// Used by the Pattern screen's KEYS view and by the Synth screen. Each instance draws white keys
// with black keys on top, and handles the finger list handed to touchPoints().
namespace keyboard {

struct Layout {
  int x0;      // left edge of the first key
  int top;     // top of the keys
  int bottom;  // bottom of the keys
  int areaW;   // width the white keys share
  int blackH;  // height of the black keys
};

struct Keyboard {
  Layout layout;
  int inst = 0;         // the instrument (synth) this keyboard plays
  int keyCount = 14;    // white keys shown (starting from a C); fewer = bigger keys
  int octaveBase = 60;  // MIDI note of the first key (a C)
  // Notes currently down, and when a finger was last seen on each (see touchPoints()).
  int held[5];
  uint32_t seenMs[5];
  int heldCount = 0;
  uint32_t lastKeepAliveMs = 0;
};

// Draws the whole keyboard on the visible layer.
void draw(const Keyboard& k);
// Changes the number of white keys (releases held notes first); the caller redraws.
void setKeyCount(Keyboard& k, int count);
// The MIDI note under a point, or -1.
int keyAt(const Keyboard& k, int x, int y);
// Feed the fingers on the screen once per loop (n may be 0). Starts notes, and releases them once
// a finger has been gone for a moment (the touch chip briefly loses fingers). While notes are
// down the Teensy is told every ~150 ms so it can release anything that stops being refreshed.
// Only the keys that changed are redrawn. With `active` false everything is released.
void touchPoints(Keyboard& k, const int* xs, const int* ys, int n, bool active);
// Lets go of every held note.
void releaseAll(Keyboard& k);

}  // namespace keyboard
