#include "StemEngine.h"
#include <cstring>
namespace studio {
const char* validate(const StemSet&s,const IAudioAssets&a){if(s.apiVersion!=1)return "Unsupported Stem Engine API";if(!s.id)return "Missing set ID";for(unsigned i=0;i<4;++i){const auto&x=s.stems[i];if(std::strncmp(x.file,StemFiles[i],sizeof x.file))return "Canonical filename/order mismatch";if(!x.asset||!a.length(x.asset))return "Missing asset";for(unsigned j=0;j<i;++j)if(x.asset==s.stems[j].asset)return "Duplicate asset";if(x.sampleRate!=SampleRate)return "Convert all stems to 44100 Hz";if(x.channels!=2||x.bits!=16)return "Convert to stereo PCM16";if(!x.frames||x.frames!=s.stems[0].frames||a.length(x.asset)!=x.frames)return "Frame count mismatch";if(x.alignmentOffset!=0)return "Alignment offset must be zero";}return nullptr;}
void PreparedStemEngine::submit(const StemSet&s,const IAudioAssets&a){state_={ImportStage::Validating,.9f,"Validating"};if(auto e=validate(s,a)){state_={ImportStage::Failed,0,e};return;}set_=s;state_={ImportStage::Ready,1,"STEM SPLIT COMPLETE"};}
bool StemPlayer::load(const StemSet&s,const IAudioAssets&a){if(validate(s,a))return false;set_=s;cursor_=0;loaded_=true;return true;}
bool StemPlayer::read(IAudioAssets&a,std::array<Stereo,4>&out){out={};if(!loaded_||cursor_>=set_.stems[0].frames)return false;std::array<Stereo,4> staged{};for(unsigned i=0;i<4;++i)if(!a.frame(set_.stems[i].asset,cursor_,staged[i])){++underruns;++cursor_;return false;}out=staged;++cursor_;return true;}
}
