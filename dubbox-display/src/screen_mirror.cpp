#include "screen_mirror.h"
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace mirror {
namespace {
constexpr size_t kCapacity = 65536;
char* queue = nullptr; // Keep all 64 KiB, allocated from the broader heap instead of the small static DRAM segment.
size_t head = 0, tail = 0, used = 0;
uint32_t sequence = 0, dropped = 0, lastFrame = 0;
int layer = 0;
bool active = false;

int layerFor(unsigned long address) { return int(address / 1843200UL); }

void emit(const char* format, ...) {
  if (!active) return;
  flush();
  char line[192];
  int prefix = snprintf(line, sizeof(line), "@V1,%lu,", (unsigned long)sequence++);
  va_list args;
  va_start(args, format);
  int body = vsnprintf(line + prefix, sizeof(line) - prefix, format, args);
  va_end(args);
  if (body < 0 || body >= int(sizeof(line)) - prefix - 12) { ++dropped; return; }
  size_t len = prefix + body;
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < len; ++i) hash = (hash ^ uint8_t(line[i])) * 16777619u;
  len += snprintf(line + len, sizeof(line) - len, "*%08lx\n", (unsigned long)hash);
  if (len > kCapacity - used) { ++dropped; return; }
  for (size_t i = 0; i < len; ++i) { queue[head] = line[i]; head = (head + 1) % kCapacity; }
  used += len;
  flush();
}
}

void flush() {
  // Write only whole records that fit the UART TX buffer. Never wait for the host.
  for (int budget = 0; budget < 4 && used; ++budget) {
    char line[192];
    size_t len = 0;
    do { line[len] = queue[(tail + len) % kCapacity]; ++len; }
    while (len < used && len < sizeof(line) && line[len - 1] != '\n');
    if (line[len - 1] != '\n' || Serial.availableForWrite() < int(len)) return;
    Serial.write(reinterpret_cast<const uint8_t*>(line), len);
    tail = (tail + len) % kCapacity;
    used -= len;
  }
}

void begin() {
  if (!queue) queue = static_cast<char*>(malloc(kCapacity));
  if (!queue) { active = false; Serial.println("screen mirror: 64 KiB allocation failed"); return; }
  head = tail = used = 0;
  sequence = dropped = 0;
  active = true;
  emit("Z,1280,720");
}
void canvas(unsigned long address) { layer = layerFor(address); }
void rect(int x1, int y1, int x2, int y2, uint16_t color) {
  emit("R,%d,%d,%d,%d,%d,%u", layer, x1, y1, x2, y2, color);
}
void glyph(int font, unsigned char ch, int x, int baseline, uint16_t color) {
  emit("G,%d,%d,%u,%d,%d,%u", layer, font, ch, x, baseline, color);
}
void copy(unsigned long src, unsigned long dst, int x, int y, int w, int h) {
  emit("B,%d,%d,%d,%d,%d,%d", layerFor(src), layerFor(dst), x, y, w, h);
}
void frame(int screen) {
  flush();
  if (millis() - lastFrame < 100) return;
  lastFrame = millis();
  emit("F,%lu,%d,%lu", (unsigned long)lastFrame, screen, (unsigned long)dropped);
}
void touch(const char* phase, int x, int y) {
  emit("T,%s,%d,%d,%lu", phase, x, y, (unsigned long)millis());
}
}
