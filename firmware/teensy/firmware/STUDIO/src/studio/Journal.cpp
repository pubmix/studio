#include "Journal.h"
#include <cstring>
#include <cstdio>
namespace studio {namespace {
uint32_t checksum(const uint8_t*p,size_t n){uint32_t h=2166136261;while(n--)h=(h^*p++)*16777619;return h;}
uint32_t recordHash(const uint8_t*p,size_t n){uint32_t h=checksum(p,12);for(size_t i=16;i<n;++i)h=(h^p[i])*16777619;return h;}
uint32_t get(const uint8_t*p){uint32_t n=0;for(int i=0;i<4;++i)n|=uint32_t(p[i])<<(i*8);return n;}
void put(uint8_t*p,uint32_t n){for(int i=0;i<4;++i)p[i]=uint8_t(n>>(i*8));}
bool path(char*out,size_t n,const char*p,char suffix){if(!p||!*p||*p=='/'||std::strstr(p,".."))return false;return std::snprintf(out,n,"%s.%c",p,suffix)<int(n);}
}
bool JournalStorage::slot(const char*p,char s,uint8_t*b,size_t&n,uint32_t&generation){char name[192];if(!path(name,sizeof name,p,s)||!raw_.readBlob(name,b,ProjectBytes+16,n)||n<16)return false;if(get(b)!=0x314c4e4a||get(b+8)!=n-16||get(b+12)!=recordHash(b,n))return false;generation=get(b+4);return generation!=0;}
bool JournalStorage::read(const char*p,uint8_t*out,size_t cap,size_t&n){std::array<uint8_t,ProjectBytes+16>a{},b{};size_t na=0,nb=0;uint32_t ga=0,gb=0;bool va=slot(p,'a',a.data(),na,ga),vb=slot(p,'b',b.data(),nb,gb);n=0;if(!va&&!vb)return false;bool chooseA=va&&(!vb||ga>=gb);auto&source=chooseA?a:b;n=(chooseA?na:nb)-16;if(n>cap)return false;std::memcpy(out,source.data()+16,n);return true;}
bool JournalStorage::atomicWrite(const char*p,const uint8_t*data,size_t n){if(n>ProjectBytes)return false;std::array<uint8_t,ProjectBytes+16>b{};size_t size=0;uint32_t ga=0,gb=0;bool va=slot(p,'a',b.data(),size,ga),vb=slot(p,'b',b.data(),size,gb);uint32_t generation=(ga>gb?ga:gb)+1;if(!generation)return false;char target=!va?'a':!vb?'b':ga<=gb?'a':'b';char name[192];if(!path(name,sizeof name,p,target))return false;put(b.data(),0x314c4e4a);put(b.data()+4,generation);put(b.data()+8,uint32_t(n));std::memcpy(b.data()+16,data,n);put(b.data()+12,recordHash(b.data(),n+16));if(!raw_.writeBlob(name,b.data(),n+16))return false;uint32_t verified=0;return slot(p,target,b.data(),size,verified)&&verified==generation;}
}
