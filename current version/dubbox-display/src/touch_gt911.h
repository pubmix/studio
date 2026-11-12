#pragma once
#include <stdint.h>

// I2C: SDA 21, SCL 22. Reset/INT lines below.
constexpr int kTouchSdaPin = 21;
constexpr int kTouchSclPin = 22;
constexpr int kTouchIntPin = 27;
constexpr int kTouchRstPin = 26;

struct TouchPoint {
  uint16_t x;
  uint16_t y;
};

bool touchBegin();
// Returns number of points read (0 if none). Fills up to maxPoints.
int touchRead(TouchPoint* out, int maxPoints);
