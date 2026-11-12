#pragma once
#include <stdint.h>
#include <stddef.h>
struct Preferences {
 inline static bool available=true, writeOK=true;
 inline static uint8_t stored=0;
 inline static unsigned writes=0;
 bool begin(const char*,bool) { return available; }
 uint8_t getUChar(const char*, uint8_t) { return stored; }
 size_t putUChar(const char*, uint8_t v) { ++writes; if (!writeOK) return 0; stored=v; return 1; }
 void end() {}
};
