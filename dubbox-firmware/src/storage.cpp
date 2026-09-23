#include "storage.h"

#include <SD.h>
#include <ctype.h>
#include <string.h>

namespace dubbox {

bool initSdCard() { return SD.begin(BUILTIN_SDCARD); }

namespace {

bool hasWavExtension(const char* name) {
  size_t len = strlen(name);
  if (len < 4) return false;
  const char* ext = name + len - 4;
  return ext[0] == '.' && tolower(ext[1]) == 'w' && tolower(ext[2]) == 'a' &&
         tolower(ext[3]) == 'v';
}

}  // namespace

int listWavFiles(char outNames[][kMaxWavFilenameLen]) {
  File dir = SD.open("/");
  if (!dir) return 0;

  int count = 0;
  while (count < kMaxWavFileEntries) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory() && hasWavExtension(entry.name())) {
      strncpy(outNames[count], entry.name(), kMaxWavFilenameLen - 1);
      outNames[count][kMaxWavFilenameLen - 1] = '\0';
      ++count;
    }
    entry.close();
  }
  dir.close();
  return count;
}

}  // namespace dubbox
