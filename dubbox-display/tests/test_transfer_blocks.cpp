#include "../src/transfer_blocks.h"
#include <zlib.h>
#include <vector>
#include <cassert>
#include <iostream>
#include <fstream>
using Bytes = std::vector<uint8_t>;
bool inflateChecked(const uint8_t* source, size_t size, uint8_t* target, size_t expected) {
  z_stream stream{}; assert(inflateInit(&stream) == Z_OK);
  stream.next_in = const_cast<Bytef*>(source); stream.avail_in = size;
  stream.next_out = target; stream.avail_out = expected;
  int result = inflate(&stream, Z_FINISH);
  bool ok = result == Z_STREAM_END && stream.total_in == size && stream.total_out == expected;
  inflateEnd(&stream); return ok;
}
Bytes encode(const Bytes& input) {
  Bytes result;
  for (size_t offset=0; offset<input.size(); offset+=8192) {
    size_t raw = std::min(size_t(8192), input.size()-offset);
    Bytes block(compressBound(raw)); uLongf size=block.size();
    assert(compress2(block.data(), &size, input.data()+offset, raw, 6)==Z_OK);
    result.insert(result.end(), {uint8_t(size), uint8_t(size>>8), uint8_t(raw), uint8_t(raw>>8)});
    result.insert(result.end(), block.begin(), block.begin()+size);
  }
  return result;
}
bool decode(const Bytes& input, size_t expected, size_t step, Bytes& output) {
  TransferBlocks decoder; decoder.reset(expected);
  auto sink=[&](const uint8_t* p,size_t n){output.insert(output.end(),p,p+n);return true;};
  for(size_t o=0;o<input.size();o+=step)
    if(!decoder.feed(input.data()+o,std::min(step,input.size()-o),inflateChecked,sink))return false;
  return decoder.complete();
}
int main(int argc,char** argv) {
  if(argc==4){
    std::ifstream in(argv[1],std::ios::binary); Bytes bytes((std::istreambuf_iterator<char>(in)),{}), out;
    if(!decode(bytes,std::stoul(argv[2]),97,out))return 1;
    std::ofstream(argv[3],std::ios::binary).write(reinterpret_cast<const char*>(out.data()),out.size());return 0;
  }
  Bytes raw(25000);for(size_t i=0;i<raw.size();++i)raw[i]=(i*i+7*i)%251;
  auto encoded=encode(raw);
  for(size_t step:{1,2,3,4,17,1024,8192,65536}){Bytes out;assert(decode(encoded,raw.size(),step,out));assert(out==raw);}
  for(size_t cut=0;cut<encoded.size();++cut){Bytes out;Bytes shortInput(encoded.begin(),encoded.begin()+cut);assert(!decode(shortInput,raw.size(),97,out));}
  for(size_t pos:{size_t(0),size_t(2),size_t(4),encoded.size()-1}){Bytes corrupt=encoded,out;corrupt[pos]^=0xff;assert(!decode(corrupt,raw.size(),5,out));}
  {Bytes out,extra=encoded;extra.push_back(0);assert(!decode(extra,raw.size(),97,out));}
  {Bytes out;assert(!decode(encoded,raw.size()-1,97,out));}
  {Bytes out;assert(!decode(encoded,raw.size()+1,97,out));}
  {Bytes out;assert(!decode(Bytes{255,255,0,32},8192,1,out));}
  {TransferBlocks d;d.reset(raw.size());assert(!d.feed(encoded.data(),encoded.size(),inflateChecked,[](const uint8_t*,size_t){return false;}));assert(!d.complete());}
  std::cout<<"Transfer block tests passed (fragmentation, every truncation, corruption, bounds, sink failure).\n";
}
