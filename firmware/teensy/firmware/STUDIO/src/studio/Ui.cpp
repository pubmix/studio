#include "Ui.h"
#include <cstdio>
namespace studio {
Tokens tokens(Theme t){constexpr Tokens bank[]={{0x15171b,0xf5f5f5,0x55ddaa,0x818890,0xff5555,"sans"},{0xf5f5f0,0x151515,0x006e62,0x707070,0xbb2020,"sans"},{0x170b29,0xffefff,0xff44dd,0x8770aa,0xff4530,"synth"},{0xc0c0c0,0x101010,0x000080,0x666666,0xb00000,"system"},{0xd6f5ff,0x123544,0x007db8,0x567888,0xc53030,"round"},{0x2b251c,0xf7e8bb,0xee9638,0x95876c,0xed5544,"panel"},{0x001100,0x70ff70,0x99ff99,0x408040,0xffaa55,"mono"},{0x182038,0xe8f0c0,0x80d8c0,0x708898,0xff6868,"pixel"}};return bank[unsigned(t)<8?unsigned(t):0];}
const char* effectName(Effect e){constexpr const char* names[]={"STUDIO Dub Echo","Digital Delay","Room Reverb (prototype)","Low-pass Send","High-pass Send","Drive Send"};return unsigned(e)<6?names[unsigned(e)]:"Unavailable";}
void render(const App&a,IDisplay&d){auto t=tokens(a.project.theme);d.clear(t.background);d.label(16,16,"STUDIO",t.text);char line[96];std::snprintf(line,sizeof line,"%.1f BPM  4/4",double(a.project.bpm));d.label(16,44,line,t.muted);
 if(a.session.page==Page::Recovery){d.label(16,90,"Recovered Session Available",t.accent);return;}
 if(a.session.mode==Mode::Home){constexpr const char*n[]={"DUB","JAM","DAW","FILES","SETTINGS"};for(int i=0;i<5;++i)d.label(16+(i%2)*160,90+(i/2)*52,n[i],t.accent);return;}
 if(a.session.mode==Mode::Daw){d.label(16,90,"DAW — workflow awaiting review",t.text);return;}
 if(a.session.mode==Mode::Settings){for(int i=0;i<7;++i)d.label(16,85+i*28,SettingNames[i],t.text);return;}
 if(a.session.mode==Mode::Files){const char*action=a.files.action==FileContext::Action::AssignSlot?"ASSIGN TO SLOT":a.files.action==FileContext::Action::LoadTrack?"LOAD TO TRACK":"BROWSE / PREVIEW";std::snprintf(line,sizeof line,"%s %u",action,a.files.target+1);d.label(16,90,line,t.accent);return;}
 if(a.session.page==Page::Browser){d.label(16,90,a.session.musicMode==Mode::Dub?effectName(Effect(a.session.browserIndex)):"Sound: Sine / Saw / Drums",t.accent);return;}
 if(a.session.page==Page::Design){d.label(16,90,a.session.musicMode==Mode::Dub?"DESIGN FX: Time / Feedback / Tone / Drive / Width / Mapping":"DESIGN SOUND: Oscillator / Envelope / Macro / Drive",t.accent);return;}
 for(unsigned i=0;i<4;++i){int x=16+int(i)*120;if(a.session.musicMode==Mode::Dub){d.label(x,90,StemNames[i],t.text);auto&f=a.project.dub[i].fx;std::snprintf(line,sizeof line,"%s %d%%",f.enabled?"ON":"OFF",int(f.intensity*100));d.label(x,124,line,t.accent);}else{std::snprintf(line,sizeof line,"TRACK %u",i+1);d.label(x,90,line,t.text);}}
 if(a.session.musicMode==Mode::Jam)d.label(16,180,a.project.tracks[a.session.lane].sound.kind==SoundKind::Drum?"DRUM PADS":"KEYBOARD — TWO OCTAVES",t.accent);
}
}
