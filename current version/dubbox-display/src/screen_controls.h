#pragma once

// CONTROLS TEST screen (from the main menu): shows the four faders, four encoders (turn + push), and the
// four fx-select and four drum-pad buttons live, straight from the Teensy's raw inputs. Each control turns
// green once it has been seen working (fader moved across most of its travel, encoder turned both ways and
// pushed, button pressed), so a bench check is: touch everything until the counter reads 24 / 24.
// The Teensy streams the raw states only while this screen is open (`#,1` / `#,0` on the link).
namespace controlsscreen {

void enter();
// Stops the Teensy's control stream; call when leaving the screen.
void leave();
void touchDown(int x, int y);
// Call every loop; `visible` false just discards pending link changes.
void update(bool visible);

// Draw current state without navigation or device commands.
void redraw();

}  // namespace controlsscreen
