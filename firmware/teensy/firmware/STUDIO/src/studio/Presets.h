#pragma once
#include "Persistence.h"
namespace studio {
// Prototype presets use the same checksummed portable envelope as projects.
// The name tag rejects loading a Sound as an FX preset, or vice versa.
bool saveSound(IStorage&,const char*,const Sound&);bool loadSound(IStorage&,const char*,Sound&);
bool saveFx(IStorage&,const char*,const Fx&);bool loadFx(IStorage&,const char*,Fx&);
}
