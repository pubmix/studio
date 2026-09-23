#include "waveform.h"

#include <SD.h>
#include <string.h>

namespace dubbox {

namespace {

// Reads exactly `len` bytes into `buf`, returning false at EOF/short read
// — WAV parsing can't proceed on a truncated chunk.
bool readExact(File& f, uint8_t* buf, size_t len) {
  int got = f.read(buf, len);
  return got == static_cast<int>(len);
}

uint32_t le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t le16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

// An opened WAV file, positioned at the start of its "data" chunk, plus the format
// info the scanners need. Only 16-bit PCM is understood; anything else fails to open.
struct WavStream {
  File file;
  uint16_t numChannels = 1;
  uint32_t sampleRate = 44100;
  uint32_t dataSize = 0;
};

FLASHMEM bool openWavData(const char* filename, WavStream& out) {
  out.file = SD.open(filename, FILE_READ);
  if (!out.file) return false;

  uint8_t riff[12];
  if (!readExact(out.file, riff, sizeof(riff)) ||
      memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
    out.file.close();
    return false;
  }

  uint16_t bitsPerSample = 16;
  bool foundFmt = false;
  bool foundData = false;

  // Walk RIFF sub-chunks until "data" (the audio payload) is found, or the
  // file runs out. Chunks are padded to an even byte count per the RIFF
  // spec, so unknown/skipped chunks account for that.
  while (!foundData) {
    uint8_t chunkHeader[8];
    if (!readExact(out.file, chunkHeader, sizeof(chunkHeader))) break;
    uint32_t chunkSize = le32(chunkHeader + 4);

    if (memcmp(chunkHeader, "fmt ", 4) == 0) {
      uint8_t fmt[16];
      uint32_t toRead = (chunkSize < sizeof(fmt)) ? chunkSize : sizeof(fmt);
      if (!readExact(out.file, fmt, toRead)) break;
      out.numChannels = le16(fmt + 2);
      out.sampleRate = le32(fmt + 4);
      bitsPerSample = le16(fmt + 14);
      uint32_t skip = chunkSize - toRead + (chunkSize % 2);
      if (skip > 0) out.file.seek(out.file.position() + skip);
      foundFmt = true;
    } else if (memcmp(chunkHeader, "data", 4) == 0) {
      out.dataSize = chunkSize;
      foundData = true;
    } else {
      uint32_t skip = chunkSize + (chunkSize % 2);
      out.file.seek(out.file.position() + skip);
    }
  }

  if (!foundFmt || !foundData || bitsPerSample != 16 || out.numChannels == 0 ||
      out.sampleRate == 0 || out.dataSize == 0) {
    out.file.close();
    return false;
  }
  return true;
}

constexpr size_t kReadBufBytes = 512;

}  // namespace

FLASHMEM bool PeakScanner::start(const char* filename, uint8_t* out, int columns) {
  if (active_) file_.close();
  active_ = false;
  ok_ = false;
  WavStream wav;
  if (!openWavData(filename, wav)) return false;
  file_ = wav.file;
  frameSize_ = 2 * wav.numChannels;
  totalFrames_ = wav.dataSize / frameSize_;
  dataStart_ = file_.position();
  out_ = out;
  columns_ = columns;
  column_ = 0;
  if (totalFrames_ < static_cast<uint32_t>(columns) * 4) {
    file_.close();
    return false;
  }
  memset(out_, 0, columns_);
  active_ = true;
  return true;
}

bool PeakScanner::step(uint32_t budgetUs) {
  if (!active_) return true;
  uint32_t startUs = micros();
  uint8_t buf[kProbeBytes];
  const uint32_t probeFrames = kProbeBytes / frameSize_;
  const uint32_t colFrames = totalFrames_ / columns_;

  while (column_ < columns_) {
    uint32_t colStart = static_cast<uint32_t>(
        (static_cast<uint64_t>(totalFrames_) * column_) / columns_);
    int32_t peak = 0;
    for (int p = 0; p < kProbesPerColumn; ++p) {
      uint32_t frame = colStart + (colFrames * p) / kProbesPerColumn;
      if (frame + probeFrames > totalFrames_) frame = totalFrames_ - probeFrames;
      file_.seek(dataStart_ + frame * frameSize_);
      int got = file_.read(buf, probeFrames * frameSize_);
      for (int off = 0; off + 1 < got; off += frameSize_) {
        int32_t s = static_cast<int16_t>(buf[off] | (buf[off + 1] << 8));
        if (s < 0) s = -s;
        if (s > peak) peak = s;
      }
    }
    int32_t v = peak >> 7;
    out_[column_] = static_cast<uint8_t>(v > 255 ? 255 : v);
    ++column_;
    if (micros() - startUs >= budgetUs) break;
  }

  if (column_ >= columns_) {
    file_.close();
    active_ = false;
    ok_ = true;
    return true;
  }
  return false;
}

namespace {
constexpr int kBeatHop = 2048;            // samples per onset frame (~46 ms at 44.1 kHz)
constexpr int kBeatMaxFrames = 4000;      // ~186 s analysed
// Shared scratch for beat and tempo analysis (never used at the same time). RAM2 is
// nearly full: the heap that LVGL allocates its screens from is what is left over.
DMAMEM static float g_analysisBuf[kBeatMaxFrames];
static float* const g_beatFlux = g_analysisBuf;

inline float fluxMax3(int i, int n) {
  float m = g_beatFlux[i];
  if (i > 0 && g_beatFlux[i - 1] > m) m = g_beatFlux[i - 1];
  if (i + 1 < n && g_beatFlux[i + 1] > m) m = g_beatFlux[i + 1];
  return m;
}
}  // namespace

FLASHMEM bool BeatScanner::start(const char* filename) {
  if (active_) file_.close();
  active_ = false;
  result_ = BeatInfo();
  WavStream wav;
  if (!openWavData(filename, wav)) return false;
  file_ = wav.file;
  sampleRate_ = wav.sampleRate;
  frameSize_ = 2 * wav.numChannels;
  bytesRemaining_ = wav.dataSize;
  dataStart_ = file_.position();
  dataSize_ = wav.dataSize;
  hopFill_ = 0;
  hopSum_ = 0.0;
  prevLog_ = 0.0f;
  frames_ = 0;
  active_ = true;
  return true;
}

bool BeatScanner::step(uint32_t budgetUs) {
  if (!active_) return true;
  const uint32_t startUs = micros();
  uint8_t buf[2048];
  const uint32_t chunk = (sizeof(buf) / frameSize_) * frameSize_;

  while (bytesRemaining_ > 0 && frames_ < kBeatMaxFrames) {
    uint32_t toRead = bytesRemaining_ < chunk ? bytesRemaining_ : chunk;
    int got = file_.read(buf, toRead);
    // A read can fail when something else touches the SD card at the same moment:
    // seek back to where we were and try again a few times.
    for (int retry = 0; got <= 0 && retry < 4 && bytesRemaining_ > 0; ++retry) {
      file_.seek(dataStart_ + (dataSize_ - bytesRemaining_));
      got = file_.read(buf, toRead);
    }
    if (got <= 0) {
      bytesRemaining_ = 0;
      break;
    }
    bytesRemaining_ -= static_cast<uint32_t>(got);
    for (int off = 0; off + 1 < got; off += frameSize_) {
      int32_t s = static_cast<int16_t>(buf[off] | (buf[off + 1] << 8));
      hopSum_ += static_cast<double>(s) * s;
      if (++hopFill_ >= static_cast<uint32_t>(kBeatHop)) {
        float rms = sqrtf(static_cast<float>(hopSum_ / kBeatHop));
        float lg = logf(1.0f + rms);
        float flux = lg - prevLog_;
        prevLog_ = lg;
        g_beatFlux[frames_++] = flux > 0.0f ? flux : 0.0f;
        hopSum_ = 0.0;
        hopFill_ = 0;
        if (frames_ >= kBeatMaxFrames) break;
      }
    }
    if (micros() - startUs >= budgetUs) return false;
  }

  file_.close();
  active_ = false;
  analyze();
  return true;
}

FLASHMEM void BeatScanner::analyze() {
  result_ = BeatInfo();
  const int n = frames_;
  result_.confidence = -1.0f;  // -1: too few frames, -2: silent, -3: no periodicity
  result_.candidateBpm = static_cast<float>(n);  // (frame count while bailing out early)
  if (n < 120) return;
  const float hopMs = kBeatHop * 1000.0f / static_cast<float>(sampleRate_);

  float mean = 0.0f;
  for (int i = 0; i < n; ++i) mean += g_beatFlux[i];
  mean /= n;
  if (mean <= 0.0f) {
    result_.confidence = -2.0f;
    return;
  }

  // 1) Tempo: autocorrelation of the onset envelope over 60-200 BPM, lightly
  //    weighted towards 120 BPM to avoid half/double-tempo picks.
  int lagMin = static_cast<int>((60000.0f / 200.0f) / hopMs);
  int lagMax = static_cast<int>((60000.0f / 60.0f) / hopMs) + 1;
  if (lagMin < 2) lagMin = 2;
  if (lagMax > 200) lagMax = 200;
  float ac[203] = {0};
  for (int lag = lagMin - 1; lag <= lagMax + 1; ++lag) {
    float sum = 0.0f;
    for (int i = 0; i + lag < n; ++i) {
      sum += (g_beatFlux[i] - mean) * (g_beatFlux[i + lag] - mean);
    }
    ac[lag] = sum / (n - lag);
  }
  int bestLag = lagMin;
  float bestWeighted = -1e30f;
  for (int lag = lagMin; lag <= lagMax; ++lag) {
    float bpm = 60000.0f / (lag * hopMs);
    float octaves = log2f(bpm / 120.0f) / 0.9f;
    float weight = expf(-0.5f * octaves * octaves);
    float score = ac[lag] * weight;
    if (score > bestWeighted) {
      bestWeighted = score;
      bestLag = lag;
    }
  }
  if (bestWeighted <= 0.0f) {
    result_.confidence = -3.0f;
    return;
  }
  float y0 = ac[bestLag - 1], y1 = ac[bestLag], y2 = ac[bestLag + 1];
  float denom = y0 - 2.0f * y1 + y2;
  float period = static_cast<float>(bestLag);
  if (denom < -1e-9f) period += 0.5f * (y0 - y2) / denom;

  // 2) Refine tempo and find the beat phase: try periods within +-2% and every
  //    start offset, score how much onset energy lands on the grid.
  float bestScore = -1.0f, bestPeriod = period, bestPhase = 0.0f;
  for (float f = -0.05f; f <= 0.0501f; f += 0.001f) {
    float p = period * (1.0f + f);
    for (float ph = 0.0f; ph < p; ph += 0.25f) {
      float sum = 0.0f;
      int count = 0;
      for (float t = ph; t < n - 1; t += p) {
        sum += fluxMax3(static_cast<int>(t + 0.5f), n);
        ++count;
      }
      float score = sum / count;
      if (score > bestScore) {
        bestScore = score;
        bestPeriod = p;
        bestPhase = ph;
      }
    }
  }
  float confidence = bestScore / mean;
  result_.confidence = confidence;
  result_.candidateBpm = 60000.0f / (bestPeriod * hopMs);
  if (confidence < 1.25f) return;  // no clear pulse

  // 3) Which beat is "1"? The strongest of the four positions in a bar.
  float bar[4] = {0, 0, 0, 0};
  int k = 0;
  for (float t = bestPhase; t < n - 1; t += bestPeriod, ++k) {
    bar[k & 3] += fluxMax3(static_cast<int>(t + 0.5f), n);
  }
  int downbeat = 0;
  for (int j = 1; j < 4; ++j) {
    if (bar[j] > bar[downbeat]) downbeat = j;
  }
  float firstFrame = bestPhase + downbeat * bestPeriod;
  result_.valid = true;
  result_.bpm = 60000.0f / (bestPeriod * hopMs);
  result_.firstDownbeatMs = static_cast<uint32_t>(firstFrame * hopMs);
  result_.barMs = 4.0f * bestPeriod * hopMs;
}

}  // namespace dubbox
