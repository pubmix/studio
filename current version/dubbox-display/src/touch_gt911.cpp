#include "touch_gt911.h"
#include <Arduino.h>
#include <Wire.h>
#include "touch_gt911_cfg.h"

static uint8_t gtAddr = 0x5D;

static bool writeRegs(uint16_t reg, const uint8_t* data, uint16_t len) {
  Wire.beginTransmission(gtAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  for (uint16_t i = 0; i < len; ++i) Wire.write(data[i]);
  return Wire.endTransmission() == 0;
}

static bool readRegs(uint16_t reg, uint8_t* data, uint8_t len) {
  Wire.beginTransmission(gtAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(gtAddr, len) != len) return false;
  for (uint8_t i = 0; i < len; ++i) data[i] = Wire.read();
  return true;
}

bool touchBegin() {
  Wire.begin(kTouchSdaPin, kTouchSclPin);
  Wire.setClock(100000);

  // INT low while RST rises selects I2C address 0x5D.
  pinMode(kTouchRstPin, OUTPUT);
  pinMode(kTouchIntPin, OUTPUT);
  digitalWrite(kTouchRstPin, LOW);
  digitalWrite(kTouchIntPin, LOW);
  delay(20);
  digitalWrite(kTouchRstPin, HIGH);
  delay(50);
  pinMode(kTouchIntPin, INPUT);
  delay(100);

  for (int i = 0; i < 5; ++i) {
    writeRegs(0x8047, kGt911Config, sizeof(kGt911Config));
    delay(10);
  }
  uint8_t chk[5] = {0};
  if (!readRegs(0x8047, chk, 5)) return false;
  // config bytes 2..3 = X max low/high (0x02D0 = 720 low byte 0xD0, high 0x02)
  return chk[1] == 0xD0 && chk[2] == 0x02;
}

int touchRead(TouchPoint* out, int maxPoints) {
  uint8_t status = 0;
  if (!readRegs(0x814E, &status, 1)) return 0;
  int n = status & 0x0F;
  if (!(status & 0x80) || n == 0) {
    if (status & 0x80) {
      uint8_t zero = 0;
      writeRegs(0x814E, &zero, 1);
    }
    return 0;
  }
  if (n > maxPoints) n = maxPoints;
  if (n > 5) n = 5;
  uint8_t buf[40];
  readRegs(0x8150, buf, n * 8 > 32 ? 32 : n * 8);
  if (n > 4) readRegs(0x8150 + 32, buf + 32, 8);
  for (int i = 0; i < n; ++i) {
    out[i].x = (uint16_t)(buf[i * 8 + 1] << 8 | buf[i * 8 + 0]);
    out[i].y = (uint16_t)(buf[i * 8 + 3] << 8 | buf[i * 8 + 2]);
  }
  uint8_t zero = 0;
  writeRegs(0x814E, &zero, 1);
  return n;
}
