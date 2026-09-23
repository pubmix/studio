#pragma once

#include <Audio.h>

#include "audio_play_sd_wav_seekable.h"
#include "drum_machine.h"
#include "project_state.h"

namespace dubbox {

// Owns the audio graph: 4x SD WAV players -> per-track fx sends -> a stereo mixer pair ->
// the Audio Shield's I2S output, plus the transport (a shared clock over all tracks), the
// per-track clips and the master volume.
class AudioEngine {
 public:
  // Starts the SD card, allocates audio memory, and wires the graph.
  // Returns false if the SD card fails to mount or the codec does not answer.
  bool begin();

  // ---- Tracks ----

  // Starts playback of a WAV file on a track (0..kNumTracks-1). Returns false if the file
  // couldn't be opened. `filename` is copied into an internal buffer (kMaxFilenameLen) and
  // retained, so togglePlayPauseAll() can restart the file once playback runs off the end
  // and trackFilename() can report it. Safe on a track that is already playing (the old
  // file is stopped first). Do not run any other SD read (e.g. a waveform scan) while a
  // track streams: that once wedged the SD bus.
  bool loadTrack(int trackIndex, const char* filename);
  // Stops a track and empties its slot (project close / blank project).
  void unloadTrack(int trackIndex);
  bool trackLoaded(int trackIndex) const { return trackFilenames_[trackIndex][0] != '\0'; }
  // The file most recently loaded on a track, or nullptr if the slot is empty.
  const char* trackFilename(int trackIndex) const {
    return trackFilenames_[trackIndex][0] == '\0' ? nullptr : trackFilenames_[trackIndex];
  }
  static constexpr int kMaxFilenameLen = 64;
  // Length of the loaded file in ms (0 until the player has parsed the WAV header, which
  // takes a few audio blocks after loadTrack()).
  uint32_t trackLengthMillis(int trackIndex);
  bool isTrackPlaying(int trackIndex);
  // Sets linear gain (0.0 = silent, 1.0 = unity) for a track's fader.
  void setTrackGain(int trackIndex, float gain);
  // Master headphone volume (0.0-1.0, the SGTL5000's own volume register).
  void setMasterVolume(float volume01);
  float masterVolume() const { return masterVolume_; }

  // ---- Transport ----

  // Toggles play/pause on all tracks together, as one shared "song". Once the tracks have
  // run off the end this restarts them from the beginning instead (AudioPlaySdWav ignores
  // togglePlayPause() in its STOPPED state).
  void togglePlayPauseAll();
  // Pauses every track that is playing (loadTrack() starts playback immediately).
  void pauseAll();
  // True while the transport clock is running.
  bool isPlaying();
  // True once every loaded player has stopped (played through, or nothing loaded).
  bool isStopped();
  // Jumps the transport (and every track) to `ms`, playing or paused.
  void seekAllToMillis(uint32_t ms);
  // Call every loop: advances the transport clock, moves each track's player between clips
  // on time and keeps the clock locked to the audio.
  void updateTransport();
  uint32_t transportPositionMillis() const { return clockMs_; }
  // Timeline extent shown on screen: how far the last clip of any track could reach if it
  // were uncropped (its offset + the file length), or the end of the last pattern clip.
  // Stable while cropping.
  uint32_t songEndMs();

  // ---- Clips ----
  //
  // A track holds up to kMaxClips clips. Each has a file window [start, end) and an offset:
  // timeline time = file time + offset, so a clip can sit anywhere on the timeline. A track
  // has one audio player, so at a clip boundary the engine seeks it to the next clip's part
  // of the file and parks it (paused) in the gaps. Clips of a track keep their order and
  // never overlap on the timeline. A track is audible only while the clock is inside a clip.
  static constexpr int kMaxClips = 6;
  int trackClipCount(int trackIndex) const { return clipCount_[trackIndex]; }
  uint32_t clipStartMs(int trackIndex, int clip) const { return clipStart_[trackIndex][clip]; }
  uint32_t clipEndMs(int trackIndex, int clip) const { return clipEnd_[trackIndex][clip]; }
  int32_t clipOffsetMs(int trackIndex, int clip) const { return clipOff_[trackIndex][clip]; }
  // Sets one clip's file edges, clamped to the file and to its neighbours (in file time and
  // on the timeline).
  void setClipWindow(int trackIndex, int clip, uint32_t startMs, uint32_t endMs);
  // Moves a clip by setting its offset, clamped to its neighbours and to timeline start.
  void setClipOffsetMs(int trackIndex, int clip, int32_t offsetMs);
  // Splits the clip containing file position `ms` in two. False if there is no room
  // (kMaxClips), `ms` is not inside a clip, or a piece would be under 100 ms.
  bool splitClipAt(int trackIndex, uint32_t ms);
  // Joins clip `clip` with the next one (undo of a snip). False if they have been moved
  // apart (offsets differ).
  bool mergeClips(int trackIndex, int clip);
  // Replaces a track's clips (project load); an end of 0 means "to the end of the file".
  // `offsets` may be null (all zero); null starts/ends reset to one clip over the whole file.
  void setTrackClips(int trackIndex, const uint32_t* starts, const uint32_t* ends,
                     const int32_t* offsets, int count);
  // Re-evaluates every track's crop gate against the clock; call every loop().
  void updateTrackCropGates();

  // ---- Drum machine ----

  // Patterns, tempo and pattern clips live in the drum machine; its clock is the transport
  // clock, so pattern clips line up with the audio clips on the same timeline.
  DrumMachine& drums() { return drums_; }

  // ---- Metering / diagnostics ----

  // Audio-reactive peak level 0.0-1.0, tapped pre-fader off the track's own signal. The last
  // known value is kept in between AudioAnalyzePeak updates, so it can be polled every loop.
  float trackPeakLevel(int trackIndex);
  // The Audio Library's DSP load, 0-100%, and the peak since the last reset.
  float cpuUsagePercent();
  float cpuUsagePercentMax();
  void resetCpuUsagePercentMax();
  // The shared audio_block_t pool: current and peak usage out of kAudioMemoryBlocks. Delay
  // is the dominant consumer (see setDelayTimeMs()); pinned near the ceiling means glitches.
  int audioMemoryUsage();
  int audioMemoryUsageMax();
  // The pool lives in RAM2/DMAMEM, not RAM1. 280 blocks let a single delay run to 650 ms.
  static constexpr int kAudioMemoryBlocks = 280;
  // Set when begin() returns false. USB serial doesn't buffer, so callers should re-print
  // this periodically instead of relying on a monitor catching it once.
  const char* lastError() const { return lastError_; }

  // ---- Per-track fx ----
  //
  // Every track has its own chain of effect slots wired in series (slot 0 feeds slot 1, and
  // so on). Slot 0 can be any of the four types; extra slots (added from the display) are
  // Flange/Chorus only. Two types cannot run as four independent instances, for hardware
  // budget reasons, so they are shared/exclusive: only one track has each wired in at a
  // time, and selecting it on another track steals it:
  //   - Delay draws from the shared audio-block pool (setDelayTimeMs()); four long delay
  //     lines would each be shorter than a single one can be.
  //   - Freeverb needs ~24.6 KB of RAM1 per instance; four would be ~98 KB.
  // Flange and Chorus need only small caller-supplied delay lines, so each track gets its
  // own. No duplicates in a chain, so the longest is primary + Flange + Chorus (3 slots).
  enum FxType { kFxDelay = 0, kFxFlange = 1, kFxFreeverb = 2, kFxChorus = 3 };
  static constexpr int kNumFxTypes = 4;
  static constexpr int kMaxAdditionalFxSlots = 2;

  // Cycles the effect type at chain position `slot` to the next (+1) / previous (-1) type
  // still available there, skipping types already in this track's chain. A no-op if nothing
  // else is available. Live-rewires the chain; taking Delay/Freeverb from another track
  // tears down that track's connections for it.
  void cycleTrackFxSlotType(int trackIndex, int slot, int direction);
  int trackFxSlotCount(int trackIndex) const;
  int trackFxSlotType(int trackIndex, int slot) const;
  // The slot the encoder (hold+turn = type, tap = primary parameter) acts on. Defaults to 0;
  // addTrackFxSlot() moves it to the new slot.
  int trackActiveFxSlot(int trackIndex) const { return trackActiveFxSlot_[trackIndex]; }
  void setTrackActiveFxSlot(int trackIndex, int slot);
  // True if `fxType` (Flange or Chorus) could be added to this track's chain right now.
  bool trackFxSlotTypeAvailable(int trackIndex, int fxType) const;
  // Appends `fxType` as the chain's last slot and makes it the active slot. A silent no-op if
  // trackFxSlotTypeAvailable() is false.
  void addTrackFxSlot(int trackIndex, int fxType);
  // Which track currently has Delay/Freeverb wired in (-1 if none yet).
  int delayOwnerTrack() const { return delayOwnerTrack_; }
  int freeverbOwnerTrack() const { return freeverbOwnerTrack_; }
  // 0.0-1.0: how loud the track's whole effect chain is in the final mix. The dry signal
  // stays at full level (send/return, not a crossfade).
  void setTrackFxWetLevel(int trackIndex, float amount01);
  float trackFxWetLevel(int trackIndex) const { return trackFxWetLevel_[trackIndex]; }
  // Dub-style mute: closes the chain's send gate but leaves it wired, so echoes and reverb
  // already in flight ring out (at the level held when it was muted).
  void setTrackFxBypassed(int trackIndex, bool bypassed);
  bool trackFxBypassed(int trackIndex) const { return trackFxBypassed_[trackIndex]; }
  // Effect parameters. Delay and Freeverb apply only while `trackIndex` owns the effect
  // (ignored otherwise, since they would change another track's sound). Delay time goes up
  // to ~650 ms: at ~0.3445 blocks/ms that needs ~224 of the 280-block pool. Flange offset
  // and depth are normalized 0-1; rate is in Hz. Chorus takes 2-8 voices.
  void setDelayTimeMs(int trackIndex, float ms);
  void setDelayFeedback(int trackIndex, float amount01);  // 0 = one repeat, 1 = near-runaway (capped, kMaxFeedbackGain)
  void setFreeverbParams(int trackIndex, float roomsize01, float damping01);
  void setFlangeParams(int trackIndex, float offset01, float depth01, float rateHz);
  void setChorusVoices(int trackIndex, int voices);

 private:
  const char* lastError_ = nullptr;

  AudioPlaySdWavSeekable player_[kNumTracks];
  // Empty string (not nullptr) means "never loaded".
  char trackFilenames_[kNumTracks][kMaxFilenameLen] = {{0}};
  AudioMixer4 mixerL_;
  AudioMixer4 mixerR_;
  AudioOutputI2S out_;
  AudioControlSGTL5000 codec_;
  float masterVolume_ = 1.0f;

  // Peak metering, tapped straight off each player (pre-fader, pre-fx).
  AudioAnalyzePeak peak_[kNumTracks];
  float lastPeak_[kNumTracks] = {0.0f, 0.0f, 0.0f, 0.0f};

  // Clips: file windows and timeline offsets. An end of 0xFFFFFFFF means "no limit yet".
  uint32_t clipStart_[kNumTracks][kMaxClips] = {{0}};
  uint32_t clipEnd_[kNumTracks][kMaxClips] = {{0xFFFFFFFFu}, {0xFFFFFFFFu}, {0xFFFFFFFFu}, {0xFFFFFFFFu}};
  int32_t clipOff_[kNumTracks][kMaxClips] = {{0}};
  int clipCount_[kNumTracks] = {1, 1, 1, 1};

  // Transport clock: advances while playing, locked to the audio position of whichever track
  // is playing. `activeClip_` is the clip each player is serving (-1 = in a gap, parked).
  int activeClip_[kNumTracks] = {-1, -1, -1, -1};
  bool pendingAlign_[kNumTracks] = {false, false, false, false};
  bool clockRunning_ = false;
  bool atEnd_ = false;
  uint32_t clockMs_ = 0;
  uint32_t lastClockTick_ = 0;
  int clipAtTimeline(int trackIndex, int64_t timelineMs) const;
  void alignTrack(int trackIndex);
  // Where playback really finishes: the end of the last clip of any loaded track.
  uint32_t playEndMs();

  // 1.0 while the clock is inside one of the track's clips, else 0.0. Combined with the
  // fader in recomputeTrackGain().
  float trackCropGain_[kNumTracks] = {1.0f, 1.0f, 1.0f, 1.0f};

  // Shared/exclusive effects: one owning track at a time (see "Per-track fx" above).
  AudioMixer4 delayFeedbackMix_;
  AudioEffectDelay delay_;
  AudioEffectFreeverb freeverb_;
  int delayOwnerTrack_ = 0;     // matches trackFxType_'s default below
  int freeverbOwnerTrack_ = 2;  // ditto

  // Per-track independent effects.
  AudioEffectFlange flange_[kNumTracks];
  AudioEffectChorus chorus_[kNumTracks];
  static constexpr int kFlangeDelayLength = 12 * AUDIO_BLOCK_SAMPLES;
  static constexpr int kChorusDelayLength = 1024 * 2;
  static DMAMEM short flangeDelayLine_[kNumTracks][kFlangeDelayLength];
  static DMAMEM short chorusDelayLine_[kNumTracks][kChorusDelayLength];

  // One bus summing all tracks' effect outputs (each at its own wet level) into the final
  // mix, the way the dry mix is summed once via mixerL_/R_.
  AudioMixer4 wetSubMix_;
  AudioMixer4 finalMixL_;  // inputs: 0 dry, 1 wet, 2 drums, 3 piano
  AudioMixer4 finalMixR_;
  DrumMachine drums_;
  AudioConnection* patchDrumL_ = nullptr;
  AudioConnection* patchDrumR_ = nullptr;
  AudioConnection* patchPianoL_ = nullptr;
  AudioConnection* patchPianoR_ = nullptr;

  // Chain position 0 ("primary"): any of the 4 types.
  int trackFxType_[kNumTracks] = {kFxDelay, kFxFlange, kFxFreeverb, kFxChorus};
  // Chain positions 1..: Flange/Chorus only, in the order added. -1 marks an empty slot;
  // only the first trackAdditionalFxCount_ entries are meaningful.
  int trackAdditionalFx_[kNumTracks][kMaxAdditionalFxSlots] = {
      {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
  int trackAdditionalFxCount_[kNumTracks] = {0, 0, 0, 0};
  int trackActiveFxSlot_[kNumTracks] = {0, 0, 0, 0};
  float trackFxWetLevel_[kNumTracks] = {0.4f, 0.4f, 0.4f, 0.4f};
  // All tracks start bypassed: no fx audible until turned on.
  bool trackFxBypassed_[kNumTracks] = {true, true, true, true};
  // Bypassing only closes the send gate (sendMix_); the effect stays wired (wired once, the
  // first time it is switched on) so tails keep ringing out through the wet return.
  bool trackFxWired_[kNumTracks] = {false, false, false, false};
  // Source level (fader x crop) captured when an effect is muted: the tail keeps playing at
  // that level even if the fader is lowered afterwards.
  float trackFxMuteHold_[kNumTracks] = {0.0f, 0.0f, 0.0f, 0.0f};
  AudioMixer4 sendMix_[kNumTracks];
  AudioConnection* patchSendIn_[kNumTracks] = {nullptr};
  // The fader value last passed to setTrackGain(). Each track's fx send taps its player
  // upstream of the fader-controlled dry mix, so the fader is folded into the wet gain too:
  // pulling a fader down silences the fx along with the dry signal.
  float trackFaderGain_[kNumTracks] = {1.0f, 1.0f, 1.0f, 1.0f};

  // Applies fader x crop gate to the track's dry mixer gain and re-derives its wet gain: the
  // single place the two factors combine.
  void recomputeTrackGain(int trackIndex);
  // Re-applies wet level / bypass / fader / crop gate to wetSubMix_'s gain for the track.
  void applyTrackFxMixGain(int trackIndex);

  // The AudioStream carrying effect `type`'s output for a track: delay_/freeverb_ (shared,
  // trackIndex ignored) or flange_/chorus_[trackIndex].
  AudioStream& fxStreamRef(int trackIndex, int type);
  // Tears down the track's AudioConnections for effect `type`; releases Delay/Freeverb
  // ownership if this track held it.
  void disconnectTrackFx(int trackIndex, int type);
  // Wires effect `type` between `upstream` and `downstream` (nullptr = the track's
  // wetSubMix_ input, i.e. the chain's last stage); true if it got wired. For Delay/Freeverb
  // owned by another track: with `allowClaim` steals it (rebuilding that track's remaining
  // chain), without it does nothing and returns false. Assumes the slots were torn down.
  bool connectTrackFx(int trackIndex, int type, AudioStream& upstream, AudioStream* downstream,
                      bool allowClaim);
  // Disconnects the track's ENTIRE chain as trackFxType_/trackAdditionalFx_ stand right
  // now, so a caller about to change the chain contents must call this BEFORE writing the new
  // type (or it tears down the not-yet-connected new type and leaks the connected old one).
  void teardownTrackFxChain(int trackIndex);
  // Builds the chain fresh from the current state (no-op while bypassed). `allowClaim` is
  // true for a track's own action (type cycle, un-bypass) and false for addTrackFxSlot() and
  // for a victim's rebuild after a steal, which must not claim the effect straight back.
  void connectTrackFxChainFromState(int trackIndex, bool allowClaim);
  // teardown + connect back to back: correct only when the chain contents are not changing.
  void rebuildTrackFxChain(int trackIndex, bool allowClaim);

  // Allocated in begin() with `new` (PJRC's pattern for wiring a graph in a loop).
  AudioConnection* patchTrackL_[kNumTracks] = {nullptr};
  AudioConnection* patchTrackR_[kNumTracks] = {nullptr};
  AudioConnection* patchPeak_[kNumTracks] = {nullptr};
  AudioConnection* patchDryToFinalL_ = nullptr;
  AudioConnection* patchDryToFinalR_ = nullptr;
  AudioConnection* patchWetSubToFinalL_ = nullptr;
  AudioConnection* patchWetSubToFinalR_ = nullptr;
  AudioConnection* patchOutL_ = nullptr;
  AudioConnection* patchOutR_ = nullptr;

  // Delay's plumbing, rewired whenever ownership changes. Delay is only ever the primary slot,
  // so its upstream is always the owner's player; patchDelayOut_ goes to the owner's wetSubMix_
  // input, or to its first additional slot if it has one.
  AudioConnection* patchDelayTrackTap_ = nullptr;  // owner's player -> delayFeedbackMix_ input0
  AudioConnection* patchDelayIn_ = nullptr;        // delayFeedbackMix_ -> delay_
  AudioConnection* patchDelayFeedback_ = nullptr;  // delay_ -> delayFeedbackMix_ input1 (repeats)
  AudioConnection* patchDelayOut_ = nullptr;       // delay_ -> wetSubMix_ or the next chain slot

  // Freeverb's plumbing (no feedback loop; its filters are internal). Primary slot only.
  AudioConnection* patchFreeverbTrackTap_ = nullptr;  // owner's player -> freeverb_
  AudioConnection* patchFreeverbOut_ = nullptr;       // freeverb_ -> wetSubMix_ or the next slot

  // Flange/Chorus plumbing: tied to their track, at any chain position, so their source and
  // destination vary with where in the chain they sit (see connectTrackFx()).
  AudioConnection* patchFlangeIn_[kNumTracks] = {nullptr};
  AudioConnection* patchFlangeOut_[kNumTracks] = {nullptr};
  AudioConnection* patchChorusIn_[kNumTracks] = {nullptr};
  AudioConnection* patchChorusOut_[kNumTracks] = {nullptr};
};

}  // namespace dubbox
