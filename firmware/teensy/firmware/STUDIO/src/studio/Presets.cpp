#include "Presets.h"
#include <cstring>
namespace studio {
bool saveSound(IStorage&s,const char*path,const Sound&sound){Project p;std::strcpy(p.name,"STUDIO:SOUND:1");p.tracks[0].sound=sound;return saveProject(s,path,p);}
bool loadSound(IStorage&s,const char*path,Sound&sound){Project p;if(!loadProject(s,path,p)||std::strcmp(p.name,"STUDIO:SOUND:1"))return false;sound=p.tracks[0].sound;return true;}
bool saveFx(IStorage&s,const char*path,const Fx&fx){Project p;std::strcpy(p.name,"STUDIO:FX:1");p.dub[0].fx=fx;return saveProject(s,path,p);}
bool loadFx(IStorage&s,const char*path,Fx&fx){Project p;if(!loadProject(s,path,p)||std::strcmp(p.name,"STUDIO:FX:1"))return false;fx=p.dub[0].fx;return true;}
}
