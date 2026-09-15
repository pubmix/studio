#pragma once
#include "App.h"
namespace studio {
struct Tokens {uint32_t background,text,accent,muted,danger;const char*font;};
Tokens tokens(Theme);
const char* effectName(Effect);
void render(const App&,IDisplay&);
constexpr const char* SettingNames[]={"Audio","MIDI","Storage","Display","Controls","System","About"};
constexpr const char* FactorySamples[]={"Kick","Snare","Hi-Hat","Clap","Guitar Skank","Piano Clash","Airhorn"};
struct SoundPak {uint32_t id=0;char name[48]{};std::array<uint32_t,32> sounds{},samples{},fxPresets{};};
}
