#pragma once
#include <vector>
#include <algorithm>
#include <cstring>
inline std::vector<unsigned char> fixture;
class File {
 size_t pos=0; bool opened=false;
public:
 File(bool open=false):opened(open){}
 operator bool() const {return opened;}
 void close(){opened=false;}
 bool available(){return opened && pos<fixture.size();}
 size_t position(){return pos;}
 bool seek(size_t n){if(!opened)return false;pos=n;return true;}
 size_t read(void* dst,size_t n){n=std::min(n,fixture.size()-pos);memcpy(dst,fixture.data()+pos,n);pos+=n;return n;}
};
struct SDClass {File open(const char*){return File(true);}};
inline SDClass SD;
