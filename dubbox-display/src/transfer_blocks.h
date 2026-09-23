#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// deflate-blocks-v1: repeated LE uint16 encoded size, uint16 decoded size,
// then a complete zlib stream (including Adler32). No trailing bytes allowed.
// The caller supplies a checked inflater and a backpressured output sink.
class TransferBlocks {
 public:
  static constexpr size_t kBlock = 8192;
  static constexpr size_t kEncoded = kBlock + 64;
  void reset(uint32_t expected) { expected_ = expected; total_ = have_ = need_ = raw_ = 0; bad_ = false; }
  template<class Inflate, class Sink>
  bool feed(const uint8_t* data, size_t size, Inflate inflate, Sink sink) {
    if (bad_) return false;
    while (size) {
      if (total_ == expected_) return reject();
      const size_t target = need_ ? need_ + 4 : 4;
      size_t n = target - have_;
      if (n > size) n = size;
      memcpy(encoded_ + have_, data, n); have_ += n; data += n; size -= n;
      if (have_ == 4 && !need_) {
        need_ = encoded_[0] | (uint16_t(encoded_[1]) << 8);
        raw_ = encoded_[2] | (uint16_t(encoded_[3]) << 8);
        if (!need_ || need_ > kEncoded || !raw_ || raw_ > kBlock || raw_ > expected_ - total_) return reject();
      }
      if (need_ && have_ == need_ + 4) {
        if (!inflate(encoded_ + 4, need_, decoded_, raw_) || !sink(decoded_, raw_)) return reject();
        total_ += raw_; have_ = need_ = raw_ = 0;
      }
    }
    return true;
  }
  bool complete() const { return !bad_ && !have_ && total_ == expected_; }
 private:
  bool reject() { bad_ = true; return false; }
  uint8_t encoded_[kEncoded + 4], decoded_[kBlock];
  uint32_t expected_ = 0, total_ = 0;
  size_t have_ = 0, need_ = 0, raw_ = 0;
  bool bad_ = false;
};
