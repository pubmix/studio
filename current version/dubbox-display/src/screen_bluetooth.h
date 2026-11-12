#pragma once

// Bluetooth screen: on/off, the saved speaker, nearby speakers to pair with, and an I2S-input meter.
namespace btscreen {

void enter();
void touchDown(int x, int y);
void update();

// Draw current state without navigation or device commands.
void redraw();

}  // namespace btscreen
