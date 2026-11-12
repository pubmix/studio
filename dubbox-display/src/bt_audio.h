#pragma once
#include <stdint.h>

// Streams the Teensy's I2S1 audio out over Bluetooth (A2DP source) to a speaker or headphones.
//
// Wiring (3.3 V logic, the ESP32 only ever listens): Teensy 21 (BCLK) -> GPIO34, Teensy 20 (LRCLK) ->
// GPIO35, Teensy 7 (data out) -> GPIO36. The Teensy drives the clocks; the ESP32 is an I2S slave.
namespace btaudio {

constexpr int kMaxDevices = 8;
constexpr int kNameLen = 32;

struct Device {
  char name[kNameLen] = {0};
  int rssi = 0;
};

enum class Status { Off, Starting, Stopping, Scanning, Connecting, Connected };

// Loads the saved on/off state and speaker name; starts Bluetooth if it was left on.
void begin();
// Call every loop: periodic debug stats.
void update();

bool active();  // the user's on/off choice
void setActive(bool on);
Status status();
const char* savedName();  // "" = no speaker chosen yet

// Speakers found by the current scan (cleared whenever Bluetooth restarts).
int deviceCount();
Device device(int index);
// Picks a found speaker: remembered, and connected to on the next scan pass (a few seconds).
void selectDevice(int index);
// Disconnects and forgets the saved speaker, then scans again.
void forgetDevice();

// I2S capture health, for the screen and for checking the wiring.
struct Capture {
  uint32_t framesPerSec = 0;  // ~44100 when the Teensy's clocks arrive; 0 = nothing on GPIO34/35
  int peakPermille = 0;       // loudest sample since the last call, 0-1000
};
Capture takeCapture();

}  // namespace btaudio
