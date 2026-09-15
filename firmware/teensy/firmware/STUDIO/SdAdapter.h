#pragma once
#include <SD.h>
#include "src/studio/StemEngine.h"
// Foreground-only SDIO adapter. Never read SD from AudioStream::update().
class SdAssets:public studio::IAudioAssets {
 struct Asset {File file;uint32_t id=0,frames=0,offset=0,start=UINT32_MAX;uint16_t valid=0;std::array<uint8_t,2048>cache{};};
 std::array<Asset,16> assets_{};
 static uint16_t u16(const uint8_t*p){return p[0]|uint16_t(p[1])<<8;}static uint32_t u32(const uint8_t*p){return u16(p)|uint32_t(u16(p+2))<<16;}
public:
 bool add(uint32_t id,const char*path,studio::StemAsset*meta=nullptr){Asset* a=nullptr;for(auto&x:assets_)if(x.id==id||!x.id){a=&x;break;}if(!a)return false;File f=SD.open(path);if(!f)return false;uint8_t h[16];if(f.read(h,12)!=12||memcmp(h,"RIFF",4)||memcmp(h+8,"WAVE",4)||uint64_t(u32(h+4))+8!=f.size())return false;bool fmt=false,data=false;uint32_t frames=0,offset=0;
 for(uint32_t p=12;p+8<=f.size();){f.seek(p);if(f.read(h,8)!=8)return false;uint32_t n=u32(h+4);if(uint64_t(p)+8+n>f.size())return false;if(!memcmp(h,"fmt ",4)){if(fmt||n<16||f.read(h,16)!=16||u16(h)!=1||u16(h+2)!=2||u32(h+4)!=44100||u32(h+8)!=176400||u16(h+12)!=4||u16(h+14)!=16)return false;fmt=true;}else if(!memcmp(h,"data",4)){if(data||!n||n%4)return false;frames=n/4;offset=p+8;data=true;}p+=8+n+(n&1);}
 if(!fmt||!data)return false;a->file=f;a->id=id;a->frames=frames;a->offset=offset;a->start=UINT32_MAX;
 if(meta){meta->asset=id;meta->frames=frames;meta->sampleRate=44100;meta->channels=2;meta->bits=16;meta->alignmentOffset=0;const char*name=strrchr(path,'/');strncpy(meta->file,name?name+1:path,sizeof meta->file-1);}return true;}
 uint64_t length(uint32_t id)const override{for(auto&a:assets_)if(a.id==id)return a.frames;return 0;}
 bool frame(uint32_t id,uint64_t p,studio::Stereo&out)override{out={};for(auto&a:assets_)if(a.id==id){if(p>=a.frames)return false;uint32_t start=uint32_t(p/512)*512;if(a.start!=start){a.file.seek(a.offset+start*4);a.valid=a.file.read(a.cache.data(),min(uint32_t(2048),(a.frames-start)*4));a.start=start;}size_t i=(p-start)*4;if(i+4>a.valid)return false;out={int16_t(u16(a.cache.data()+i))/32768.f,int16_t(u16(a.cache.data()+i+2))/32768.f};return true;}return false;}
};
#include "src/studio/Journal.h"
class SdRawStorage:public studio::IRawStorage {
public:
 bool readBlob(const char*path,uint8_t*b,size_t cap,size_t&n)override{char full[224];if(snprintf(full,sizeof full,"STUDIO/%s",path)>=int(sizeof full))return false;File f=SD.open(full);n=0;if(!f||f.size()>cap)return false;n=f.size();return f.read(b,n)==n;}
 bool writeBlob(const char*path,const uint8_t*b,size_t n)override{
  char full[224];if(snprintf(full,sizeof full,"STUDIO/%s",path)>=int(sizeof full))return false;path=full;
  char parent[224];strncpy(parent,path,sizeof parent-1);parent[sizeof parent-1]=0;
  for(char*p=parent;*p;++p)if(*p=='/'){*p=0;if(*parent&&!SD.exists(parent)&&!SD.mkdir(parent))return false;*p='/';}
  File f=SD.open(path,FILE_WRITE_BEGIN);if(!f)return false;
  bool ok=f.truncate()&&f.write(b,n)==n;f.flush();f.close();return ok;
 }
};
