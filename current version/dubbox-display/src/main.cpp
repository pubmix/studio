#include <Arduino.h>
#include <SPI.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include "LCD.h"
#include "SSD2828.h"
#include "bt_audio.h"
#include "debug_log.h"
#include "midi_bridge.h"
#include "touch_gt911.h"
#include "ui.h"
#include "screen_mirror.h"
#include "teensy_link.h"
#include "wifi_upload.h"

void lcmSetSpiHz(uint32_t hz);

static const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "CRASH (panic)";
    case ESP_RST_INT_WDT: return "CRASH (interrupt watchdog)";
    case ESP_RST_TASK_WDT: return "CRASH (task watchdog)";
    case ESP_RST_WDT: return "CRASH (watchdog)";
    case ESP_RST_BROWNOUT: return "BROWNOUT (supply voltage dipped)";
    case ESP_RST_EXT: return "reset pin";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    default: return "unknown";
  }
}

void setup() {
  Serial.begin(460800);
  // First, so Bluetooth (if left on) takes its memory before anything else does: it needs a lot.
  btaudio::begin();
  delay(500);
  dbg::printf("RESET REASON: %s\n", resetReasonName(esp_reset_reason()));
  dbg::println("Dub-Box display: Dub-Box UI");
  SSD2828_Initial();
  ER5517.Parallel_Init();
  ER5517.HW_Reset();
  ER5517.System_Check_Temp();
  delay(50);
  while (ER5517.LCD_StatusRead() & 0x02) {}
  ER5517.initial();
  ER5517.Display_ON();
  dbg::printf("LT7683 status after init: 0x%02x\n", (unsigned)ER5517.LCD_StatusRead());

  ER5517.Select_Main_Window_16bpp();
  ER5517.Main_Image_Start_Address(layer1_start_addr);
  ER5517.Main_Image_Width(LCD_XSIZE_TFT);
  ER5517.Main_Window_Start_XY(0, 0);
  ER5517.Canvas_Image_Start_address(0);
  ER5517.Canvas_image_width(LCD_XSIZE_TFT);
  ER5517.Active_Window_XY(0, 0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT, LCD_YSIZE_TFT);

  lcmSetSpiHz(LCM_FAST_SPI_HZ);
  dbg::printf("touch init: %s\n", touchBegin() ? "OK" : "FAILED");
  teensylink::begin();
  wifiup::begin();  // skips connecting while Bluetooth is on
  midibridge::begin();  // may silence dbg:: from here on, if the bridge was left on
  if (!midibridge::active()) mirror::begin();
  ui::begin();
  dbg::printf("HEAP after setup: %u (largest %u)\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

// Debug: lines typed on the USB serial console are forwarded to the Teensy
// unchanged (e.g. "O,DEMO" or "Q"), so the link can be exercised from a PC. Suspended
// while the MIDI bridge owns this same port — raw MIDI bytes would otherwise be read as
// (garbage) debug lines, and vice versa.
static uint32_t pendingSnapshot = 0;
static uint32_t lastSnapshot = 0;
static bool snapshotServed = false;

static void forwardDebugLines() {
  static char buf[64];
  static int len = 0;
  static bool overflow = false;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      buf[len] = '\0';
      if (overflow) {
        overflow = false; // Never forward a truncated console command.
      } else if (strncmp(buf, "!mirror,", 8) == 0) {
        // Consume malformed requests locally as well; never forward them to audio.
        char* end = nullptr;
        errno = 0;
        unsigned long id = strtoul(buf + 8, &end, 10);
        bool digits = len > 8;
        for (int i = 8; i < len; ++i) if (buf[i] < '0' || buf[i] > '9') digits = false;
        if (digits && !errno && end && !*end && id && id <= UINT32_MAX)
          pendingSnapshot = static_cast<uint32_t>(id); // At most one queued request.
      } else if (len > 3 && strncmp(buf, "!up,", 4) == 0) {
        wifiup::debugUpload(atoi(buf + 4));
      } else if (len > 3 && strncmp(buf, "!bt,", 4) == 0) {
        btaudio::setActive(atoi(buf + 4) != 0);
      } else if (len > 0) {
        Serial2.print(buf);
        Serial2.print('\n');
      }
      len = 0;
    } else if (c != '\r') {
      if (len < (int)sizeof(buf) - 1) buf[len++] = c;
      else overflow = true;
    }
  }
}

void loop() {
  if (midibridge::active()) {
    midibridge::update();
  } else {
    forwardDebugLines();
  }
  ui::update();
  if (!midibridge::active() && pendingSnapshot &&
      (!snapshotServed || millis() - lastSnapshot >= 1000)) {
    if (ui::mirrorSnapshot(pendingSnapshot)) {
      pendingSnapshot = 0;
      lastSnapshot = millis();
      snapshotServed = true;
    }
  }
  wifiup::update();
  btaudio::update();
}
