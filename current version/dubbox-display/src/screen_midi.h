#pragma once

// USB MIDI bridge screen (from the main menu): turns the bridge on/off and shows the last
// note it saw, since the normal debug log is silenced while the bridge owns the USB-serial
// port (see midi_bridge.h).
namespace midiscreen {

void enter();
void touchDown(int x, int y);
// Call every loop while the screen is showing: redraws the last-event line as new MIDI arrives.
void update();

// Draw current state without navigation or device commands.
void redraw();

}  // namespace midiscreen
