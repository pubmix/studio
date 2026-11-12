#pragma once
#include <stdint.h>

// Home-WiFi connection plus a small web server for adding audio to the Dub-Box: a browser page
// (upload_page.h) sends WAV files, which are relayed over the UART link to the Teensy's SD card.
namespace wifiup {

enum class State { Off, Connecting, Connected, Failed };

// Loads the saved network (if any) and starts connecting.
void begin();
// Call every main loop: connection watch, and relaying an upload to the Teensy.
void update();

State state();
const char* ssid();      // the network being used (empty if none)
const char* address();   // "192.168.1.23" once connected, else ""
const char* failReason();  // short text when state() == Failed
bool hasSaved();

// Joins a network; it is remembered once the connection succeeds.
void connect(const char* ssid, const char* password);
void forget();

// Network scan (asynchronous): startScan(), then poll scanCount() until it is >= 0.
constexpr int kMaxScan = 8;
void startScan();
int scanCount();  // -1 while scanning
const char* scanSsid(int i);
int scanRssi(int i);
bool scanOpen(int i);

// The upload in progress (or the last one).
enum class Stage { Idle, Starting, Sending, Done, Failed };
struct Progress {
  Stage stage = Stage::Idle;
  uint32_t size = 0;
  uint32_t sent = 0;  // bytes the Teensy has confirmed
  char name[48] = {0};
  char error[24] = {0};
};
Progress progress();

// Test hook (USB serial "!up,<seconds>"): uploads a generated tone as a stereo WAV of that length,
// exercising the whole path to the Teensy's SD card without needing a browser.
void debugUpload(int seconds);

}  // namespace wifiup
