#pragma once
#include "studio/Hal.h"
#include "studio/StemEngine.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
namespace studio {
namespace fs=std::filesystem;
struct FileInfo {std::string path;uint64_t bytes;bool directory;};
class HostStorage:public IStorage {
 fs::path root_;std::vector<std::string> recent_;
public:explicit HostStorage(fs::path root);fs::path resolve(const std::string&)const;
 bool read(const char*,uint8_t*,size_t,size_t&)override;bool atomicWrite(const char*,const uint8_t*,size_t)override;
 std::vector<FileInfo> browse(const std::string&folder,const std::string&query="")const;
 bool copy(const std::string&,const std::string&);bool move(const std::string&,const std::string&);bool trash(const std::string&,std::string&trashPath);bool restore(const std::string&,const std::string&);
 const std::vector<std::string>&recents()const{return recent_;}uint64_t freeBytes()const;void touch(const std::string&);
};
class HostAssets:public IAudioAssets {
 struct Wav {std::ifstream file;uint64_t frames=0,offset=0,cacheStart=UINT64_MAX;uint32_t rate=0;uint16_t channels=0,bits=0;std::array<uint8_t,4096> cache{};size_t cacheBytes=0;};
 std::map<uint32_t,Wav> wavs_;std::map<uint32_t,std::vector<Stereo>> memory_;
public:bool addWav(uint32_t,const fs::path&,StemAsset*meta=nullptr);void addMemory(uint32_t,std::vector<Stereo>);uint64_t length(uint32_t)const override;bool frame(uint32_t,uint64_t,Stereo&)override;
 bool preparedFolder(const fs::path&,StemSet&,std::string&error);
};
class TextDisplay:public IDisplay {public:void clear(uint32_t)override;void label(int,int,const char*,uint32_t)override;};
}
