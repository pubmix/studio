#include "theme.h"
#include "screen_bluetooth.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "bt_audio.h"
#include "ui_common.h"

namespace btscreen {
namespace {

using namespace ui;

constexpr Rect kToggle = {60, 144, 620, 246};
constexpr Rect kForget = {640, 144, 1220, 246};
constexpr int kStatusY = 264;
constexpr int kMeterY = 308;
constexpr int kListHeaderY = 350;
constexpr int kRowTop = 390;
constexpr int kRowH = 102;
constexpr int kRowStride = 112;
constexpr int kMaxRows = 2;

int listPage=0;
constexpr Rect kPrev={60,610,360,712}, kNext={920,610,1220,712};
uint32_t g_listSig = 0;
uint32_t g_statusSig = 0;
uint32_t g_meterSig = 0;
uint32_t g_toggleSig = 0;
uint32_t lastMeterMs = 0;
btaudio::Capture lastCapture{};

Rect rowRect(int i) { return {60, kRowTop + i * kRowStride, 1220, kRowTop + i * kRowStride + kRowH}; }

const char* statusText(btaudio::Status s) {
  switch (s) {
    case btaudio::Status::Off: return "OFF";
    case btaudio::Status::Starting: return "STARTING...";
    case btaudio::Status::Stopping: return "STOPPING...";
    case btaudio::Status::Scanning: return "SCANNING FOR SPEAKERS";
    case btaudio::Status::Connecting: return "CONNECTING...";
    case btaudio::Status::Connected: return "CONNECTED";
  }
  return "";
}

uint32_t hashStr(uint32_t h, const char* s) {
  while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
  return h;
}

void drawToggle() {
  const bool on = btaudio::active();
  setCanvas(kVisible);
  drawButton(kToggle, on ? "BLUETOOTH: ON" : "BLUETOOTH: OFF", fontLarge(), rgb565(on ? theme::palette().background : theme::palette().text),
             rgb565(on ? 0x42a5f5 : theme::palette().surface));
  const bool hasSaved = btaudio::savedName()[0] != '\0';
  drawButton(kForget, "FORGET SPEAKER", fontLarge(), rgb565(hasSaved ? theme::palette().text : theme::palette().muted),
             rgb565(hasSaved ? 0x9c2a2a : 0x2a2a30));
  g_toggleSig = (on ? 1u : 0u) * 2u + (hasSaved ? 1u : 0u);
}

uint32_t toggleSig() {
  return (btaudio::active() ? 1u : 0u) * 2u + (btaudio::savedName()[0] != '\0' ? 1u : 0u);
}

void drawStatus() {
  const btaudio::Status s = btaudio::status();
  char line[120];
  if (s == btaudio::Status::Connected) {
    snprintf(line, sizeof(line), "STATUS: CONNECTED TO %s", btaudio::savedName());
  } else if (s == btaudio::Status::Connecting) {
    snprintf(line, sizeof(line), "STATUS: CONNECTING TO %s...", btaudio::savedName());
  } else if (s == btaudio::Status::Scanning && btaudio::savedName()[0] != '\0') {
    snprintf(line, sizeof(line), "STATUS: LOOKING FOR %s (turn it on / pairing mode)", btaudio::savedName());
  } else {
    snprintf(line, sizeof(line), "STATUS: %s", statusText(s));
  }
  setCanvas(kVisible);
  fillRect(60, kStatusY, 1220, kStatusY + 40, rgb565(theme::palette().background));
  drawText(60, kStatusY, line, fontLarge(), rgb565(s == btaudio::Status::Connected ? 0x66bb6a : theme::palette().text),
           rgb565(theme::palette().background), 1160);
}

uint32_t statusSig() {
  uint32_t h = 2166136261u ^ (uint32_t)btaudio::status();
  return hashStr(h, btaudio::savedName());
}

void drawMeter(const btaudio::Capture& c) {
  lastCapture = c;
  char line[120];
  if (!btaudio::active()) {
    snprintf(line, sizeof(line), "AUDIO IN: (Bluetooth is off)");
  } else if (c.framesPerSec < 1000) {
    snprintf(line, sizeof(line), "AUDIO IN: NO SIGNAL - check Teensy 21/20/7 to ESP32 GPIO34/35/36");
  } else {
    snprintf(line, sizeof(line), "AUDIO IN: %u Hz   LEVEL %d%%", (unsigned)c.framesPerSec, c.peakPermille / 10);
  }
  setCanvas(kVisible);
  fillRect(60, kMeterY, 1220, kMeterY + 36, rgb565(theme::palette().background));
  drawText(60, kMeterY, line, fontSmall(), rgb565(c.framesPerSec < 1000 && btaudio::active() ? 0xff9f3d : theme::palette().muted),
           rgb565(theme::palette().background), 1160);
  g_meterSig = (c.framesPerSec < 1000 ? 0u : 1u) * 1000u + (uint32_t)(c.peakPermille / 20);
}

uint32_t listSig() {
  uint32_t h = 2166136261u ^ (uint32_t)btaudio::status() ^ ((uint32_t)btaudio::deviceCount() << 8);
  for (int i = 0; i < btaudio::deviceCount() ; ++i) h = hashStr(h, btaudio::device(i).name);
  return hashStr(h, btaudio::savedName());
}

void drawList() {
  setCanvas(kVisible);
  fillRect(0, kListHeaderY, kW - 1, kH - 1, rgb565(theme::palette().background));
  const btaudio::Status s = btaudio::status();
  if (s == btaudio::Status::Off || s == btaudio::Status::Stopping) return;
  if (s == btaudio::Status::Connected) {
    drawText(60, kListHeaderY, "Streaming. FORGET SPEAKER to pair a different one.", fontSmall(), rgb565(theme::palette().muted),
             rgb565(theme::palette().background), 1160);
    return;
  }
  drawText(60, kListHeaderY, "NEARBY SPEAKERS - put yours in pairing mode, then tap it", fontSmall(), rgb565(theme::palette().muted),
           rgb565(theme::palette().background), 1160);
  const int n = btaudio::deviceCount();
  if(listPage*kMaxRows>=n) listPage=0;
  if(listPage>0) drawButton(kPrev,"Previous",fontLarge(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
  if((listPage+1)*kMaxRows<n) drawButton(kNext,"Next",fontLarge(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
  for (int i = 0; i+listPage*kMaxRows < n && i < kMaxRows; ++i) {
    const btaudio::Device d = btaudio::device(i+listPage*kMaxRows);
    char label[64];
    snprintf(label, sizeof(label), "%s   (%d dBm)", d.name, d.rssi);
    const bool saved = strcmp(d.name, btaudio::savedName()) == 0;
    drawButton(rowRect(i), label, fontLarge(), rgb565(theme::palette().text), rgb565(saved ? theme::palette().accent : 0x24303f));
  }
  if (n == 0) {
    drawText(60, kRowTop, "(none found yet)", fontSmall(), rgb565(theme::palette().muted), rgb565(theme::palette().background), 600);
  }
}

}  // namespace

void enter() {
  listPage=0;
  setCanvas(kVisible);
  fillRect(0, kHeaderH, kW - 1, kH - 1, rgb565(theme::palette().background));
  drawText(60, 108, "BLUETOOTH AUDIO OUT", fontSmall(), rgb565(theme::palette().muted), rgb565(theme::palette().background));
  drawToggle();
  drawStatus();
  g_statusSig = statusSig();
  drawMeter(btaudio::takeCapture());
  drawList();
  g_listSig = listSig();

}

void touchDown(int x, int y) {
  if (inRect(kToggle, x, y)) {
    btaudio::setActive(!btaudio::active());
    drawToggle();
    return;
  }
  if (inRect(kForget, x, y)) {
    if (btaudio::savedName()[0] != '\0') btaudio::forgetDevice();
    return;
  }
  const btaudio::Status s = btaudio::status();
  if (s == btaudio::Status::Off || s == btaudio::Status::Stopping || s == btaudio::Status::Connected) return;
  if(listPage>0 && inRect(kPrev,x,y)) { --listPage; drawList(); return; }
  if((listPage+1)*kMaxRows<btaudio::deviceCount() && inRect(kNext,x,y)) { ++listPage; drawList(); return; }
  for (int i = 0; i < kMaxRows && i+listPage*kMaxRows < btaudio::deviceCount(); ++i) {
    if (inRect(rowRect(i), x, y)) {
      btaudio::selectDevice(i+listPage*kMaxRows);
      return;
    }
  }
}

void update() {
  if (toggleSig() != g_toggleSig) drawToggle();
  if (statusSig() != g_statusSig) {
    drawStatus();
    g_statusSig = statusSig();
  }
  if (listSig() != g_listSig) {
    drawList();
    g_listSig = listSig();
  }
  const uint32_t now = millis();
  if (now - lastMeterMs >= 500) {
    lastMeterMs = now;
    const btaudio::Capture c = btaudio::takeCapture();
    const uint32_t sig = (c.framesPerSec < 1000 ? 0u : 1u) * 1000u + (uint32_t)(c.peakPermille / 20);
    if (sig != g_meterSig) drawMeter(c);
  }
}


void redraw() {
  setCanvas(kVisible);
  fillRect(0,kHeaderH,kW-1,kH-1,rgb565(theme::palette().background));
  drawText(60,108,"BLUETOOTH AUDIO OUT",fontSmall(),rgb565(theme::palette().muted),rgb565(theme::palette().background));
  drawToggle(); drawStatus(); drawMeter(lastCapture); drawList();
}

}  // namespace btscreen
