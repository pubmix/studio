#include "theme.h"
#include "screen_wifi.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "bt_audio.h"
#include "ui_common.h"
#include "wifi_upload.h"

namespace wifiscreen {
namespace {

using namespace ui;

enum class View { Status, Scan, Keys };
View view = View::Status;
bool blockedByBluetooth = false;  // WiFi is held off while Bluetooth is on (see btaudio::setActive)

// ---- Status view ----
constexpr Rect kChoose = {60, 392, 620, 494};
constexpr Rect kForget = {640, 392, 1220, 494};
constexpr int kBarX1 = 60, kBarX2 = 1220, kBarY1 = 600, kBarY2 = 632;

char shownNet[48];
char shownState[40];
char shownAddr[40];
char shownUpload[100];
int shownPct = -1;
bool shownSaved = false;

// ---- Scan view ----
constexpr int kRowTop = 150, kRowH = 102, kRowStride = 112;
constexpr Rect kPrev = {60,610,260,712};
constexpr Rect kNext = {280,610,480,712};
constexpr Rect kRescan = {500,610,700,712};
constexpr Rect kManual = {720,610,960,712};
constexpr Rect kScanBack = {980,610,1220,712};
int scanPage=0;
constexpr int kPageRows=4;
int shownScan = -2;

// ---- Keyboard view ----
constexpr int kMaxPass = 63;
constexpr int kMaxSsid = 32;
constexpr int kUnit = 98, kKeyW = 88, kKeyH = 94, kX0 = 155;
constexpr int kRowY[4] = {208, 308, 408, 508};
constexpr Rect kField = {60, 144, 1220, 196};
constexpr Rect kShift = {kX0, 508, kX0 + 137, 508 + kKeyH};
constexpr Rect kDel = {kX0 + 833, 508, kX0 + 833 + 137, 508 + kKeyH};
constexpr Rect kSym = {kX0, 608, kX0 + 180, 710};
constexpr Rect kSpace = {kX0 + 195, 608, kX0 + 635, 710};
constexpr Rect kCancel = {kX0 + 650, 608, kX0 + 810, 710};
constexpr Rect kOk = {kX0 + 825, 608, kX0 + 970, 710};

const char* const kPages[3][4] = {
    {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"},
    {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"},
    {"!@#$%^&*()", "-_=+[]{}|\\", ";:'\",.<>/?", "`~"},
};
int page = 0;
bool typingSsid = false;  // first the name (manual entry), then the password
char ssidBuf[kMaxSsid + 1];
char passBuf[kMaxPass + 1];

char* field() { return typingSsid ? ssidBuf : passBuf; }
int fieldMax() { return typingSsid ? kMaxSsid : kMaxPass; }

Rect keyRect(int row, int col) {
  int n = static_cast<int>(strlen(kPages[page][row]));
  int x;
  if (row == 0 || row == 1) {
    x = kX0 + (10 - n) * kUnit / 2 + col * kUnit;
  } else if (row == 2) {
    x = kX0 + (10 - n) * kUnit / 2 + col * kUnit;
  } else {
    x = kX0 + 147 + col * kUnit;
  }
  return {x, kRowY[row], x + kKeyW, kRowY[row] + kKeyH};
}

void drawKey(const Rect& r, const char* label, uint32_t fill = theme::palette().surface, uint32_t fg = theme::palette().text) {
  drawButton(r, label, fontLarge(), rgb565(fg), rgb565(fill));
}

void drawField() {
  setCanvas(kVisible);
  fillRect(kField.x1, kField.y1, kField.x2, kField.y2, rgb565(theme::palette().text));
  fillRect(kField.x1 + 3, kField.y1 + 3, kField.x2 - 3, kField.y2 - 3, rgb565(theme::palette().surface));
  const char* text = field();
  const int len = static_cast<int>(strlen(text));
  const int keep = 46;  // the tail that fits
  char shown[keep + 4];
  snprintf(shown, sizeof(shown), "%s_", len > keep ? text + (len - keep) : text);
  drawText(kField.x1 + 16, kField.y1 + 7, shown, fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().surface),
           kField.x2 - kField.x1 - 34);
}

void drawKeys() {
  setCanvas(kVisible);
  fillRect(0, 204, kW - 1, 719, rgb565(theme::palette().background));
  for (int row = 0; row < 4; ++row) {
    const char* keys = kPages[page][row];
    for (int col = 0; keys[col]; ++col) {
      char label[2] = {keys[col], '\0'};
      drawKey(keyRect(row, col), label);
    }
  }
  drawKey(kShift, "SHIFT", page == 1 ? theme::palette().accent : (page == 2 ? theme::palette().surface : theme::palette().surface), page == 2 ? theme::palette().muted : theme::palette().text);
  drawKey(kDel, "DEL", 0x6b3030);
  drawKey(kSym, page == 2 ? "ABC" : "?123", theme::palette().surface);
  drawKey(kSpace, "SPACE");
  drawKey(kCancel, "CANCEL", 0x8a2b2b);
  drawKey(kOk, typingSsid ? "NEXT" : "OK", theme::palette().accent);
}

char keyMessage[128] = {0};
void showMessage(const char* text) {
  if(text != keyMessage) { strncpy(keyMessage,text,sizeof(keyMessage)-1); keyMessage[sizeof(keyMessage)-1]=0; }
  setCanvas(kVisible);
  drawText(60, 106, text, fontSmall(), rgb565(0xff6b6b), rgb565(theme::palette().background), 900);
}

void drawKeysScreen() {
  setCanvas(kVisible);
  fillRect(0, kHeaderH, kW - 1, kH - 1, rgb565(theme::palette().background));
  char label[64];
  if (typingSsid) {
    snprintf(label, sizeof(label), "NETWORK NAME");
  } else {
    snprintf(label, sizeof(label), "PASSWORD FOR %s", ssidBuf);
  }
  drawText(60, 106, label, fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().background), 900);
  drawField();
  drawKeys();
  if(keyMessage[0]) showMessage(keyMessage);
}

void enterKeys(bool ssidFirst, const char* knownSsid) {
  keyMessage[0]=0;
  view = View::Keys;
  typingSsid = ssidFirst;
  page = 0;
  passBuf[0] = '\0';
  if (ssidFirst) {
    ssidBuf[0] = '\0';
  } else {
    strncpy(ssidBuf, knownSsid, kMaxSsid);
    ssidBuf[kMaxSsid] = '\0';
  }
  drawKeysScreen();
}

// ---- Status drawing ----

void drawStatusStatic() {
  setCanvas(kVisible);
  fillRect(0, kHeaderH, kW - 1, kH - 1, rgb565(theme::palette().background));
  drawText(60, 108, "NETWORK", fontSmall(), rgb565(theme::palette().muted), rgb565(theme::palette().background));
  drawText(60, 196, "TO ADD AUDIO, OPEN THIS ADDRESS IN A BROWSER", fontSmall(), rgb565(theme::palette().muted), rgb565(theme::palette().background));
  drawText(60, 540, "UPLOAD", fontSmall(), rgb565(theme::palette().muted), rgb565(theme::palette().background));
  drawButton(kChoose, "CHOOSE NETWORK", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().accent));
  shownNet[0] = shownState[0] = shownAddr[0] = shownUpload[0] = '\0';
  shownPct = -1;
  shownSaved = !wifiup::hasSaved();  // forces the FORGET button to be drawn
}

const char* stateText(uint32_t& color) {
  switch (wifiup::state()) {
    case wifiup::State::Connected:
      color = 0x66bb6a;
      return "CONNECTED";
    case wifiup::State::Connecting:
      color = 0xffca28;
      return "CONNECTING...";
    case wifiup::State::Failed:
      color = 0xff6b6b;
      return wifiup::failReason();
    default:
      color = theme::palette().muted;
      return "NOT SET UP";
  }
}

void refreshStatus() {
  setCanvas(kVisible);
  const uint32_t nowMs = millis();
  (void)nowMs;

  char net[48];
  snprintf(net, sizeof(net), "%s", wifiup::ssid()[0] ? wifiup::ssid() : "-");
  if (strcmp(net, shownNet) != 0) {
    strcpy(shownNet, net);
    drawText(60, 142, net, fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().background), 620);
  }
  uint32_t color = 0;
  const char* st = stateText(color);
  if (strcmp(st, shownState) != 0) {
    strncpy(shownState, st, sizeof(shownState) - 1);
    drawText(700, 142, st, fontLarge(), rgb565(color), rgb565(theme::palette().background), 520);
  }

  char addr[40];
  if (wifiup::address()[0]) {
    snprintf(addr, sizeof(addr), "http://%s", wifiup::address());
  } else {
    snprintf(addr, sizeof(addr), "%s", "(not connected)");
  }
  if (strcmp(addr, shownAddr) != 0) {
    strcpy(shownAddr, addr);
    drawText(60, 232, addr, fontLarge(), rgb565(wifiup::address()[0] ? 0x4dd0e1 : theme::palette().muted), rgb565(theme::palette().background), 700);
    drawText(60, 296, wifiup::address()[0] ? "or   http://dubbox.local" : "", fontSmall(), rgb565(theme::palette().muted),
             rgb565(theme::palette().background), 500);
  }

  if (shownSaved != wifiup::hasSaved()) {
    shownSaved = wifiup::hasSaved();
    drawButton(kForget, "FORGET NETWORK", fontLarge(), rgb565(shownSaved ? theme::palette().text : theme::palette().muted),
               rgb565(shownSaved ? 0x8a2b2b : theme::palette().surface));
  }

  // Upload progress
  const wifiup::Progress p = wifiup::progress();
  char line[100] = "";
  uint32_t lineColor = theme::palette().text;
  int pct = -1;
  switch (p.stage) {
    case wifiup::Stage::Starting:
      snprintf(line, sizeof(line), "STARTING %s ...", p.name);
      pct = 0;
      break;
    case wifiup::Stage::Sending:
      pct = p.size ? static_cast<int>(static_cast<uint64_t>(p.sent) * 100 / p.size) : 0;
      snprintf(line, sizeof(line), "RECEIVING %s   %d%%   %lu / %lu KB", p.name, pct,
               static_cast<unsigned long>(p.sent / 1024), static_cast<unsigned long>(p.size / 1024));
      break;
    case wifiup::Stage::Done:
      snprintf(line, sizeof(line), "SAVED AS %s", p.name);
      lineColor = 0x66bb6a;
      pct = 100;
      break;
    case wifiup::Stage::Failed:
      snprintf(line, sizeof(line), "UPLOAD FAILED: %s", p.error);
      lineColor = 0xff6b6b;
      break;
    default:
      snprintf(line, sizeof(line), "NOTHING YET - SEND A FILE FROM THE BROWSER PAGE");
      lineColor = theme::palette().muted;
      break;
  }
  if (strcmp(line, shownUpload) != 0) {
    strncpy(shownUpload, line, sizeof(shownUpload) - 1);
    drawText(60, 566, line, fontSmall(), rgb565(lineColor), rgb565(theme::palette().background), 1160);
  }
  if (pct != shownPct) {
    shownPct = pct;
    fillRect(kBarX1, kBarY1, kBarX2, kBarY2, rgb565(theme::palette().surface));
    if (pct > 0) {
      fillRect(kBarX1, kBarY1, kBarX1 + (kBarX2 - kBarX1) * pct / 100, kBarY2,
               rgb565(p.stage == wifiup::Stage::Done ? 0x66bb6a : 0x3d7fff));
    }
  }
}

// ---- Scan drawing ----

void drawScanBody() {
  setCanvas(kVisible);
  fillRect(0, kHeaderH, kW - 1, kH - 1, rgb565(theme::palette().background));
  drawText(60, 106, "CHOOSE YOUR NETWORK", fontSmall(), rgb565(theme::palette().muted), rgb565(theme::palette().background));
  drawButton(kRescan, "RESCAN", fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().accent));
  drawButton(kManual, "Type name", fontSmall(), rgb565(theme::palette().text), rgb565(theme::palette().surface));
  drawButton(kScanBack, "BACK", fontLarge(), rgb565(theme::palette().text), rgb565(0x8a2b2b));
  shownScan = -2;
}

void drawScan() { view=View::Scan; scanPage=0; drawScanBody(); }

void refreshScan() {
  const int n = wifiup::scanCount();
  if (n == shownScan) return;
  shownScan = n;
  setCanvas(kVisible);
  fillRect(0, kRowTop, kW - 1, kRowTop + kPageRows * kRowStride, rgb565(theme::palette().background));
  fillRect(60,610,480,712,rgb565(theme::palette().background));
  if(scanPage*kPageRows>=n) scanPage=0;
  if(scanPage>0) drawButton(kPrev,"Previous",fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
  if((scanPage+1)*kPageRows<n) drawButton(kNext,"Next",fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().surface));
  if (n < 0) {
    drawText(60, kRowTop + 20, "SCANNING...", fontLarge(), rgb565(0xffca28), rgb565(theme::palette().background));
    return;
  }
  if (n == 0) {
    drawText(60, kRowTop + 20, "NO NETWORKS FOUND - TRY RESCAN", fontLarge(), rgb565(0xff6b6b), rgb565(theme::palette().background));
    return;
  }
  for (int row=0;row<kPageRows && row+scanPage*kPageRows<n;++row) {
    int i=row+scanPage*kPageRows;
    const int y = kRowTop + row * kRowStride;
    fillRoundRect({60,y,1220,y+kRowH},14,rgb565(theme::palette().surface));
    drawText(80, y + 10, wifiup::scanSsid(i), fontLarge(), rgb565(theme::palette().text), rgb565(theme::palette().surface), 700);
    drawText(900, y + 16, wifiup::scanOpen(i) ? "OPEN" : "SECURED", fontSmall(), rgb565(theme::palette().muted), rgb565(theme::palette().surface), 130);
    const int rssi = wifiup::scanRssi(i);
    const int bars = rssi > -55 ? 4 : (rssi > -67 ? 3 : (rssi > -78 ? 2 : 1));
    for (int b = 0; b < 4; ++b) {
      const int h = 10 + b * 8;
      fillRect(1060 + b * 22, y + 46 - h, 1060 + b * 22 + 14, y + 46, rgb565(b < bars ? 0x66bb6a : theme::palette().surface));
    }
  }
}

void leaveToStatus() {
  view = View::Status;
  drawStatusStatic();
}

void submitField() {
  if (typingSsid) {
    if (ssidBuf[0] == '\0') {
      showMessage("ENTER THE NETWORK NAME");
      return;
    }
    char known[kMaxSsid + 1];
    strcpy(known, ssidBuf);
    enterKeys(false, known);
    return;
  }
  wifiup::connect(ssidBuf, passBuf);
  leaveToStatus();
}

void keysTouch(int x, int y) {
  if (inRect(kCancel, x, y)) {
    leaveToStatus();
    return;
  }
  if (inRect(kOk, x, y)) {
    submitField();
    return;
  }
  if (inRect(kShift, x, y)) {
    if (page != 2) {
      page = page == 1 ? 0 : 1;
      drawKeys();
    }
    return;
  }
  if (inRect(kSym, x, y)) {
    page = page == 2 ? 0 : 2;
    drawKeys();
    return;
  }
  char* text = field();
  int len = static_cast<int>(strlen(text));
  bool changed = false;
  if (inRect(kDel, x, y)) {
    if (len > 0) {
      text[len - 1] = '\0';
      changed = true;
    }
  } else if (inRect(kSpace, x, y)) {
    if (len < fieldMax()) {
      text[len] = ' ';
      text[len + 1] = '\0';
      changed = true;
    }
  } else {
    for (int row = 0; row < 4 && !changed; ++row) {
      const char* keys = kPages[page][row];
      for (int col = 0; keys[col]; ++col) {
        if (inRect(keyRect(row, col), x, y)) {
          if (len < fieldMax()) {
            text[len] = keys[col];
            text[len + 1] = '\0';
          }
          changed = true;
          if (page == 1) {  // shift applies to one letter
            page = 0;
            drawField();
            drawKeys();
            changed = false;
          }
          break;
        }
      }
    }
  }
  if (changed) {
    drawField();
    showMessage("");
  }
}

}  // namespace

void enter() {
  view = View::Status;
  blockedByBluetooth = btaudio::active();
  if (blockedByBluetooth) {
    ui::setCanvas(ui::kVisible);
    ui::fillRect(0, kHeaderH, ui::kW - 1, ui::kH - 1, ui::rgb565(theme::palette().background));
    ui::drawText(60, 120, "WIFI UPLOAD IS OFF WHILE BLUETOOTH IS ON", ui::fontLarge(), ui::rgb565(theme::palette().text),
                 ui::rgb565(theme::palette().background), 1160);
    ui::drawText(60, 190, "Bluetooth and WiFi share one radio and the display's memory, so only one runs at a time.",
                 ui::fontSmall(), ui::rgb565(theme::palette().text), ui::rgb565(theme::palette().background), 1160);
    ui::drawText(60, 226, "To upload over WiFi: open the BLUETOOTH screen and turn Bluetooth off (the display restarts).",
                 ui::fontSmall(), ui::rgb565(theme::palette().text), ui::rgb565(theme::palette().background), 1160);
    return;
  }
  drawStatusStatic();
  refreshStatus();
}

void touchDown(int x, int y) {
  if (blockedByBluetooth) return;
  switch (view) {
    case View::Status:
      if (inRect(kChoose, x, y)) {
        wifiup::startScan();
        drawScan();
      } else if (wifiup::hasSaved() && inRect(kForget, x, y)) {
        wifiup::forget();
      }
      break;
    case View::Scan:
      if (inRect(kScanBack, x, y)) {
        leaveToStatus();
      } else if (inRect(kRescan, x, y)) {
        scanPage=0; shownScan=-2; wifiup::startScan();
      } else if (scanPage>0 && inRect(kPrev,x,y)) {
        --scanPage; shownScan=-2; refreshScan();
      } else if ((scanPage+1)*kPageRows<wifiup::scanCount() && inRect(kNext,x,y)) {
        ++scanPage; shownScan=-2; refreshScan();
      } else if (inRect(kManual, x, y)) {
        enterKeys(true, "");
      } else {
        const int n = wifiup::scanCount();
        for (int row=0;row<kPageRows && row+scanPage*kPageRows<n;++row) {
          int i=row+scanPage*kPageRows;
          const int rowY = kRowTop + row * kRowStride;
          if (y >= rowY && y <= rowY + kRowH && x >= 60 && x <= 1220) {
            if (wifiup::scanOpen(i)) {
              wifiup::connect(wifiup::scanSsid(i), "");
              leaveToStatus();
            } else {
              enterKeys(false, wifiup::scanSsid(i));
            }
            break;
          }
        }
      }
      break;
    case View::Keys:
      keysTouch(x, y);
      break;
  }
}

void update() {
  if (blockedByBluetooth) return;
  if (view == View::Status) {
    refreshStatus();
  } else if (view == View::Scan) {
    refreshScan();
  }
}


void redraw() {
  if(blockedByBluetooth) {
    setCanvas(kVisible);
    drawText(60,120,"WIFI UPLOAD IS OFF WHILE BLUETOOTH IS ON",fontLarge(),rgb565(theme::palette().text),rgb565(theme::palette().background),1160);
    drawText(60,190,"Bluetooth and WiFi share one radio and the display's memory, so only one runs at a time.",fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().background),1160);
    drawText(60,226,"To upload over WiFi: open the BLUETOOTH screen and turn Bluetooth off (the display restarts).",fontSmall(),rgb565(theme::palette().text),rgb565(theme::palette().background),1160);
  } else if(view==View::Status) { drawStatusStatic(); refreshStatus(); }
  else if(view==View::Scan) { drawScanBody(); refreshScan(); }
  else { drawKeysScreen(); }
}

}  // namespace wifiscreen
