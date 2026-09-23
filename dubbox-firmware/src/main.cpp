#include <Arduino.h>
#include <MTP_Teensy.h>
#include <SD.h>

#include "audio_engine.h"
#include "esp_link.h"
#include "input_manager.h"
#include "project.h"
#include "project_state.h"
#include "waveform.h"

// Dub-Box firmware. All UI lives on the ESP32 touchscreen (see esp_link.h); this side
// runs the audio engine, projects on the SD card, background waveform/beat scans, and the
// physical faders, encoders and buttons.

namespace {

dubbox::AudioEngine audioEngine;
dubbox::EspLink espLink;
dubbox::ProjectManager projects;
dubbox::InputManager inputManager;

// Fine waveform envelopes for the ESP32 display, scanned in the background from loop()
// while paused. DMAMEM keeps them out of RAM1's tight stack budget.
constexpr int kPeakColumns = 244;
DMAMEM uint8_t wavePeaks[dubbox::kNumTracks][kPeakColumns];
dubbox::PeakScanner peakScanner;
dubbox::BeatScanner beatScanner;
int peakScanTrack = -1;
int beatScanTrack = -1;
bool peakPending[dubbox::kNumTracks] = {true, true, true, true};
bool beatPending[dubbox::kNumTracks] = {false, false, false, false};

// Bench-test files: the first boot with an empty SD card seeds a "DEMO" project from
// these four WAVs at the card root.
const char* const kTrackFiles[dubbox::kNumTracks] = {"TRACK1.WAV", "TRACK2.WAV", "TRACK3.WAV",
                                                     "TRACK4.WAV"};

// Effect names, indexed by AudioEngine::FxType (each track's chain can hold different
// types per slot). There is no phaser in the Teensy Audio Library, so Freeverb took that
// place alongside the real Flange/Chorus.
constexpr const char* kTrackFxNames[dubbox::AudioEngine::kNumFxTypes] = {"Delay", "Flange",
                                                                        "Freeverb", "Chorus"};

// Teensy 4.1's onboard LED: blinks once loop() is running, so you can tell the firmware is
// alive with no monitor open and no sound coming out.
constexpr uint8_t kHeartbeatPin = LED_BUILTIN;
constexpr uint32_t kHeartbeatIntervalMs = 500;
constexpr uint32_t kStatusPrintIntervalMs = 1000;
uint32_t lastHeartbeatMs = 0;
bool heartbeatOn = false;
uint32_t lastStatusPrintMs = 0;
uint32_t loopMaxUs = 0;  // worst loop() iteration since the last status print

// Per-track fx knobs. Encoder i is track i's own fx control, acting on that track's ACTIVE
// slot (chosen on the display):
//   rotate                       -> the whole chain's wet level, 0-100%
//   hold the button + rotate     -> cycle the active slot's effect type
//   tap (no rotation while held) -> cycle the active effect's own primary parameter
// Each fx-select button toggles that track's whole chain on/off (bypass). Delay and
// Freeverb are shared between tracks (one wired in at a time, a RAM budget limit);
// Flange/Chorus are independent per track and the only ones that can be stacked.
bool encoderWasHeld[dubbox::kNumTracks] = {false, false, false, false};
bool encoderManipulatedDuringHold[dubbox::kNumTracks] = {false, false, false, false};

// A single track's wet signal competes with the dry mix of all four, so the default is high
// (see also AudioEngine::kWetMixBoost).
int trackFxWetPercent[dubbox::kNumTracks] = {65, 65, 65, 65};
// ~20-24 detents per turn over a 0-100 range: 6 per detent sweeps it in about one turn.
constexpr int kWetStepPerDetent = 6;

// Delay and Freeverb hold one value each, since only one track can own either at a time.
float delayTimeMs = 300.0f;
int delayFeedbackPercent = 45;
// 650 ms is the longest delay the audio memory pool can hold (see AudioEngine::setDelayTimeMs()).
constexpr float kDelayTimePresetsMs[] = {50, 100, 150, 200, 300, 400, 500, 600, 650};
constexpr int kNumDelayTimePresets = sizeof(kDelayTimePresetsMs) / sizeof(kDelayTimePresetsMs[0]);
int delayTimePresetIndex = 4;  // matches delayTimeMs

// Freeverb and Flange get a few named "character" presets bundling their parameters: there
// is one encoder per track, so only wet level is continuous and one preset is cycleable.
struct FreeverbPreset {
  const char* name;
  float roomsize01;
  float damping01;
};
constexpr FreeverbPreset kFreeverbPresets[] = {
    {"Small", 0.2f, 0.7f},
    {"Room", 0.4f, 0.5f},
    {"Hall", 0.7f, 0.3f},
    {"Cathedral", 0.95f, 0.15f},
};
constexpr int kNumFreeverbPresets = sizeof(kFreeverbPresets) / sizeof(kFreeverbPresets[0]);
int freeverbPresetIndex = 2;  // "Hall"

struct FlangePreset {
  const char* name;
  float offset01;
  float depth01;
  float rateHz;
};
constexpr FlangePreset kFlangePresets[] = {
    {"Subtle", 0.2f, 0.15f, 0.15f},
    {"Classic", 0.4f, 0.35f, 0.3f},
    {"Deep", 0.6f, 0.6f, 0.5f},
    {"Extreme", 0.9f, 0.9f, 1.2f},
};
constexpr int kNumFlangePresets = sizeof(kFlangePresets) / sizeof(kFlangePresets[0]);
int flangePresetIndex[dubbox::kNumTracks] = {2, 2, 2, 2};  // "Deep"

constexpr int kChorusVoicePresets[] = {2, 3, 4, 6, 8};
constexpr int kNumChorusVoicePresets = sizeof(kChorusVoicePresets) / sizeof(kChorusVoicePresets[0]);
int chorusVoicePresetIndex[dubbox::kNumTracks] = {1, 1, 1, 1};

// AudioPlaySdWav cuts off abruptly at end-of-file, and that discontinuity is an audible
// click. Left alone, the delay's feedback loop keeps re-circulating that click, heard as a
// repeating "clip" after the song ends. So feedback is muted the moment playback stops
// (and restored on the next play): at most one more echo.
bool wasStoppedLastLoop = false;

// Cycles track `track`'s active fx slot's own primary parameter. A logged no-op if the slot
// is Delay/Freeverb but another track currently owns that shared effect.
void cycleTrackFxPrimaryParam(int track) {
  int activeSlot = audioEngine.trackActiveFxSlot(track);
  switch (audioEngine.trackFxSlotType(track, activeSlot)) {
    case dubbox::AudioEngine::kFxDelay:
      if (audioEngine.delayOwnerTrack() != track) {
        Serial.printf("Track %d: Delay is in use by Track %d\n", track + 1,
                      audioEngine.delayOwnerTrack() + 1);
        return;
      }
      delayTimePresetIndex = (delayTimePresetIndex + 1) % kNumDelayTimePresets;
      delayTimeMs = kDelayTimePresetsMs[delayTimePresetIndex];
      audioEngine.setDelayTimeMs(track, delayTimeMs);
      Serial.printf("Track %d: Delay time = %.0fms\n", track + 1, delayTimeMs);
      break;
    case dubbox::AudioEngine::kFxFreeverb:
      if (audioEngine.freeverbOwnerTrack() != track) {
        Serial.printf("Track %d: Freeverb is in use by Track %d\n", track + 1,
                      audioEngine.freeverbOwnerTrack() + 1);
        return;
      }
      freeverbPresetIndex = (freeverbPresetIndex + 1) % kNumFreeverbPresets;
      audioEngine.setFreeverbParams(track, kFreeverbPresets[freeverbPresetIndex].roomsize01,
                                    kFreeverbPresets[freeverbPresetIndex].damping01);
      Serial.printf("Track %d: Freeverb = %s\n", track + 1,
                    kFreeverbPresets[freeverbPresetIndex].name);
      break;
    case dubbox::AudioEngine::kFxFlange:
      flangePresetIndex[track] = (flangePresetIndex[track] + 1) % kNumFlangePresets;
      audioEngine.setFlangeParams(track, kFlangePresets[flangePresetIndex[track]].offset01,
                                  kFlangePresets[flangePresetIndex[track]].depth01,
                                  kFlangePresets[flangePresetIndex[track]].rateHz);
      Serial.printf("Track %d: Flange = %s\n", track + 1,
                    kFlangePresets[flangePresetIndex[track]].name);
      break;
    case dubbox::AudioEngine::kFxChorus:
      chorusVoicePresetIndex[track] = (chorusVoicePresetIndex[track] + 1) % kNumChorusVoicePresets;
      audioEngine.setChorusVoices(track, kChorusVoicePresets[chorusVoicePresetIndex[track]]);
      Serial.printf("Track %d: Chorus voices = %d\n", track + 1,
                    kChorusVoicePresets[chorusVoicePresetIndex[track]]);
      break;
  }
}

// A project was opened/closed/created: waveforms and beat grids for the affected tracks are
// stale, so drop them and queue fresh scans for tracks that now hold a file.
void refreshScansForChangedTracks() {
  uint8_t changed = projects.takeLoadedMask();
  for (int i = 0; i < dubbox::kNumTracks; ++i) {
    if (!(changed & (1 << i))) continue;
    espLink.clearWaveform(i);
    espLink.setBeats(i, dubbox::BeatInfo());
    if (peakScanTrack == i) {
      peakScanner.cancel();
      peakScanTrack = -1;
    }
    if (beatScanTrack == i) {
      beatScanner.cancel();
      beatScanTrack = -1;
    }
    peakPending[i] = audioEngine.trackLoaded(i);
    beatPending[i] = audioEngine.trackLoaded(i);
  }
}

// Background SD scans (waveform peaks, then beat grid), one track at a time and only while
// paused: reading the card while tracks stream can starve playback.
void runBackgroundScans() {
  if (audioEngine.isPlaying()) return;
  if (peakScanner.active()) {
    if (peakScanner.step(1500)) {
      Serial.printf("PEAKS: track %d scan %s\n", peakScanTrack + 1,
                    peakScanner.ok() ? "done" : "failed");
      if (peakScanner.ok()) espLink.setWaveform(peakScanTrack, wavePeaks[peakScanTrack], kPeakColumns);
      peakScanTrack = -1;
    }
    return;
  }
  if (beatScanner.active()) {
    if (beatScanner.step(1500)) {
      const dubbox::BeatInfo& b = beatScanner.result();
      Serial.printf("BEATS: track %d %s bpm %.1f first %lu ms bar %.0f ms (candidate %.1f bpm, "
                    "confidence %.2f)\n",
                    beatScanTrack + 1, b.valid ? "found" : "none", b.bpm,
                    static_cast<unsigned long>(b.firstDownbeatMs), b.barMs, b.candidateBpm,
                    b.confidence);
      espLink.setBeats(beatScanTrack, b);
      beatScanTrack = -1;
    }
    return;
  }
  for (int i = 0; i < dubbox::kNumTracks; ++i) {
    if (!peakPending[i]) continue;
    peakPending[i] = false;
    const char* file = audioEngine.trackLoaded(i) ? audioEngine.trackFilename(i) : nullptr;
    if (file == nullptr) continue;
    bool started = peakScanner.start(file, wavePeaks[i], kPeakColumns);
    Serial.printf("PEAKS: track %d (%s) start %s\n", i + 1, file, started ? "ok" : "FAILED");
    if (started) peakScanTrack = i;
    return;
  }
  for (int i = 0; i < dubbox::kNumTracks; ++i) {
    if (!beatPending[i]) continue;
    beatPending[i] = false;
    const char* file = audioEngine.trackFilename(i);
    if (file == nullptr) continue;
    if (beatScanner.start(file)) beatScanTrack = i;
    return;
  }
}

// Physical controls: per-track fx knobs and bypass buttons, and the four drum pads.
void handleFxControls() {
  for (int track = 0; track < dubbox::kNumTracks; ++track) {
    int32_t d = inputManager.encoderRotationDelta(track);
    bool heldNow = inputManager.encoderHeld(track);
    if (heldNow && !encoderWasHeld[track]) encoderManipulatedDuringHold[track] = false;

    if (heldNow) {
      if (d != 0) {
        int activeSlot = audioEngine.trackActiveFxSlot(track);
        audioEngine.cycleTrackFxSlotType(track, activeSlot, d > 0 ? 1 : -1);
        encoderManipulatedDuringHold[track] = true;
        Serial.printf("Track %d slot %d: fx = %s\n", track + 1, activeSlot,
                      kTrackFxNames[audioEngine.trackFxSlotType(track, activeSlot)]);
      }
    } else if (d != 0) {
      // One wet knob per track, for the WHOLE chain's output.
      trackFxWetPercent[track] =
          constrain(trackFxWetPercent[track] + d * kWetStepPerDetent, 0, 100);
      audioEngine.setTrackFxWetLevel(track, trackFxWetPercent[track] / 100.0f);
      Serial.printf("Track %d: wet = %d%%\n", track + 1, trackFxWetPercent[track]);
    }

    if (!heldNow && encoderWasHeld[track] && !encoderManipulatedDuringHold[track]) {
      cycleTrackFxPrimaryParam(track);
    }
    encoderWasHeld[track] = heldNow;

    if (inputManager.fxSelectPressed(track)) {
      bool nowBypassed = !audioEngine.trackFxBypassed(track);
      audioEngine.setTrackFxBypassed(track, nowBypassed);
      Serial.printf("Track %d: fx %s\n", track + 1, nowBypassed ? "OFF" : "ON");
    }
  }

  // The four drum pads play the drum sounds routed to them (see DrumMachine::padPressed()).
  for (int pad = 0; pad < dubbox::InputManager::kNumDrumPadButtons; ++pad) {
    if (inputManager.drumPadPressed(pad)) audioEngine.drums().padPressed(pad);
    if (inputManager.drumPadReleased(pad)) audioEngine.drums().padReleased(pad);
  }
}

void printStatus() {
  bool playing[dubbox::kNumTracks];
  for (int i = 0; i < dubbox::kNumTracks; ++i) playing[i] = audioEngine.isTrackPlaying(i);
  Serial.printf(
      "faders: %.2f %.2f %.2f %.2f | playing: %d%d%d%d | cpu: %.0f%% (peak %.0f%%) | mem: "
      "%d/%d (peak %d) | loop: %lu us (max) | drums: %d bpm, %d clips, %lu hits, %d inst | peaks: %.2f %.2f %.2f %.2f\n",
      inputManager.faderValue(0), inputManager.faderValue(1), inputManager.faderValue(2),
      inputManager.faderValue(3), playing[0], playing[1], playing[2], playing[3],
      audioEngine.cpuUsagePercent(), audioEngine.cpuUsagePercentMax(),
      audioEngine.audioMemoryUsage(), dubbox::AudioEngine::kAudioMemoryBlocks,
      audioEngine.audioMemoryUsageMax(), static_cast<unsigned long>(loopMaxUs),
      audioEngine.drums().bpm(), audioEngine.drums().clipCount(),
      static_cast<unsigned long>(audioEngine.drums().hitCount()),
      audioEngine.drums().instrumentCount(),
      audioEngine.trackPeakLevel(0), audioEngine.trackPeakLevel(1), audioEngine.trackPeakLevel(2),
      audioEngine.trackPeakLevel(3));
  audioEngine.resetCpuUsagePercentMax();
  loopMaxUs = 0;
}

}  // namespace

// A hard fault leaves a report behind across the automatic reboot; it is printed once, a
// few seconds after boot so a monitor has time to attach.
bool crashReportPending = false;

FLASHMEM void setup() {
  Serial.begin(115200);
  crashReportPending = static_cast<bool>(CrashReport);
  pinMode(kHeartbeatPin, OUTPUT);

  espLink.begin(&projects);
  inputManager.begin();

  if (!audioEngine.begin()) {
    // Fast blink = fatal init error, distinct from the normal heartbeat. The reason is
    // re-printed every second: USB serial isn't buffered, so a monitor that attaches late
    // would otherwise miss it.
    uint32_t lastErrorPrintMs = 0;
    while (true) {
      digitalWrite(kHeartbeatPin, (millis() / 100) % 2);
      uint32_t now = millis();
      if (now - lastErrorPrintMs >= 1000) {
        lastErrorPrintMs = now;
        Serial.println(audioEngine.lastError());
      }
      delay(50);
    }
  }
  Serial.println("Audio engine ready.");

  // Exposes the SD card as an MTP "disk" over the same USB connection, so WAVs can be
  // dragged onto it from a PC. The SD is already mounted by AudioEngine::begin().
  MTP.begin();
  MTP.addFilesystem(SD, "Dub-Box SD");
  Serial.println("MTP ready — SD card should appear as a drive over USB.");

  // No tracks are loaded at boot: a project (created or opened from the ESP32 display)
  // decides which WAV sits on which track.
  projects.begin(&audioEngine);
  {
    char existing[dubbox::kMaxProjects][dubbox::kProjectNameMax + 1];
    if (projects.listNames(existing, dubbox::kMaxProjects) == 0 && SD.exists(kTrackFiles[0])) {
      projects.seedFromFiles("DEMO", kTrackFiles);
    }
  }

  // Every track starts with its fx bypassed; this just seeds each remembered parameter so an
  // effect is already at a sensible setting the first time it is turned on. Track 0/2 are
  // Delay's/Freeverb's default owners.
  audioEngine.setDelayTimeMs(0, delayTimeMs);
  audioEngine.setDelayFeedback(0, delayFeedbackPercent / 100.0f);
  audioEngine.setFreeverbParams(2, kFreeverbPresets[freeverbPresetIndex].roomsize01,
                                kFreeverbPresets[freeverbPresetIndex].damping01);
  for (int i = 0; i < dubbox::kNumTracks; ++i) {
    audioEngine.setFlangeParams(i, kFlangePresets[flangePresetIndex[i]].offset01,
                                kFlangePresets[flangePresetIndex[i]].depth01,
                                kFlangePresets[flangePresetIndex[i]].rateHz);
    audioEngine.setChorusVoices(i, kChorusVoicePresets[chorusVoicePresetIndex[i]]);
    audioEngine.setTrackGain(i, 1.0f);  // replaced by the live fader reading every loop()
  }
}

void loop() {
  uint32_t loopStartUs = micros();

  if (crashReportPending && millis() > 6000) {
    crashReportPending = false;
    Serial.println("=== CRASH REPORT from the previous run ===");
    Serial.print(CrashReport);
    Serial.println("=== end of crash report ===");
  }

  // MTP shares the SD card with audio streaming, and servicing it while tracks play locks
  // the Teensy up (audible as a buzz), so it only runs while paused and not while a
  // background scan is reading a file (which it can make fail).
  if (!audioEngine.isPlaying() && !peakScanner.active() && !beatScanner.active() && !espLink.uploading()) MTP.loop();

  inputManager.update();
  float faders[dubbox::kNumTracks];
  for (int i = 0; i < dubbox::kNumTracks; ++i) {
    faders[i] = inputManager.faderValue(i);
    audioEngine.setTrackGain(i, faders[i]);
  }
  audioEngine.updateTrackCropGates();
  audioEngine.updateTransport();
  espLink.update(audioEngine, faders);
  projects.update();
  refreshScansForChangedTracks();
  runBackgroundScans();
  handleFxControls();

  // See wasStoppedLastLoop: mute the delay feedback the instant playback runs off the end.
  bool isStoppedNow = audioEngine.isStopped();
  if (isStoppedNow && !wasStoppedLastLoop) {
    audioEngine.setDelayFeedback(audioEngine.delayOwnerTrack(), 0.0f);
    Serial.println("Transport: tracks ran off the end, muting delay feedback");
  } else if (!isStoppedNow && wasStoppedLastLoop) {
    audioEngine.setDelayFeedback(audioEngine.delayOwnerTrack(), delayFeedbackPercent / 100.0f);
  }
  wasStoppedLastLoop = isStoppedNow;

  uint32_t now = millis();
  if (now - lastHeartbeatMs >= kHeartbeatIntervalMs) {
    lastHeartbeatMs = now;
    heartbeatOn = !heartbeatOn;
    digitalWrite(kHeartbeatPin, heartbeatOn);
  }
  if (now - lastStatusPrintMs >= kStatusPrintIntervalMs) {
    lastStatusPrintMs = now;
    printStatus();
  }

  uint32_t loopUs = micros() - loopStartUs;
  if (loopUs > loopMaxUs) loopMaxUs = loopUs;

  delay(5);
}
