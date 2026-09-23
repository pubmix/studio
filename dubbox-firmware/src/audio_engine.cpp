#include "audio_engine.h"

#include <string.h>

#include "storage.h"

namespace dubbox {

namespace {

// SGTL5000's volume(float) maps 0.0-1.0 straight to its headphone-volume register range, so
// 1.0 is the codec's real maximum.
constexpr float kDefaultCodecVolume = 1.0f;

// Max feedback gain at full intensity, kept safely below 1.0: at or above unity the echo grows
// without bound instead of decaying.
constexpr float kMaxFeedbackGain = 0.75f;

// The wet path carries ONE track's effect output but is mixed against the SUM of all four dry
// tracks, so at equal gain even 100% wet reads as much quieter. This boosts the whole wet bus;
// kept well short of 2x to leave margin before clipping on a loud, bass-heavy source.
constexpr float kWetMixBoost = 1.8f;

// See the audio-block budget note on AudioEngine::setDelayTimeMs().
constexpr float kMaxDelayMs = 650.0f;

// Flange offset/depth are exposed as 0-1 and scaled to a classic flange range; going further
// starts to sound like a slow chorus/slap delay. Kept under the 768-sample buffer combined so
// AudioEffectFlange::voices() never has to clamp.
constexpr int kFlangeMinOffsetSamples = 20;   // ~0.45ms @ 44.1kHz
constexpr int kFlangeMaxOffsetSamples = 500;  // ~11.3ms
constexpr int kFlangeMaxDepthSamples = 200;   // ~4.5ms

}  // namespace

DMAMEM short AudioEngine::flangeDelayLine_[kNumTracks][AudioEngine::kFlangeDelayLength];
DMAMEM short AudioEngine::chorusDelayLine_[kNumTracks][AudioEngine::kChorusDelayLength];

bool AudioEngine::begin() {
  AudioMemory(kAudioMemoryBlocks);

  if (!initSdCard()) {
    lastError_ = "SD card init failed — check it's inserted and FAT32.";
    Serial.println(lastError_);
    return false;
  }

  // Each track feeds the main mixers (what setTrackGain() controls). Fx chains are wired
  // lazily by rebuildTrackFxChain(); every track starts bypassed, so nothing is wired here.
  for (int i = 0; i < kNumTracks; ++i) {
    patchTrackL_[i] = new AudioConnection(player_[i], 0, mixerL_, i);
    patchTrackR_[i] = new AudioConnection(player_[i], 1, mixerR_, i);
    mixerL_.gain(i, 0.0f);
    mixerR_.gain(i, 0.0f);

    patchPeak_[i] = new AudioConnection(player_[i], 0, peak_[i], 0);

    patchSendIn_[i] = new AudioConnection(player_[i], 0, sendMix_[i], 0);
    sendMix_[i].gain(0, 0.0f);  // effect starts muted

    wetTap_[i].gain(1.0f);
    wetTapL_[i] = new AudioConnection(wetTap_[i], 0, wetSubMixL_, i);
    wetTapR_[i] = new AudioConnection(wetTap_[i], 0, wetSubMixR_, i);
    wetSubMixR_.gain(i, 0.0f);
    wetSubMixL_.gain(i, 0.0f);  // matches trackFxBypassed_[i]'s default (true)
  }

  delayFeedbackMix_.gain(0, 1.0f);  // owning track's tap, always passes through
  delayFeedbackMix_.gain(1, 0.0f);  // Delay's feedback tap, set by setDelayFeedback()

  patchDryToFinalL_ = new AudioConnection(mixerL_, 0, finalMixL_, 0);
  patchDryToFinalR_ = new AudioConnection(mixerR_, 0, finalMixR_, 0);
  patchWetSubToFinalL_ = new AudioConnection(wetSubMixL_, 0, finalMixL_, 1);
  patchWetSubToFinalR_ = new AudioConnection(wetSubMixR_, 0, finalMixR_, 1);
  finalMixL_.gain(0, 1.0f);
  // The wet bus is boosted so effects stand out against the summed dry mix (see kWetMixBoost).
  finalMixL_.gain(1, kWetMixBoost);
  finalMixR_.gain(0, 1.0f);
  finalMixR_.gain(1, kWetMixBoost);
  finalMixL_.gain(2, 1.0f);  // drums
  finalMixR_.gain(2, 1.0f);
  drums_.begin();
  patchDrumL_ = new AudioConnection(drums_.output(), 0, finalMixL_, 2);
  patchDrumR_ = new AudioConnection(drums_.output(), 0, finalMixR_, 2);
  finalMixL_.gain(3, 1.0f);  // piano
  finalMixR_.gain(3, 1.0f);
  patchPianoL_ = new AudioConnection(drums_.instrumentOutput(), 0, finalMixL_, 3);
  patchPianoR_ = new AudioConnection(drums_.instrumentOutput(), 0, finalMixR_, 3);

  patchOutL_ = new AudioConnection(finalMixL_, 0, out_, 0);
  patchOutR_ = new AudioConnection(finalMixR_, 0, out_, 1);

  // The delay-line buffers are static, so begin() can only be called once (one global engine).
  for (int i = 0; i < kNumTracks; ++i) {
    flange_[i].begin(flangeDelayLine_[i], kFlangeDelayLength,
                      kFlangeMinOffsetSamples, 0, 0.2f);
    chorus_[i].begin(chorusDelayLine_[i], kChorusDelayLength, 2);
  }

  // enable() talks to the SGTL5000 over I2C and returns false if it never responds, most often
  // because the Audio Shield is not fully seated. Checked, since a failed init otherwise means
  // silence with no indication why.
  if (!codec_.enable()) {
    lastError_ =
        "SGTL5000 codec enable failed — check the Audio Shield is fully "
        "seated on the header pins.";
    Serial.println(lastError_);
    return false;
  }
  masterVolume_ = kDefaultCodecVolume;
  codec_.volume(kDefaultCodecVolume);
  Serial.printf("Codec enabled OK, headphone volume set to %.0f%%\n",
                kDefaultCodecVolume * 100.0f);

  return true;
}

bool AudioEngine::loadTrack(int trackIndex, const char* filename) {
  strncpy(trackFilenames_[trackIndex], filename, kMaxFilenameLen - 1);
  trackFilenames_[trackIndex][kMaxFilenameLen - 1] = '\0';
  pendingAlign_[trackIndex] = true;
  activeClip_[trackIndex] = -1;
  return player_[trackIndex].play(filename);
}

void AudioEngine::setMasterVolume(float volume01) {
  masterVolume_ = constrain(volume01, 0.0f, 1.0f);
  codec_.volume(masterVolume_);
}

void AudioEngine::setTrackGain(int trackIndex, float gain) {
  trackFaderGain_[trackIndex] = gain;
  recomputeTrackGain(trackIndex);
}

bool AudioEngine::isTrackPlaying(int trackIndex) {
  return player_[trackIndex].isPlaying();
}

int AudioEngine::clipAtTimeline(int t, int64_t p) const {
  for (int k = 0; k < clipCount_[t]; ++k) {
    int64_t ts = static_cast<int64_t>(clipStart_[t][k]) + clipOff_[t][k];
    int64_t te = static_cast<int64_t>(clipEnd_[t][k]) + clipOff_[t][k];
    if (p >= ts && p < te) return k;
  }
  return -1;
}

void AudioEngine::alignTrack(int t) {
  if (!trackLoaded(t)) return;
  if (player_[t].lengthMillis() == 0) {  // header not parsed yet: retry from updateTransport()
    pendingAlign_[t] = true;
    return;
  }
  int k = clipAtTimeline(t, clockMs_);
  activeClip_[t] = k;
  if (k < 0) {
    if (player_[t].isPlaying()) player_[t].togglePlayPause();  // park in the gap
    return;
  }
  int64_t fp = static_cast<int64_t>(clockMs_) - clipOff_[t][k];
  player_[t].seek(static_cast<uint32_t>(fp < 0 ? 0 : fp));
  if (clockRunning_ && player_[t].isPaused()) player_[t].togglePlayPause();
  if (!clockRunning_ && player_[t].isPlaying()) player_[t].togglePlayPause();
}

uint32_t AudioEngine::songEndMs() {
  int64_t end = 0;
  for (int t = 0; t < kNumTracks; ++t) {
    if (trackFilenames_[t][0] == '\0') continue;
    uint32_t len = trackLengthMillis(t);
    if (len == 0) continue;
    int64_t e = static_cast<int64_t>(len) + clipOff_[t][clipCount_[t] - 1];
    if (e > end) end = e;
  }
  if (drums_.endMs() > end) end = drums_.endMs();
  return static_cast<uint32_t>(end);
}

uint32_t AudioEngine::playEndMs() {
  int64_t end = 0;
  for (int t = 0; t < kNumTracks; ++t) {
    if (trackFilenames_[t][0] == '\0') continue;
    int last = clipCount_[t] - 1;
    if (clipEnd_[t][last] == 0xFFFFFFFFu) continue;  // no limit set yet
    int64_t e = static_cast<int64_t>(clipEnd_[t][last]) + clipOff_[t][last];
    if (e > end) end = e;
  }
  if (drums_.endMs() > end) end = drums_.endMs();
  return static_cast<uint32_t>(end);
}

void AudioEngine::togglePlayPauseAll() {
  if (isStopped()) {
    // Reached the end (or nothing was ever started): start over from the top.
    bool any = false;
    for (int i = 0; i < kNumTracks; ++i) {
      if (trackFilenames_[i][0] != '\0') {
        player_[i].play(trackFilenames_[i]);
        pendingAlign_[i] = true;
        activeClip_[i] = -1;
        any = true;
      }
    }
    if (drums_.clipCount() > 0) any = true;
    clockMs_ = 0;
    atEnd_ = false;
    clockRunning_ = any;
    lastClockTick_ = millis();
    drums_.stopPreview();
    drums_.resync(0);
    return;
  }
  if (clockRunning_) {
    clockRunning_ = false;
    drums_.releaseAll();
    for (int i = 0; i < kNumTracks; ++i) {
      if (player_[i].isPlaying()) player_[i].togglePlayPause();
    }
  } else {
    clockRunning_ = true;
    lastClockTick_ = millis();
    drums_.stopPreview();
    drums_.resync(clockMs_);
    for (int i = 0; i < kNumTracks; ++i) {
      if (activeClip_[i] >= 0 && player_[i].isPaused()) player_[i].togglePlayPause();
    }
  }
}

bool AudioEngine::isPlaying() { return clockRunning_; }

bool AudioEngine::isStopped() {
  if (atEnd_) return true;
  for (int i = 0; i < kNumTracks; ++i) {
    if (trackFilenames_[i][0] == '\0') continue;
    if (!player_[i].isStopped()) return false;
  }
  // Every loaded player has stopped (or none is loaded); pattern clips may still play on.
  return drums_.endMs() <= clockMs_;
}

void AudioEngine::unloadTrack(int trackIndex) {
  player_[trackIndex].stop();
  trackFilenames_[trackIndex][0] = '\0';
  clipCount_[trackIndex] = 1;
  clipStart_[trackIndex][0] = 0;
  clipEnd_[trackIndex][0] = 0xFFFFFFFFu;
  clipOff_[trackIndex][0] = 0;
  activeClip_[trackIndex] = -1;
  pendingAlign_[trackIndex] = false;
}

void AudioEngine::pauseAll() {
  drums_.releaseAll();
  for (int i = 0; i < kNumTracks; ++i) {
    if (player_[i].isPlaying()) player_[i].togglePlayPause();
  }
  clockRunning_ = false;
}

void AudioEngine::seekAllToMillis(uint32_t ms) {
  clockMs_ = ms;
  atEnd_ = false;
  lastClockTick_ = millis();
  drums_.resync(ms);
  for (int i = 0; i < kNumTracks; ++i) alignTrack(i);
}

void AudioEngine::setClipOffsetMs(int t, int clip, int32_t off) {
  if (clip < 0 || clip >= clipCount_[t]) return;
  int64_t start = clipStart_[t][clip];
  int64_t end = clipEnd_[t][clip];
  if (end == 0xFFFFFFFFll) end = trackLengthMillis(t);
  int64_t lower = (clip > 0) ? static_cast<int64_t>(clipEnd_[t][clip - 1]) + clipOff_[t][clip - 1] : 0;
  int64_t upper = (clip < clipCount_[t] - 1)
                      ? static_cast<int64_t>(clipStart_[t][clip + 1]) + clipOff_[t][clip + 1]
                      : 4000000ll;
  int64_t minOff = lower - start;
  int64_t maxOff = upper - end;
  if (maxOff < minOff) maxOff = minOff;
  int64_t o = off;
  if (o < minOff) o = minOff;
  if (o > maxOff) o = maxOff;
  clipOff_[t][clip] = static_cast<int32_t>(o);
  alignTrack(t);
}

void AudioEngine::updateTransport() {
  uint32_t now = millis();
  drums_.update();
  for (int i = 0; i < kNumTracks; ++i) {
    if (pendingAlign_[i] && player_[i].lengthMillis() > 0) {
      pendingAlign_[i] = false;
      alignTrack(i);
    }
  }
  if (!clockRunning_) {
    lastClockTick_ = now;
    return;
  }
  clockMs_ += now - lastClockTick_;
  lastClockTick_ = now;

  // Stay locked to the audio: a track that is playing inside a clip defines the position.
  for (int i = 0; i < kNumTracks; ++i) {
    int k = activeClip_[i];
    if (k >= 0 && player_[i].isPlaying()) {
      int64_t p = static_cast<int64_t>(player_[i].positionMillis()) + clipOff_[i][k];
      int64_t d = p - static_cast<int64_t>(clockMs_);
      if (p >= 0 && d > -100 && d < 100) clockMs_ = static_cast<uint32_t>(p);
      break;
    }
  }

  // Move each track's player to the clip the clock is in (or park it in a gap).
  for (int i = 0; i < kNumTracks; ++i) {
    if (trackFilenames_[i][0] == '\0' || pendingAlign_[i]) continue;
    int k = clipAtTimeline(i, clockMs_);
    if (k != activeClip_[i]) {
      activeClip_[i] = k;
      if (k < 0) {
        if (player_[i].isPlaying()) player_[i].togglePlayPause();
      } else {
        int64_t fp = static_cast<int64_t>(clockMs_) - clipOff_[i][k];
        int64_t cur = player_[i].positionMillis();
        bool contiguous = player_[i].isPlaying() && (cur - fp < 40) && (fp - cur < 40);
        if (!contiguous) player_[i].seek(static_cast<uint32_t>(fp < 0 ? 0 : fp));
        if (player_[i].isPaused()) player_[i].togglePlayPause();
      }
    } else if (k >= 0 && player_[i].isPaused()) {
      player_[i].togglePlayPause();
    }
  }

  drums_.onTimeline(clockMs_);

  // The song is over once the clock passes the end of the last clip.
  uint32_t end = playEndMs();
  if (end > 0 && clockMs_ >= end) {
    atEnd_ = true;
    clockRunning_ = false;
    for (int i = 0; i < kNumTracks; ++i) {
      if (player_[i].isPlaying()) player_[i].togglePlayPause();
    }
  } else if (isStopped()) {
    clockRunning_ = false;  // every player ran off its file
  }
}

bool AudioEngine::mergeClips(int t, int clip) {
  if (clip < 0 || clip + 1 >= clipCount_[t]) return false;
  if (clipOff_[t][clip] != clipOff_[t][clip + 1]) return false;  // moved apart: cannot rejoin
  clipEnd_[t][clip] = clipEnd_[t][clip + 1];
  for (int j = clip + 1; j + 1 < clipCount_[t]; ++j) {
    clipStart_[t][j] = clipStart_[t][j + 1];
    clipEnd_[t][j] = clipEnd_[t][j + 1];
    clipOff_[t][j] = clipOff_[t][j + 1];
  }
  --clipCount_[t];
  return true;
}

uint32_t AudioEngine::trackLengthMillis(int trackIndex) {
  return player_[trackIndex].lengthMillis();
}

float AudioEngine::trackPeakLevel(int trackIndex) {
  if (peak_[trackIndex].available()) {
    lastPeak_[trackIndex] = peak_[trackIndex].read();
  }
  return lastPeak_[trackIndex];
}

float AudioEngine::cpuUsagePercent() { return AudioProcessorUsage(); }
float AudioEngine::cpuUsagePercentMax() { return AudioProcessorUsageMax(); }
void AudioEngine::resetCpuUsagePercentMax() { AudioProcessorUsageMaxReset(); }

int AudioEngine::audioMemoryUsage() { return AudioMemoryUsage(); }
int AudioEngine::audioMemoryUsageMax() { return AudioMemoryUsageMax(); }

void AudioEngine::setTrackPan(int trackIndex, int value) {
  if (trackIndex < 0 || trackIndex >= kNumTracks) return;
  AudioNoInterrupts();
  trackPan_[trackIndex] = constrain(value, -1000, 1000);
  recomputeTrackGain(trackIndex);
  AudioInterrupts();
}

void AudioEngine::recomputeTrackGain(int trackIndex) {
  float gain = trackFaderGain_[trackIndex] * trackCropGain_[trackIndex];
  const float pan = trackPan_[trackIndex] / 1000.0f;
  mixerL_.gain(trackIndex, gain * (pan > 0 ? 1.0f - pan : 1.0f));
  mixerR_.gain(trackIndex, gain * (pan < 0 ? 1.0f + pan : 1.0f));
  applyTrackFxMixGain(trackIndex);
}

void AudioEngine::applyTrackFxMixGain(int trackIndex) {
  // Post-fader send, independent return: zero/bypass/crop stops new input,
  // while the already-buffered delay/reverb decays at the selected wet level.
  const float source = trackFaderGain_[trackIndex] * trackCropGain_[trackIndex];
  sendMix_[trackIndex].gain(0, trackFxBypassed_[trackIndex] ? 0.0f : source);
  const float gain = trackFxWetLevel_[trackIndex];
  const float pan = trackPan_[trackIndex] / 1000.0f;
  wetSubMixL_.gain(trackIndex, gain * (pan > 0 ? 1.0f - pan : 1.0f));
  wetSubMixR_.gain(trackIndex, gain * (pan < 0 ? 1.0f + pan : 1.0f));
}

void AudioEngine::setClipWindow(int trackIndex, int clip, uint32_t startMs, uint32_t endMs) {
  if (clip < 0 || clip >= clipCount_[trackIndex]) return;
  uint32_t len = trackLengthMillis(trackIndex);
  uint32_t lower = (clip > 0) ? clipEnd_[trackIndex][clip - 1] : 0;
  uint32_t upper = (clip < clipCount_[trackIndex] - 1)
                       ? clipStart_[trackIndex][clip + 1]
                       : (len > 0 ? len : 0xFFFFFFFFu);
  // Also keep the clip off its neighbours and the start of the timeline: the offsets can
  // differ, so the file-time limits above are not enough.
  int64_t off = clipOff_[trackIndex][clip];
  int64_t tlLower = (clip > 0) ? static_cast<int64_t>(clipEnd_[trackIndex][clip - 1]) + clipOff_[trackIndex][clip - 1] : 0;
  int64_t fileLower = tlLower - off;
  if (fileLower > static_cast<int64_t>(lower)) lower = static_cast<uint32_t>(fileLower < 0 ? 0 : fileLower);
  if (clip < clipCount_[trackIndex] - 1) {
    int64_t fileUpper = static_cast<int64_t>(clipStart_[trackIndex][clip + 1]) + clipOff_[trackIndex][clip + 1] - off;
    if (fileUpper < static_cast<int64_t>(upper)) upper = static_cast<uint32_t>(fileUpper < 0 ? 0 : fileUpper);
  }
  if (startMs < lower) startMs = lower;
  if (startMs > upper) startMs = upper;
  if (endMs > upper) endMs = upper;
  if (endMs < startMs) endMs = startMs;
  clipStart_[trackIndex][clip] = startMs;
  clipEnd_[trackIndex][clip] = endMs;
}

bool AudioEngine::splitClipAt(int trackIndex, uint32_t ms) {
  constexpr uint32_t kMinPieceMs = 100;
  if (clipCount_[trackIndex] >= kMaxClips) return false;
  uint32_t len = trackLengthMillis(trackIndex);
  for (int k = 0; k < clipCount_[trackIndex]; ++k) {
    uint32_t start = clipStart_[trackIndex][k];
    uint32_t end = clipEnd_[trackIndex][k];
    if (len > 0 && end > len) end = len;
    if (ms < start + kMinPieceMs || ms + kMinPieceMs > end) continue;
    for (int j = clipCount_[trackIndex]; j > k + 1; --j) {
      clipStart_[trackIndex][j] = clipStart_[trackIndex][j - 1];
      clipEnd_[trackIndex][j] = clipEnd_[trackIndex][j - 1];
      clipOff_[trackIndex][j] = clipOff_[trackIndex][j - 1];
    }
    clipStart_[trackIndex][k + 1] = ms;
    clipEnd_[trackIndex][k + 1] = end;
    clipOff_[trackIndex][k + 1] = clipOff_[trackIndex][k];
    clipEnd_[trackIndex][k] = ms;
    ++clipCount_[trackIndex];
    return true;
  }
  return false;
}

void AudioEngine::setTrackClips(int trackIndex, const uint32_t* starts, const uint32_t* ends,
                                const int32_t* offsets, int count) {
  uint32_t len = trackLengthMillis(trackIndex);
  if (count < 1 || starts == nullptr || ends == nullptr) {
    clipCount_[trackIndex] = 1;
    clipStart_[trackIndex][0] = 0;
    clipEnd_[trackIndex][0] = len > 0 ? len : 0xFFFFFFFFu;
    clipOff_[trackIndex][0] = 0;
    alignTrack(trackIndex);
    return;
  }
  if (count > kMaxClips) count = kMaxClips;
  int64_t prevTimelineEnd = 0;
  int n = 0;
  for (int k = 0; k < count; ++k) {
    uint32_t s = starts[k];
    uint32_t e = (ends[k] == 0 || (len > 0 && ends[k] > len)) ? len : ends[k];
    if (e < s) e = s;
    int32_t off = offsets ? offsets[k] : 0;
    if (static_cast<int64_t>(s) + off < prevTimelineEnd) off = static_cast<int32_t>(prevTimelineEnd - s);
    clipStart_[trackIndex][n] = s;
    clipEnd_[trackIndex][n] = e;
    clipOff_[trackIndex][n] = off;
    prevTimelineEnd = static_cast<int64_t>(e) + off;
    ++n;
  }
  clipCount_[trackIndex] = n;
  alignTrack(trackIndex);
}

void AudioEngine::updateTrackCropGates() {
  for (int i = 0; i < kNumTracks; ++i) {
    // Audible only while the transport clock is inside one of the track's clips.
    float newGain = (clipAtTimeline(i, clockMs_) >= 0) ? 1.0f : 0.0f;
    if (newGain != trackCropGain_[i]) {
      trackCropGain_[i] = newGain;
      recomputeTrackGain(i);
    }
  }
}

AudioStream& AudioEngine::fxStreamRef(int trackIndex, int type) {
  switch (type) {
    case kFxDelay:
      return delay_;
    case kFxFreeverb:
      return freeverb_;
    case kFxChorus:
      return chorus_[trackIndex];
    case kFxFlange:
    default:
      return flange_[trackIndex];
  }
}

void AudioEngine::disconnectTrackFx(int trackIndex, int type) {
  switch (type) {
    case kFxDelay:
      if (delayOwnerTrack_ == trackIndex) {
        delete patchDelayTrackTap_;
        patchDelayTrackTap_ = nullptr;
        delete patchDelayIn_;
        patchDelayIn_ = nullptr;
        delete patchDelayFeedback_;
        patchDelayFeedback_ = nullptr;
        delete patchDelayOut_;
        patchDelayOut_ = nullptr;
        delayOwnerTrack_ = -1;
      }
      break;
    case kFxFreeverb:
      if (freeverbOwnerTrack_ == trackIndex) {
        delete patchFreeverbTrackTap_;
        patchFreeverbTrackTap_ = nullptr;
        delete patchFreeverbOut_;
        patchFreeverbOut_ = nullptr;
        freeverbOwnerTrack_ = -1;
      }
      break;
    case kFxFlange:
      delete patchFlangeIn_[trackIndex];
      patchFlangeIn_[trackIndex] = nullptr;
      delete patchFlangeOut_[trackIndex];
      patchFlangeOut_[trackIndex] = nullptr;
      break;
    case kFxChorus:
      delete patchChorusIn_[trackIndex];
      patchChorusIn_[trackIndex] = nullptr;
      delete patchChorusOut_[trackIndex];
      patchChorusOut_[trackIndex] = nullptr;
      break;
  }
}

bool AudioEngine::connectTrackFx(int trackIndex, int type,
                                  AudioStream& upstream,
                                  AudioStream* downstream, bool allowClaim) {
  switch (type) {
    case kFxDelay: {
      if (delayOwnerTrack_ != trackIndex) {
        if (!allowClaim) return false;  // rebuilding a victim's chain — do not reclaim
        if (delayOwnerTrack_ != -1) {
          // Steal from the current owner: its type stays "Delay", it just loses its
          // connections. Ownership must change hands BEFORE the victim's chain is rebuilt,
          // or the victim would still see itself as owner and reconnect right back.
          int victim = delayOwnerTrack_;
          disconnectTrackFx(victim, kFxDelay);
          delayOwnerTrack_ = trackIndex;
          rebuildTrackFxChain(victim, /*allowClaim=*/false);
        } else {
          delayOwnerTrack_ = trackIndex;
        }
      }
      patchDelayTrackTap_ =
          new AudioConnection(upstream, 0, delayFeedbackMix_, 0);
      // delayFeedbackMix_ sums the owner's tap (input 0) with delay_'s own feedback (input 1); this feeds that sum into the delay line.
      patchDelayIn_ = new AudioConnection(delayFeedbackMix_, 0, delay_, 0);
      patchDelayFeedback_ =
          new AudioConnection(delay_, 0, delayFeedbackMix_, 1);
      patchDelayOut_ = downstream
                            ? new AudioConnection(delay_, 0, *downstream, 0)
                            : new AudioConnection(delay_, 0, wetTap_[trackIndex], 0);
      return true;
    }
    case kFxFreeverb: {
      if (freeverbOwnerTrack_ != trackIndex) {
        if (!allowClaim) return false;
        if (freeverbOwnerTrack_ != -1) {
          int victim = freeverbOwnerTrack_;
          disconnectTrackFx(victim, kFxFreeverb);
          freeverbOwnerTrack_ = trackIndex;
          rebuildTrackFxChain(victim, /*allowClaim=*/false);
        } else {
          freeverbOwnerTrack_ = trackIndex;
        }
      }
      patchFreeverbTrackTap_ = new AudioConnection(upstream, 0, freeverb_, 0);
      patchFreeverbOut_ =
          downstream ? new AudioConnection(freeverb_, 0, *downstream, 0)
                     : new AudioConnection(freeverb_, 0, wetTap_[trackIndex], 0);
      return true;
    }
    case kFxFlange:
      patchFlangeIn_[trackIndex] =
          new AudioConnection(upstream, 0, flange_[trackIndex], 0);
      patchFlangeOut_[trackIndex] =
          downstream
              ? new AudioConnection(flange_[trackIndex], 0, *downstream, 0)
              : new AudioConnection(flange_[trackIndex], 0, wetTap_[trackIndex], 0);
      return true;
    case kFxChorus:
      patchChorusIn_[trackIndex] =
          new AudioConnection(upstream, 0, chorus_[trackIndex], 0);
      patchChorusOut_[trackIndex] =
          downstream
              ? new AudioConnection(chorus_[trackIndex], 0, *downstream, 0)
              : new AudioConnection(chorus_[trackIndex], 0, wetTap_[trackIndex], 0);
      return true;
  }
  return false;
}

void AudioEngine::teardownTrackFxChain(int trackIndex) {
  disconnectTrackFx(trackIndex, trackFxType_[trackIndex]);
  for (int i = 0; i < trackAdditionalFxCount_[trackIndex]; ++i) {
    disconnectTrackFx(trackIndex, trackAdditionalFx_[trackIndex][i]);
  }
}

void AudioEngine::connectTrackFxChainFromState(int trackIndex,
                                                bool allowClaim) {
  if (!trackFxWired_[trackIndex]) return;  // never switched on yet: nothing to wire

  int additionalCount = trackAdditionalFxCount_[trackIndex];
  AudioStream* upstream = &sendMix_[trackIndex];

  AudioStream* primaryDownstream =
      (additionalCount == 0)
          ? nullptr
          : &fxStreamRef(trackIndex, trackAdditionalFx_[trackIndex][0]);
  if (connectTrackFx(trackIndex, trackFxType_[trackIndex], *upstream,
                      primaryDownstream, allowClaim)) {
    upstream = &fxStreamRef(trackIndex, trackFxType_[trackIndex]);
  }
  // else: the primary is a shared effect this track doesn't own right now, so `upstream` stays
  // the send tap and any additional slot below still gets a live signal instead of silence.

  for (int i = 0; i < additionalCount; ++i) {
    bool isLast = (i == additionalCount - 1);
    AudioStream* downstream =
        isLast ? nullptr
               : &fxStreamRef(trackIndex, trackAdditionalFx_[trackIndex][i + 1]);
    int type = trackAdditionalFx_[trackIndex][i];
    // Flange/Chorus are never shared, so allowClaim is irrelevant here —
    // always wires successfully.
    if (connectTrackFx(trackIndex, type, *upstream, downstream,
                        /*allowClaim=*/false)) {
      upstream = &fxStreamRef(trackIndex, type);
    }
  }
}

void AudioEngine::rebuildTrackFxChain(int trackIndex, bool allowClaim) {
  teardownTrackFxChain(trackIndex);
  connectTrackFxChainFromState(trackIndex, allowClaim);
}

int AudioEngine::trackFxSlotCount(int trackIndex) const {
  return 1 + trackAdditionalFxCount_[trackIndex];
}

int AudioEngine::trackFxSlotType(int trackIndex, int slot) const {
  return (slot == 0) ? trackFxType_[trackIndex]
                      : trackAdditionalFx_[trackIndex][slot - 1];
}

void AudioEngine::setTrackActiveFxSlot(int trackIndex, int slot) {
  trackActiveFxSlot_[trackIndex] =
      constrain(slot, 0, trackFxSlotCount(trackIndex) - 1);
}

bool AudioEngine::trackFxSlotTypeAvailable(int trackIndex, int fxType) const {
  if (fxType != kFxFlange && fxType != kFxChorus) return false;
  if (trackFxSlotCount(trackIndex) >= 1 + kMaxAdditionalFxSlots) return false;
  for (int s = 0; s < trackFxSlotCount(trackIndex); ++s) {
    if (trackFxSlotType(trackIndex, s) == fxType) return false;  // no duplicates
  }
  return true;
}

void AudioEngine::addTrackFxSlot(int trackIndex, int fxType) {
  if (!trackFxSlotTypeAvailable(trackIndex, fxType)) return;
  trackAdditionalFx_[trackIndex][trackAdditionalFxCount_[trackIndex]] = fxType;
  trackAdditionalFxCount_[trackIndex]++;
  trackActiveFxSlot_[trackIndex] = trackFxSlotCount(trackIndex) - 1;
  // Flange/Chorus are never shared, so this never needs to claim anything.
  rebuildTrackFxChain(trackIndex, /*allowClaim=*/false);
  applyTrackFxMixGain(trackIndex);
}

void AudioEngine::cycleTrackFxSlotType(int trackIndex, int slot,
                                        int direction) {
  static const int kPrimaryPool[] = {kFxDelay, kFxFlange, kFxFreeverb,
                                      kFxChorus};
  static const int kAdditionalPool[] = {kFxFlange, kFxChorus};
  const int* pool = (slot == 0) ? kPrimaryPool : kAdditionalPool;
  int poolSize = (slot == 0) ? 4 : 2;

  int currentType = trackFxSlotType(trackIndex, slot);
  int candidates[4];
  int numCandidates = 0;
  for (int i = 0; i < poolSize; ++i) {
    int t = pool[i];
    if (t == currentType) {
      candidates[numCandidates++] = t;
      continue;
    }
    bool usedElsewhere = false;
    for (int s = 0; s < trackFxSlotCount(trackIndex); ++s) {
      if (s != slot && trackFxSlotType(trackIndex, s) == t) {
        usedElsewhere = true;
        break;
      }
    }
    if (!usedElsewhere) candidates[numCandidates++] = t;
  }
  if (numCandidates <= 1) return;  // nothing else available at this slot

  int idx = 0;
  for (int i = 0; i < numCandidates; ++i) {
    if (candidates[i] == currentType) {
      idx = i;
      break;
    }
  }
  int newType = candidates[(idx + direction + numCandidates) % numCandidates];
  if (newType == currentType) return;

  // Tear down using the OLD type BEFORE overwriting it (see teardownTrackFxChain()).
  teardownTrackFxChain(trackIndex);
  if (slot == 0) {
    trackFxType_[trackIndex] = newType;
  } else {
    trackAdditionalFx_[trackIndex][slot - 1] = newType;
  }
  connectTrackFxChainFromState(trackIndex, /*allowClaim=*/true);
  applyTrackFxMixGain(trackIndex);
}

void AudioEngine::setTrackFxWetLevel(int trackIndex, float amount01) {
  trackFxWetLevel_[trackIndex] = constrain(amount01, 0.0f, 1.0f);
  applyTrackFxMixGain(trackIndex);
}

void AudioEngine::setTrackFxBypassed(int trackIndex, bool bypassed) {
  if (bypassed == trackFxBypassed_[trackIndex]) return;
  trackFxBypassed_[trackIndex] = bypassed;
  if (!bypassed) {
    // Switching on: wire the chain the first time, and re-claim a shared
    // effect (Delay/Freeverb) if another track took it while this one was muted.
    // Otherwise leave the wiring alone so a still-ringing tail is not interrupted.
    bool needWire = !trackFxWired_[trackIndex];
    int primary = trackFxType_[trackIndex];
    if ((primary == kFxDelay && delayOwnerTrack_ != trackIndex) ||
        (primary == kFxFreeverb && freeverbOwnerTrack_ != trackIndex)) {
      needWire = true;
    }
    trackFxWired_[trackIndex] = true;
    if (needWire) rebuildTrackFxChain(trackIndex, /*allowClaim=*/true);
  }
  applyTrackFxMixGain(trackIndex);
}

void AudioEngine::setDelayTimeMs(int trackIndex, float ms) {
  if (delayOwnerTrack_ != trackIndex) return;
  delay_.delay(0, constrain(ms, 1.0f, kMaxDelayMs));
}

void AudioEngine::setDelayFeedback(int trackIndex, float amount01) {
  if (delayOwnerTrack_ != trackIndex) return;
  delayFeedbackMix_.gain(1, constrain(amount01, 0.0f, 1.0f) * kMaxFeedbackGain);
}

void AudioEngine::setFreeverbParams(int trackIndex, float roomsize01,
                                     float damping01) {
  if (freeverbOwnerTrack_ != trackIndex) return;
  freeverb_.roomsize(constrain(roomsize01, 0.0f, 1.0f));
  freeverb_.damping(constrain(damping01, 0.0f, 1.0f));
}

void AudioEngine::setFlangeParams(int trackIndex, float offset01,
                                   float depth01, float rateHz) {
  offset01 = constrain(offset01, 0.0f, 1.0f);
  depth01 = constrain(depth01, 0.0f, 1.0f);
  int offset = kFlangeMinOffsetSamples +
               static_cast<int>(offset01 *
                                 (kFlangeMaxOffsetSamples - kFlangeMinOffsetSamples));
  int depth = static_cast<int>(depth01 * kFlangeMaxDepthSamples);
  flange_[trackIndex].voices(offset, depth, rateHz);
}

void AudioEngine::setChorusVoices(int trackIndex, int voices) {
  chorus_[trackIndex].voices(constrain(voices, 2, 8));
}

}  // namespace dubbox
