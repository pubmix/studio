#pragma once

// The controls of one instrument's synth, drawn inside the Pattern screen: presets, the two
// oscillators, the envelope, the filter and the effects, with a small keyboard at the bottom to
// play the sound while you tweak it.
namespace synthpanel {

// Draws the whole panel for instrument `inst`; `octaveBase` is the MIDI note of the keyboard's
// first key (a C).
void draw(int inst, int octaveBase);
// Shifts the small keyboard to another octave (redraws it).
void setOctave(int octaveBase);
// A touch went down: true if it landed on a control of the panel (which then handles it).
bool touchDown(int inst, int x, int y);
void touchMove(int x);
void touchUp(int inst);
// Every finger currently down, once per loop; `active` false releases held keys.
void touchPoints(int inst, const int* xs, const int* ys, int n, bool active);
// Redraws the values (a change reported by the Teensy, e.g. a project opened).
void refresh(int inst);
// Lets go of any keys held on the small keyboard.
void releaseKeys();

}  // namespace synthpanel
