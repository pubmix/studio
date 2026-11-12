#pragma once
#include <stdint.h>

// A software USB-MIDI bridge: the ESP32's own USB-serial port (the same one used for the
// debug log and firmware flashing) carries raw MIDI bytes instead, for a bridge app on the
// connected computer (e.g. Hairless MIDI<->Serial Bridge, 115200 baud, no other settings)
// to turn into a real virtual MIDI port. The classic ESP32 used here has no native USB
// peripheral, so this is the only way to get MIDI in/out of it without different hardware.
//
// Raw MIDI bytes and human-readable debug text can't share one stream, so turning the
// bridge on silences dbg:: logging (see debug_log.h) and stops the "type a line, it's sent
// to the Teensy" debug passthrough (main.cpp's forwardDebugLines()) for as long as it's on.
//
// IN: notes on MIDI channel 10 (the General MIDI drum convention) trigger the matching drum
// row by its standard GM percussion note number (36=kick 38=snare 39=clap 42=closed-hat
// 46=open-hat 45=tom 37=rim 49=perc). Notes on any other channel play the Dub-Box's first
// synth instrument (instrument 0) — there's no per-channel instrument picker yet.
// OUT: the 4 faders are reported as MIDI CC7 (channel volume), one MIDI channel (1-4) per
// fader, whenever a value changes — MIDI-Learn it onto whatever a DAW should follow.
namespace midibridge {

void begin();  // loads the saved on/off state
// Call every loop; a no-op while off.
void update();

bool active();
void setActive(bool on);  // persisted

// For the MIDI screen: the last note message seen, since the debug log is unavailable
// while the bridge is on and there's otherwise no way to confirm it's working.
struct LastEvent {
  bool valid = false;
  uint32_t ms = 0;
  int channel = 0;  // 1-16
  int note = 0;
  int velocity = 0;
  bool noteOn = false;
};
LastEvent lastEvent();

}  // namespace midibridge
