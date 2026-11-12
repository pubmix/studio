#include "midi_bridge.h"

#include <Arduino.h>
#include <Preferences.h>

#include "debug_log.h"
#include "teensy_link.h"

namespace midibridge {
namespace {

Preferences g_prefs;
bool g_active = false;

// General MIDI percussion note numbers, one per drum row, in teensylink::Drums row order
// (KICK SNARE CLAP CL-HAT OP-HAT TOM RIM PERC) — matches the mapping used everywhere else
// this project talks about GM drum notes.
constexpr int8_t kGmDrumNote[teensylink::kDrumRows] = {36, 38, 39, 42, 46, 45, 37, 49};
constexpr int kDrumChannel = 10;   // GM convention: channel 10 is drums
constexpr int kTargetInst = 0;     // notes on any other channel always play the first instrument
constexpr uint32_t kKeepAliveMs = 150;   // matches keyboard.cpp's on-screen-key keepalive
constexpr uint32_t kCcIntervalMs = 40;   // matches synth_panel.cpp's slider send throttle

int rowForNote(int note) {
  for (int r = 0; r < teensylink::kDrumRows; ++r) {
    if (kGmDrumNote[r] == note) return r;
  }
  return -1;
}

// ---- Raw MIDI byte parsing off Serial (running status supported) ----
uint8_t g_status = 0;
int g_dataNeeded = 0;  // data bytes the current status byte takes (-1 = an unhandled system message)
uint8_t g_data[2];
int g_dataGot = 0;

int expectedDataBytes(uint8_t status) {
  const uint8_t hi = status & 0xF0;
  if (hi == 0xC0 || hi == 0xD0) return 1;  // program change / channel pressure
  if (hi >= 0x80 && hi <= 0xE0) return 2;
  return -1;
}

// Notes currently held, needing the fromDisplay keepalive that sendLiveNote()'s protocol
// relies on (see teensy_link.h's `o,<inst>,<midi>,2`) — real MIDI note-off is reliable, but
// the Teensy can't tell a MIDI note apart from a flaky touchscreen one, so it still expects
// refreshes every ~150ms or the note is released after 0.6s.
constexpr int kMaxHeld = 16;
struct Held {
  bool used = false;
  int midi = 0;
};
Held g_held[kMaxHeld];
uint32_t g_lastKeepAliveMs = 0;

void holdNote(int midi) {
  for (int i = 0; i < kMaxHeld; ++i) {
    if (g_held[i].used && g_held[i].midi == midi) return;
  }
  for (int i = 0; i < kMaxHeld; ++i) {
    if (!g_held[i].used) {
      g_held[i].used = true;
      g_held[i].midi = midi;
      return;
    }
  }
}

void releaseNote(int midi) {
  for (int i = 0; i < kMaxHeld; ++i) {
    if (g_held[i].used && g_held[i].midi == midi) g_held[i].used = false;
  }
}

void releaseAllHeld() {
  for (int i = 0; i < kMaxHeld; ++i) g_held[i].used = false;
}

LastEvent g_last;

void handleMessage(uint8_t status, uint8_t d0, uint8_t d1) {
  const uint8_t hi = status & 0xF0;
  if (hi != 0x90 && hi != 0x80) return;  // only Note On / Note Off matter for now

  const int channel = (status & 0x0F) + 1;  // MIDI channels are numbered 1-16 to humans
  const int note = d0;
  const int velocity = d1;
  const bool noteOn = (hi == 0x90) && velocity > 0;  // a Note On with velocity 0 means note-off
  g_last.valid = true;
  g_last.ms = millis();
  g_last.channel = channel;
  g_last.note = note;
  g_last.velocity = velocity;
  g_last.noteOn = noteOn;

  if (channel == kDrumChannel) {
    if (noteOn) {
      int row = rowForNote(note);
      if (row >= 0) teensylink::sendDrumHit(row);
    }
    return;
  }

  if (teensylink::state().drums.instCount <= kTargetInst) return;  // no instrument to play
  teensylink::sendLiveNote(kTargetInst, note, noteOn);
  if (noteOn) {
    holdNote(note);
  } else {
    releaseNote(note);
  }
}

void pumpIncoming() {
  while (Serial.available()) {
    const uint8_t b = static_cast<uint8_t>(Serial.read());
    if (b & 0x80) {  // any byte with the high bit set starts a new status (or is a realtime byte)
      if ((b & 0xF0) == 0xF0) {
        g_dataNeeded = -1;  // system/realtime: no data bytes tracked here, just ignored
        continue;
      }
      g_status = b;
      g_dataNeeded = expectedDataBytes(b);
      g_dataGot = 0;
      continue;
    }
    if (g_dataNeeded <= 0) continue;  // a data byte with no known status yet (or a system message): drop it
    g_data[g_dataGot++] = b;
    if (g_dataGot >= g_dataNeeded) {
      handleMessage(g_status, g_data[0], g_dataNeeded > 1 ? g_data[1] : 0);
      g_dataGot = 0;  // running status: further data pairs reuse g_status until a new one arrives
    }
  }
}

void pumpKeepAlive(uint32_t now) {
  if (now - g_lastKeepAliveMs < kKeepAliveMs) return;
  g_lastKeepAliveMs = now;
  for (int i = 0; i < kMaxHeld; ++i) {
    if (g_held[i].used) teensylink::sendNoteRefresh(kTargetInst, g_held[i].midi);
  }
}

int g_lastFaderCc[teensylink::kNumTracks] = {-1, -1, -1, -1};
uint32_t g_lastCcMs = 0;

void pumpFaderCc(uint32_t now) {
  if (now - g_lastCcMs < kCcIntervalMs) return;
  g_lastCcMs = now;
  for (int i = 0; i < teensylink::kNumTracks; ++i) {
    int cc = teensylink::state().faderPermille[i] * 127 / 1000;
    cc = cc < 0 ? 0 : (cc > 127 ? 127 : cc);
    if (cc == g_lastFaderCc[i]) continue;
    g_lastFaderCc[i] = cc;
    const uint8_t msg[3] = {static_cast<uint8_t>(0xB0 | i), 7, static_cast<uint8_t>(cc)};  // CC7 = channel volume
    Serial.write(msg, 3);
  }
}

}  // namespace

void begin() {
  g_prefs.begin("midi", false);
  g_active = g_prefs.getBool("on", false);
  dbg::setEnabled(!g_active);
}

void setActive(bool on) {
  if (on == g_active) return;
  g_active = on;
  g_prefs.putBool("on", on);
  dbg::setEnabled(!on);
  if (!on) releaseAllHeld();
}

bool active() { return g_active; }

void update() {
  if (!g_active) return;
  const uint32_t now = millis();
  pumpIncoming();
  pumpKeepAlive(now);
  pumpFaderCc(now);
}

LastEvent lastEvent() { return g_last; }

}  // namespace midibridge
