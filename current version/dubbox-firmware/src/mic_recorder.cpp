#include "mic_recorder.h"

namespace dubbox {

namespace {
constexpr uint32_t kSampleRate = 44100;  // what the WAV player requires (the real clock is ~44117 Hz)
constexpr uint64_t kMinSamples = kSampleRate / 10;  // shorter takes are discarded
constexpr uint64_t kMaxSamples = static_cast<uint64_t>(kSampleRate) * 600;  // 10 minutes
}  // namespace

void MicRecorder::begin(AudioEngine* engine, ProjectManager* projects) {
  engine_ = engine;
  projects_ = projects;
}

void MicRecorder::fail(const char* code) {
  strncpy(err_, code, sizeof(err_) - 1);
  err_[sizeof(err_) - 1] = '\0';
  errMs_ = millis();
  Serial.printf("REC: %s\n", code);
}

void MicRecorder::writeHeader(uint32_t dataBytes) {
  uint8_t h[44];
  auto put32 = [&](int at, uint32_t v) {
    h[at] = v & 0xFF;
    h[at + 1] = (v >> 8) & 0xFF;
    h[at + 2] = (v >> 16) & 0xFF;
    h[at + 3] = (v >> 24) & 0xFF;
  };
  auto put16 = [&](int at, uint16_t v) {
    h[at] = v & 0xFF;
    h[at + 1] = v >> 8;
  };
  memcpy(h, "RIFF", 4);
  put32(4, 36 + dataBytes);
  memcpy(h + 8, "WAVEfmt ", 8);
  put32(16, 16);
  put16(20, 1);  // PCM
  put16(22, 1);  // mono
  put32(24, kSampleRate);
  put32(28, kSampleRate * 2);
  put16(32, 2);
  put16(34, 16);
  memcpy(h + 36, "data", 4);
  put32(40, dataBytes);
  file_.write(h, sizeof(h));
}

void MicRecorder::startRecording(bool sdBusy) {
  err_[0] = '\0';
  if (projects_ == nullptr || !projects_->isOpen()) return fail("noproject");
  if (engine_->isPlaying() || engine_->drums().previewing()) return fail("playing");
  if (sdBusy) return fail("busy");
  int track = -1;
  for (int t = 0; t < kNumTracks; ++t) {
    if (!engine_->trackLoaded(t)) {
      track = t;
      break;
    }
  }
  if (track < 0) return fail("notrack");
  bool named = false;
  for (int n = 1; n < 1000 && !named; ++n) {
    snprintf(name_, sizeof(name_), "REC%03d.wav", n);
    named = !SD.exists(name_);
  }
  if (!named) return fail("sd");
  file_ = SD.open(name_, FILE_WRITE);
  if (!file_) return fail("sd");
  writeHeader(0);

  track_ = track;
  startMs_ = engine_->transportPositionMillis();
  samples_ = 0;
  source_ = pendingSource_;
  engine_->recordQueue(source_).begin();
  recording_ = true;
  Serial.printf("REC: %s (source %d) -> track %d at %lu ms\n", name_, source_, track + 1,
                static_cast<unsigned long>(startMs_));
}

void MicRecorder::drain() {
  AudioRecordQueue& q = engine_->recordQueue(source_);
  while (q.available() >= 2) {
    memcpy(buf_, q.readBuffer(), 256);
    q.freeBuffer();
    memcpy(buf_ + 128, q.readBuffer(), 256);
    q.freeBuffer();
    if (file_.write(reinterpret_cast<uint8_t*>(buf_), 512) != 512) {
      q.end();
      file_.close();
      SD.remove(name_);
      recording_ = false;
      fail("sd");
      return;
    }
    samples_ += 256;
  }
}

void MicRecorder::finish() {
  AudioRecordQueue& q = engine_->recordQueue(source_);
  q.end();
  while (q.available() > 0) {
    memcpy(buf_, q.readBuffer(), 256);
    q.freeBuffer();
    if (file_.write(reinterpret_cast<uint8_t*>(buf_), 256) == 256) samples_ += 128;
  }
  recording_ = false;
  if (samples_ < kMinSamples) {
    file_.close();
    SD.remove(name_);
    Serial.println("REC: too short, discarded");
    return;
  }
  file_.seek(0);
  writeHeader(static_cast<uint32_t>(samples_ * 2));
  file_.close();
  Serial.printf("REC: saved %s (%lu ms)\n", name_, static_cast<unsigned long>(elapsedMs()));
  if (projects_->assignTrack(track_, name_)) {
    engine_->setClipOffsetMs(track_, 0, static_cast<int32_t>(startMs_));
    engine_->seekAllToMillis(startMs_);
  } else {
    fail("sd");
  }
}

void MicRecorder::update(bool sdBusy) {
  if (err_[0] != '\0' && millis() - errMs_ > 3000) err_[0] = '\0';  // the display shows it for a moment
  if (startRequested_) {
    startRequested_ = false;
    if (!recording_) startRecording(sdBusy);
  }
  if (stopRequested_) {
    stopRequested_ = false;
    if (recording_) finish();
  }
  if (!recording_) return;
  drain();
  if (recording_ && samples_ >= kMaxSamples) finish();
}

}  // namespace dubbox
