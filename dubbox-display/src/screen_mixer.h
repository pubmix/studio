#pragma once

// Mixer view: four channel strips (fader, live peak meter, fx chain, wet level).
namespace mixer {

void enter();
// Call every loop; `visible` false just discards pending link changes.
void update(bool visible);
// Returns the track whose audio-picker was requested ("+" tapped), or -1.
int touchDown(int x, int y);
void touchMove(int x, int y);
void touchUp();

}  // namespace mixer
