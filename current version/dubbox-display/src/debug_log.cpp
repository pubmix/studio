#include "debug_log.h"

#include <Arduino.h>
#include <stdarg.h>

namespace dbg {
namespace {
bool g_enabled = true;
}  // namespace

void setEnabled(bool on) { g_enabled = on; }

void println(const char* s) {
  if (g_enabled) Serial.println(s);
}

void printf(const char* fmt, ...) {
  if (!g_enabled) return;
  char buf[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
}

}  // namespace dbg
