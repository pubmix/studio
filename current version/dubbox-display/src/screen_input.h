#pragma once

// Input view: one strip each for the internal mic, input 1 (LINE IN L) and input 2 (LINE IN R), with
// a digital gain fader, a live level meter, and the 14.5 kHz notch / 80 Hz low-cut switches.
// Gain and filters live on the Teensy (applied before the meter and the recorder).
namespace inputscreen {

void enter();
// Stops the Teensy's level stream; call when leaving the screen.
void leave();
// Call every loop; `visible` false just discards pending link changes.
void update(bool visible);
void touchDown(int x, int y);
void touchMove(int x, int y);
void touchUp();

// Draw current state without navigation or device commands.
void redraw();

}  // namespace inputscreen
