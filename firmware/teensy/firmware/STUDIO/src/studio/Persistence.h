#pragma once
#include "Hal.h"
namespace studio {
constexpr size_t ProjectBytes=8192;
size_t encodeProject(const Project&,uint8_t*,size_t);
bool decodeProject(const uint8_t*,size_t,Project&);
bool saveProject(IStorage&,const char*,const Project&);
bool loadProject(IStorage&,const char*,Project&);
}
