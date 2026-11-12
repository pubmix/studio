#include "theme.h"
#include "screen_midi.h"

#include <Arduino.h>
#include <stdio.h>

#include "midi_bridge.h"
#include "ui_common.h"

namespace midiscreen {
namespace {

using namespace ui;

constexpr Rect kToggle = {60, 144, 620, 246};
constexpr int kEventY = 490;
uint32_t shownEventMs = 0;
bool shownActive = false;

void drawToggle() {
  const bool on = midibridge::active();
  setCanvas(kVisible);
  drawButton(kToggle, on ? "BRIDGE: ON" : "BRIDGE: OFF", fontLarge(), rgb565(on ? theme::palette().background : theme::palette().text),
             rgb565(on ? 0x66bb6a : theme::palette().surface));
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
  fillRect(60, kEventY, 1220, kEventY + 40, rgb565(theme::palette().background));
  drawText(60, kEventY, line, fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().background), 1160);
  shownEventMs = e.ms;
}

}  // namespace

void enter() {
  setCanvas(kVisible);
  fillRect(0, kHeaderH, kW - 1, kH - 1, rgb565(theme::palette().background));
  drawText(60, 108, "USB MIDI BRIDGE", fontSmall(), rgb565(theme::palette().muted), rgb565(theme::palette().background));
  drawToggle();
  drawText(60, 270, "Connect the Dub-Box's USB cable to a computer, run a serial-MIDI bridge app",
           fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().background), 1160);
  drawText(60, 300, "(e.g. Hairless MIDI<->Serial Bridge) at 115200 baud, and pick this device's",
           fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().background), 1160);
  drawText(60, 330, "serial port there. Notes on channel 10 play the drums (GM note numbers);",
           fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().background), 1160);
  drawText(60, 360, "any other channel plays SYN 1. The 4 faders send out as MIDI CC7 on",
           fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().background), 1160);
  drawText(60, 390, "channels 1-4 for a DAW to MIDI-Learn.", fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().background), 1160);
  drawText(60, 434, "The USB debug log is silenced while the bridge is ON.", fontSmall(), rgb565(0xff9f3d),
           rgb565(theme::palette().background), 1160);
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


void redraw() {
  enter();
}

}  // namespace midiscreen
