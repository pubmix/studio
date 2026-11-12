#pragma once

// The controls of one instrument's synth, drawn inside the Pattern screen: presets, the two
// oscillators, envelope, filter and effects. Choosing a preset opens the parent
// Keys view; Sound returns to these controls.
namespace synthpanel {

// Draws the whole panel for instrument `inst`; `octaveBase` is the MIDI note of the keyboard's
// first key (a C).
void draw(int inst, int octaveBase);
// Draw only: preserve pending preset selection and touch/drag state.
void redraw(int inst);
// Compatibility hook; the parent Keys view owns the keyboard.
void setOctave(int octaveBase);
// A touch went down: true if it landed on a control of the panel (which then handles it).
bool touchDown(int inst, int x, int y);
// Consume a preset selection so the parent can open the playable keyboard.
bool takePresetApplied();
void touchMove(int x);
void touchUp(int inst);
// Every finger currently down, once per loop; `active` false releases held keys.
void touchPoints(int inst, const int* xs, const int* ys, int n, bool active);
// Redraws the values (a change reported by the Teensy, e.g. a project opened).
void refresh(int inst);
// Compatibility hook; the parent owns held keyboard notes.
void releaseKeys();

}  // namespace synthpanel
