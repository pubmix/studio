#include <Arduino.h>
#include <SPI.h>
#include "LCD.h"
#include "SSD2828.h"
#include "touch_gt911.h"
#include "ui.h"
#include "teensy_link.h"
#include "wifi_upload.h"
#include "stem_client.h"
#include "screen_mirror.h"

void lcmSetSpiHz(uint32_t hz);

void setup() {
  Serial.setTxBufferSize(4096);
  Serial.begin(921600);
  delay(500);
  Serial.println("Dub-Box display: Dub-Box UI");
  SSD2828_Initial();
  ER5517.Parallel_Init();
  ER5517.HW_Reset();
  ER5517.System_Check_Temp();
  delay(50);
  while (ER5517.LCD_StatusRead() & 0x02) {}
  ER5517.initial();
  ER5517.Display_ON();
  Serial.printf("LT7683 status after init: 0x%02x\n", (unsigned)ER5517.LCD_StatusRead());

  ER5517.Select_Main_Window_16bpp();
  ER5517.Main_Image_Start_Address(layer1_start_addr);
  ER5517.Main_Image_Width(LCD_XSIZE_TFT);
  ER5517.Main_Window_Start_XY(0, 0);
  ER5517.Canvas_Image_Start_address(0);
  ER5517.Canvas_image_width(LCD_XSIZE_TFT);
  ER5517.Active_Window_XY(0, 0);
  ER5517.Active_Window_WH(LCD_XSIZE_TFT, LCD_YSIZE_TFT);

  lcmSetSpiHz(LCM_FAST_SPI_HZ);
  Serial.printf("touch init: %s\n", touchBegin() ? "OK" : "FAILED");
  teensylink::begin();
  mirror::begin();
  wifiup::begin();
  stemclient::begin();
  ui::begin();
}

// Debug: lines typed on the USB serial console are forwarded to the Teensy
// unchanged (e.g. "O,DEMO" or "Q"), so the link can be exercised from a PC.
static void forwardDebugLines() {
  static char buf[192];
  static int len = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      buf[len] = '\0';
      if (strcmp(buf, "!state") == 0) {
        Serial.printf("STATE: heap free %lu minimum %lu largest %lu\n", (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
        const auto& st = teensylink::state();
        Serial.printf("STATE: linked %d project %d %s instruments %d preview %d pan %d,%d,%d,%d\n",
          teensylink::connected(),st.project.open,st.project.name,st.drums.instCount,st.drums.previewOn,
          st.panPermille[0],st.panPermille[1],st.panPermille[2],st.panPermille[3]);
        for(int i=0;i<st.drums.instCount;++i) {
          char patch[128]; modular::format(patch,sizeof(patch),st.instrumentModular[i],st.modularPatch[i]);
          Serial.printf("STATE: instrument %d patch %s notes %d\n",i,patch,st.drums.noteCount[0][i]);
        }
      } else if (len > 3 && strncmp(buf, "!up,", 4) == 0) {
        wifiup::debugUpload(atoi(buf + 4));
      } else if (len > 0) {
        Serial2.print(buf);
        Serial2.print('\n');
      }
      len = 0;
    } else if (c != '\r' && len < (int)sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }
}

void loop() {
  forwardDebugLines();
  ui::update();
  wifiup::update();
}
