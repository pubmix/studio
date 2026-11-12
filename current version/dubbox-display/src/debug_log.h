#pragma once

// A thin wrapper around the USB-facing Serial port for human-readable debug logging.
// The MIDI bridge (midi_bridge.*) needs that same port to itself while active — raw MIDI
// bytes and printf text can't share one stream — so every debug print in the project goes
// through here instead of calling Serial directly, and setEnabled(false) makes them all
// silently no-op for as long as the bridge is running.
namespace dbg {

void setEnabled(bool on);

void println(const char* s);
void printf(const char* fmt, ...);

}  // namespace dbg
