#include "screen_midi.h"

#include <Arduino.h>
#include <stdio.h>

#include "midi_bridge.h"
#include "ui_common.h"

namespace midiscreen {
namespace {

using namespace ui;

constexpr Rect kToggle = {60, 104, 620, 184};
constexpr int kEventY = 420;
uint32_t shownEventMs = 0;
bool shownActive = false;

void drawToggle() {
  const bool on = midibridge::active();
  setCanvas(kVisible);
  drawButton(kToggle, on ? "BRIDGE: ON" : "BRIDGE: OFF", fontLarge(), rgb565(on ? 0x000000 : 0xffffff),
             rgb565(on ? 0x66bb6a : 0x3a3a48));
  shownActive = on;
}

void drawEvent() {
  const midibridge::LastEvent e = midibridge::lastEvent();
  char line[100];
  if (!e.valid) {
    snprintf(line, sizeof(line), "NOTHING RECEIVED YET");
  } else {
    snprintf(line, sizeof(line), "LAST: CH %d  NOTE %d  VEL %d  (%s)  %lus ago", e.channel, e.note, e.velocity,
             e.noteOn ? "ON" : "OFF", static_cast<unsigned long>((millis() - e.ms) / 1000));
  }
  setCanvas(kVisible);
  fillRect(60, kEventY, 1220, kEventY + 40, rgb565(0x000000));
  drawText(60, kEventY, line, fontLarge(), rgb565(0xffffff), rgb565(0x000000), 1160);
  shownEventMs = e.ms;
}

}  // namespace

void enter() {
  setCanvas(kVisible);
  fillRect(0, 61, kW - 1, kH - 1, rgb565(0x000000));
  drawText(60, 68, "USB MIDI BRIDGE", fontSmall(), rgb565(0x9aa3b5), rgb565(0x000000));
  drawToggle();
  drawText(60, 220, "Connect the Dub-Box's USB cable to a computer, run a serial-MIDI bridge app",
           fontSmall(), rgb565(0xdddddd), rgb565(0x000000), 1160);
  drawText(60, 250, "(e.g. Hairless MIDI<->Serial Bridge) at 115200 baud, and pick this device's",
           fontSmall(), rgb565(0xdddddd), rgb565(0x000000), 1160);
  drawText(60, 280, "serial port there. Notes on channel 10 play the drums (GM note numbers);",
           fontSmall(), rgb565(0xdddddd), rgb565(0x000000), 1160);
  drawText(60, 310, "any other channel plays SYN 1. The 4 faders send out as MIDI CC7 on",
           fontSmall(), rgb565(0xdddddd), rgb565(0x000000), 1160);
  drawText(60, 340, "channels 1-4 for a DAW to MIDI-Learn.", fontSmall(), rgb565(0xdddddd), rgb565(0x000000), 1160);
  drawText(60, 384, "The USB debug log is silenced while the bridge is ON.", fontSmall(), rgb565(0xff9f3d),
           rgb565(0x000000), 1160);
  drawEvent();
}

void touchDown(int x, int y) {
  if (inRect(kToggle, x, y)) {
    midibridge::setActive(!midibridge::active());
    drawToggle();
  }
}

void update() {
  if (midibridge::active() != shownActive) drawToggle();
  if (midibridge::lastEvent().ms != shownEventMs) drawEvent();
}

}  // namespace midiscreen
