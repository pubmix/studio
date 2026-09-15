#pragma once
#include "Hal.h"
namespace studio {
enum class ImportStage:uint8_t {Idle,Preparing,Copying,Validating,Ready,Failed,Cancelled};
struct StemAsset {uint32_t asset=0,sampleRate=0; uint64_t frames=0; int64_t alignmentOffset=0; uint16_t channels=0,bits=0; char file[32]{};};
struct StemSet {uint32_t apiVersion=1,id=0; char title[64]{}; std::array<StemAsset,4> stems{};};
struct ImportStatus {ImportStage stage=ImportStage::Idle;float progress=0;const char* message="";};
struct IStemEngine {virtual ~IStemEngine()=default;virtual ImportStatus status()const=0;virtual bool prepared(StemSet&)=0;virtual void cancel()=0;};
const char* validate(const StemSet&,const IAudioAssets&);
class PreparedStemEngine:public IStemEngine {StemSet set_{};ImportStatus state_{};public:void submit(const StemSet&,const IAudioAssets&);ImportStatus status()const override{return state_;}bool prepared(StemSet& s)override{if(state_.stage!=ImportStage::Ready)return false;s=set_;return true;}void cancel()override{state_={ImportStage::Cancelled,0,"Cancelled"};}};
class StemPlayer {StemSet set_{};uint64_t cursor_=0;bool loaded_=false;public:uint32_t underruns=0;bool load(const StemSet&,const IAudioAssets&);void rewind(){cursor_=0;}uint64_t position()const{return cursor_;}bool read(IAudioAssets&,std::array<Stereo,4>&);};
}
