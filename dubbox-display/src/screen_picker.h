#pragma once
#include "ui_common.h"

// Picks a WAV file from the SD card for one track.
namespace picker {

enum class Action { None, Picked, Remove };

// Sets which track the picker is for (no drawing).
void setTrack(int track);
void enter(int track);
// Redraws after a fresh file list arrives.
void refresh();
Action touchDown(int x, int y);
int track();
// The chosen file name after Action::Picked.
const char* chosen();

}  // namespace picker
