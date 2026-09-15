#pragma once
#include "Persistence.h"
namespace studio {
struct IRawStorage {virtual ~IRawStorage()=default;virtual bool readBlob(const char*,uint8_t*,size_t,size_t&)=0;virtual bool writeBlob(const char*,const uint8_t*,size_t)=0;};
// Alternate slots retain the last valid payload across interrupted writes.
// Does not guarantee filesystem integrity if the card loses power mid-metadata update.
class JournalStorage:public IStorage {
 IRawStorage&raw_;
 bool slot(const char*,char,uint8_t*,size_t&,uint32_t&);
public:explicit JournalStorage(IRawStorage&r):raw_(r){}
 bool read(const char*,uint8_t*,size_t,size_t&)override;
 bool atomicWrite(const char*,const uint8_t*,size_t)override;
};
}
