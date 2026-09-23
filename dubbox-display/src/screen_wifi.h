#pragma once

// WiFi screen (from the main menu): shows the connection and the address of the upload page, picks a
// network from a scan (or types its name), enters the password on a full on-screen keyboard, and shows
// the progress of an upload arriving over WiFi.
namespace wifiscreen {

void enter();
void touchDown(int x, int y);
// Call every loop while the screen is showing: redraws whatever changed (status, scan results, upload progress).
void update();

}  // namespace wifiscreen
