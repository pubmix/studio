#include "screen_wifi.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "ui_common.h"
#include "wifi_upload.h"

namespace wifiscreen {
namespace {

using namespace ui;

enum class View { Status, Scan, Keys };
View view = View::Status;

// ---- Status view ----
constexpr Rect kChoose = {60, 392, 470, 472};
constexpr Rect kForget = {490, 392, 900, 472};
constexpr int kBarX1 = 60, kBarX2 = 1220, kBarY1 = 600, kBarY2 = 632;

char shownNet[48];
char shownState[40];
char shownAddr[40];
char shownUpload[100];
int shownPct = -1;
bool shownSaved = false;

// ---- Scan view ----
constexpr int kRowTop = 92, kRowH = 58, kRowStride = 64;
constexpr Rect kRescan = {60, 620, 360, 696};
constexpr Rect kManual = {380, 620, 760, 696};
constexpr Rect kScanBack = {780, 620, 1080, 696};
int shownScan = -2;

// ---- Keyboard view ----
constexpr int kMaxPass = 63;
constexpr int kMaxSsid = 32;
constexpr int kUnit = 98, kKeyW = 88, kKeyH = 72, kX0 = 155;
constexpr int kRowY[4] = {204, 284, 364, 444};
constexpr Rect kField = {60, 100, 1220, 164};
constexpr Rect kShift = {kX0, 444, kX0 + 137, 444 + kKeyH};
constexpr Rect kDel = {kX0 + 833, 444, kX0 + 833 + 137, 444 + kKeyH};
constexpr Rect kSym = {kX0, 532, kX0 + 180, 604};
constexpr Rect kSpace = {kX0 + 195, 532, kX0 + 635, 604};
constexpr Rect kCancel = {kX0 + 650, 532, kX0 + 810, 604};
constexpr Rect kOk = {kX0 + 825, 532, kX0 + 970, 604};

const char* const kPages[3][4] = {
    {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"},
    {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"},
    {"!@#$%^&*()", "-_=+[]{}|\\", ";:'\",.<>/?", "`~"},
};
int page = 0;
bool typingSsid = false;  // first the name (manual entry), then the password
char ssidBuf[kMaxSsid + 1];
char passBuf[kMaxPass + 1];
int fieldLen = 0;

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

void drawKey(const Rect& r, const char* label, uint32_t fill = 0x333333, uint32_t fg = 0xffffff) {
  drawButton(r, label, fontLarge(), rgb565(fg), rgb565(fill));
}

void drawField() {
  setCanvas(kVisible);
  fillRect(kField.x1, kField.y1, kField.x2, kField.y2, rgb565(0xffffff));
  fillRect(kField.x1 + 3, kField.y1 + 3, kField.x2 - 3, kField.y2 - 3, rgb565(0x111111));
  const char* text = field();
  const int len = static_cast<int>(strlen(text));
  const int keep = 46;  // the tail that fits
  char shown[keep + 4];
  snprintf(shown, sizeof(shown), "%s_", len > keep ? text + (len - keep) : text);
  drawText(kField.x1 + 16, kField.y1 + 14, shown, fontLarge(), rgb565(0xffffff), rgb565(0x111111),
           kField.x2 - kField.x1 - 34);
}

void drawKeys() {
  setCanvas(kVisible);
  fillRect(0, 196, kW - 1, 616, rgb565(0x000000));
  for (int row = 0; row < 4; ++row) {
    const char* keys = kPages[page][row];
    for (int col = 0; keys[col]; ++col) {
      char label[2] = {keys[col], '\0'};
      drawKey(keyRect(row, col), label);
    }
  }
  drawKey(kShift, "SHIFT", page == 1 ? 0x1c4f9c : (page == 2 ? 0x222222 : 0x3a3a48), page == 2 ? 0x777777 : 0xffffff);
  drawKey(kDel, "DEL", 0x6b3030);
  drawKey(kSym, page == 2 ? "ABC" : "?123", 0x3a3a48);
  drawKey(kSpace, "SPACE");
  drawKey(kCancel, "CANCEL", 0x8a2b2b);
  drawKey(kOk, typingSsid ? "NEXT" : "OK", 0x1f7a3d);
}

void showMessage(const char* text) {
  setCanvas(kVisible);
  drawText(60, 170, text, fontSmall(), rgb565(0xff6b6b), rgb565(0x000000), 900);
}

void enterKeys(bool ssidFirst, const char* knownSsid) {
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
  setCanvas(kVisible);
  fillRect(0, 61, kW - 1, kH - 1, rgb565(0x000000));
  char label[64];
  if (ssidFirst) {
    snprintf(label, sizeof(label), "NETWORK NAME");
  } else {
    snprintf(label, sizeof(label), "PASSWORD FOR %s", ssidBuf);
  }
  drawText(60, 68, label, fontSmall(), rgb565(0xdddddd), rgb565(0x000000), 900);
  drawField();
  drawKeys();
}

// ---- Status drawing ----

void drawStatusStatic() {
  setCanvas(kVisible);
  fillRect(0, 61, kW - 1, kH - 1, rgb565(0x000000));
  drawText(60, 78, "NETWORK", fontSmall(), rgb565(0x9aa3b5), rgb565(0x000000));
  drawText(60, 196, "TO ADD AUDIO, OPEN THIS ADDRESS IN A BROWSER", fontSmall(), rgb565(0x9aa3b5), rgb565(0x000000));
  drawText(60, 540, "UPLOAD", fontSmall(), rgb565(0x9aa3b5), rgb565(0x000000));
  drawButton(kChoose, "CHOOSE NETWORK", fontLarge(), rgb565(0xffffff), rgb565(0x1c4f9c));
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
      color = 0x9aa3b5;
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
    drawText(60, 104, net, fontLarge(), rgb565(0xffffff), rgb565(0x000000), 620);
  }
  uint32_t color = 0;
  const char* st = stateText(color);
  if (strcmp(st, shownState) != 0) {
    strncpy(shownState, st, sizeof(shownState) - 1);
    drawText(700, 104, st, fontLarge(), rgb565(color), rgb565(0x000000), 520);
  }

  char addr[40];
  if (wifiup::address()[0]) {
    snprintf(addr, sizeof(addr), "http://%s", wifiup::address());
  } else {
    snprintf(addr, sizeof(addr), "%s", "(not connected)");
  }
  if (strcmp(addr, shownAddr) != 0) {
    strcpy(shownAddr, addr);
    drawText(60, 232, addr, fontLarge(), rgb565(wifiup::address()[0] ? 0x4dd0e1 : 0x777777), rgb565(0x000000), 700);
    drawText(60, 296, wifiup::address()[0] ? "or   http://dubbox.local" : "", fontSmall(), rgb565(0x9aa3b5),
             rgb565(0x000000), 500);
  }

  if (shownSaved != wifiup::hasSaved()) {
    shownSaved = wifiup::hasSaved();
    drawButton(kForget, "FORGET NETWORK", fontLarge(), rgb565(shownSaved ? 0xffffff : 0x666666),
               rgb565(shownSaved ? 0x8a2b2b : 0x222222));
  }

  // Upload progress
  const wifiup::Progress p = wifiup::progress();
  char line[100] = "";
  uint32_t lineColor = 0xdddddd;
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
      lineColor = 0x777777;
      break;
  }
  if (strcmp(line, shownUpload) != 0) {
    strncpy(shownUpload, line, sizeof(shownUpload) - 1);
    drawText(60, 566, line, fontSmall(), rgb565(lineColor), rgb565(0x000000), 1160);
  }
  if (pct != shownPct) {
    shownPct = pct;
    fillRect(kBarX1, kBarY1, kBarX2, kBarY2, rgb565(0x2a2a33));
    if (pct > 0) {
      fillRect(kBarX1, kBarY1, kBarX1 + (kBarX2 - kBarX1) * pct / 100, kBarY2,
               rgb565(p.stage == wifiup::Stage::Done ? 0x66bb6a : 0x3d7fff));
    }
  }
}

// ---- Scan drawing ----

void drawScan() {
  view = View::Scan;
  setCanvas(kVisible);
  fillRect(0, 61, kW - 1, kH - 1, rgb565(0x000000));
  drawText(60, 68, "CHOOSE YOUR NETWORK", fontSmall(), rgb565(0x9aa3b5), rgb565(0x000000));
  drawButton(kRescan, "RESCAN", fontLarge(), rgb565(0xffffff), rgb565(0x1c4f9c));
  drawButton(kManual, "TYPE THE NAME", fontLarge(), rgb565(0xffffff), rgb565(0x3a3a48));
  drawButton(kScanBack, "BACK", fontLarge(), rgb565(0xffffff), rgb565(0x8a2b2b));
  shownScan = -2;
}

void refreshScan() {
  const int n = wifiup::scanCount();
  if (n == shownScan) return;
  shownScan = n;
  setCanvas(kVisible);
  fillRect(0, kRowTop, kW - 1, kRowTop + 8 * kRowStride, rgb565(0x000000));
  if (n < 0) {
    drawText(60, kRowTop + 20, "SCANNING...", fontLarge(), rgb565(0xffca28), rgb565(0x000000));
    return;
  }
  if (n == 0) {
    drawText(60, kRowTop + 20, "NO NETWORKS FOUND - TRY RESCAN", fontLarge(), rgb565(0xff6b6b), rgb565(0x000000));
    return;
  }
  for (int i = 0; i < n; ++i) {
    const int y = kRowTop + i * kRowStride;
    fillRect(60, y, 1220, y + kRowH, rgb565(0x1d2330));
    drawText(80, y + 10, wifiup::scanSsid(i), fontLarge(), rgb565(0xffffff), rgb565(0x1d2330), 700);
    drawText(900, y + 16, wifiup::scanOpen(i) ? "OPEN" : "SECURED", fontSmall(), rgb565(0x9aa3b5), rgb565(0x1d2330), 130);
    const int rssi = wifiup::scanRssi(i);
    const int bars = rssi > -55 ? 4 : (rssi > -67 ? 3 : (rssi > -78 ? 2 : 1));
    for (int b = 0; b < 4; ++b) {
      const int h = 10 + b * 8;
      fillRect(1060 + b * 22, y + 46 - h, 1060 + b * 22 + 14, y + 46, rgb565(b < bars ? 0x66bb6a : 0x3a3a48));
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
  drawStatusStatic();
  refreshStatus();
}

void touchDown(int x, int y) {
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
        wifiup::startScan();
      } else if (inRect(kManual, x, y)) {
        enterKeys(true, "");
      } else {
        const int n = wifiup::scanCount();
        for (int i = 0; i < n; ++i) {
          const int rowY = kRowTop + i * kRowStride;
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
  if (view == View::Status) {
    refreshStatus();
  } else if (view == View::Scan) {
    refreshScan();
  }
}

}  // namespace wifiscreen
