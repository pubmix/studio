#pragma once
#include <Arduino.h>

#include "audio_engine.h"
#include "project.h"
#include "waveform.h"

namespace dubbox {

// Line-based UART link to the ESP32 display (Serial1: pin 0 = RX, pin 1 = TX).
//
// Teensy -> ESP32:
//   S,<playing 0/1>,<positionMs>,<timelineExtentMs>        ~20 Hz
//   C,<track>,<fileLenMs>,<n>,<start0>,<end0>,<offset0>,...   clips: file window + timeline offset each, on change, every 2 s
//   I,<beat toggle mask, bit per track>                     on change, every 2 s
//   M,<fader0..3 permille>,<peak0..3 permille>              ~15 Hz
//   F,<track>,<bypass>,<wetPermille>,<activeSlot>,<canAddFlange>,<canAddChorus>,<t0>,<t1>,<t2>
//                                                           on change, every 2 s (type -1 = none)
//   W,<track>,<totalCols>,<offset>,<count>,<hex peaks>      after a waveform is ready / on H
//   U,<volumePermille>                                      master headphone volume, on change / 2 s
//   B,<track>,<bpm x100>,<firstDownbeatMs>,<barMs x10>      beat grid (all zeros = none), on change / on H
//   J,<open 0/1>,<projectName>                              on change, every 2 s
//   L,<index>,<count>,<projectName>                         project list, on change / on G
//   p,<pattern>,<row0>,...,<row7>                          drum pattern: one 4-hex-digit step mask per row
//                                                           (bit n = step n), on change / every 2 s
//   y,<n>,<pat0>,<bar0>,<len0>,...                          pattern clips on the timeline, on change / every 2 s
//   r,<bpm>,<preview 0/1>,<previewPattern>,<previewStep>,<record 0=no 1=recording 2=count-in>,<click 0/1>
//                                  drum tempo + loop-preview / record state, on change / every 2 s
//   w,<count>                      number of synth instruments (0-4), on change / every 2 s
//   q,<pattern>,<inst>,<n>,<midi>:<start>:<len>,...  one instrument's notes in a pattern, on change / every 2 s
//   g,<inst>,<14 synth settings>   an instrument's patch (wave1,wave2,oct2,detune,mix,attack,decay,sustain,release,
//                                  cutoff,resonance,chorus,reverb,level), on change / every 2 s
//   u,<row0>,<row1>,<row2>,<row3>,<padMode 0=drums 1=piano>,<note0>,<note1>,<note2>,<note3>
//                                  what the 4 pads play (drum row or -1; MIDI note), on change / every 2 s
// ESP32 -> Teensy:
//   H                              hello: resend everything (waveforms included)
//   P                              toggle play/pause
//   Z                              rewind to start
//   B,<ms>                         seek every track to a position (playing or paused)
//   U,<volumePermille>             set the master headphone volume
//   K,<track>,<clip>,<startMs>,<endMs>   set one clip's file edges
//   E,<track>,<ms>                 snip: split the clip at that file position (negative = undo all snips)
//   M,<track>,<clip>               undo a snip: join the clip with the next one (refused if moved apart)
//   L,<track>,<clip>,<offsetMs>    move one clip on the timeline (clamped to its neighbours and time 0)
//   I,<track>,<0/1>                show/hide the beat markers for a track (saved with the project)
//   A,<track>,<fxType>             add an fx slot
//   X,<track>,<slot>               select the active fx slot
//   V,<track>,<wetPermille>        set the fx chain wet level
//   Y,<track>,<0/1>                fx bypass
//   s,<pattern>,<row>,<step>,<0/1> set or clear one drum step
//   w,<pattern>                    clear a pattern
//   j,<bpm>                        set the drum tempo (40-240)
//   f,<0/1>,<pattern>[,<record 0/1>]  start / stop looping a pattern on its own (not while the song plays);
//                                  record 1 = pad presses are written into the pattern (snapped to 16th steps,
//                                  with a metronome click); for a pattern already looping it only toggles recording
//   a,<pattern>,<bar>,<lenBars>[,<index>]   add a pattern clip to the timeline (index: insert position)
//   d,<index>                      delete a pattern clip
//   m,<index>,<bar>                move a pattern clip to a bar
//   l,<index>,<lenBars>            change a pattern clip's length in bars
//   i,<inst>,<0/1>[,<type>]        add (1) / delete (0); type 0=synth, 1=modular
//   @,<inst>,<type>,<10 params>,<9 sources> modular patch transaction; same format in state reports
//   n,<pattern>,<inst>,<midi>,<start>,<len>   add a note to an instrument, or change the length of the one at that pitch and step
//   x,<pattern>,<inst>,<midi>,<start>   remove the note starting there
//   c,<pattern>,<inst>             clear an instrument's notes in a pattern
//   t,<inst>,<midi>                play a note briefly (keyboard feedback)
//   k,<pad>[,<0/1>]                press (1, default) or release (0) a drum pad (0-3) as if it were the physical one
//   g,<inst>,<14 settings>         set an instrument's whole patch (same order as above)
//   e,<midi>                       a finger is holding this piano key (-1 = released): the next pad press routes
//                                  it (piano pad mode), resent while held like h
//   o,<inst>,<midi>,<0/1/2>        on-screen keyboard: note off / on (recorded while recording) / still held
//                                  (resent every ~150 ms; a note not refreshed for 0.6 s is released)
//   z,<0/1>                        sustain pedal up / down (resent while down, same 0.6 s rule)
//   b,<0/1>                        metronome click on/off during recording
//   v,<0/1>[,<inst>]               pads control drums (0) or notes (1) of instrument <inst>
//   h,<row>                        a finger is holding this drum row (-1 = released); the next pad press
//                                  routes it to that pad. Resent while held (the hold expires after ~1 s)
//   G                              send the project list
//   N,<name>                       create a blank project and open it
//   O,<name>                       open a saved project
//   Q                              close the open project (stops playback)
//   D                              list the SD card's WAV files (only while paused)
//   T,<track>,<filename>           put a WAV on a track
//   R,<track>                      empty a track
// Teensy -> ESP32 (reply to D):
//   E,<index>,<count>,<filename>   count -1 = busy playing, 0 = none
// WiFi upload of a WAV file to the SD card root (the ESP32 relays what a browser sends):
//   S,<sizeBytes>,<name>           ESP32 -> Teensy: start (name without extension, letters/digits/-/_)
//   W,<seq>,<len>,<crc16 hex>      ESP32 -> Teensy: a chunk header; exactly <len> raw bytes follow the newline
//                                  (len <= 1024, seq counts from 0, crc16 = CRC-16/CCITT-FALSE of the bytes)
//   C                              ESP32 -> Teensy: cancel (the partial file is deleted)
//   Q,ok,<filename>                Teensy -> ESP32: upload accepted (the name may differ from the requested one)
//   Q,ack,<n>                      Teensy -> ESP32: chunks 0..n-1 are on the card (cumulative; the ESP32 may
//                                  keep a few chunks in flight)
//   Q,done,<filename>              Teensy -> ESP32: the whole file arrived
//   Q,err,<busy|size|sd|seq|crc|timeout>   Teensy -> ESP32: refused / aborted (the partial file is deleted)
class EspLink {
 public:
  void begin(ProjectManager* projects);
  void update(AudioEngine& engine, const float faders[kNumTracks]);
  // True while a WiFi upload is writing to the SD card (the card must not be shared with MTP or playback).
  bool uploading() const;
  // Registers a track's finished peak envelope (pointer must stay valid) and
  // queues it for sending.
  void setWaveform(int track, const uint8_t* peaks, int columns);
  void clearWaveform(int track);
  // Beat grid found for a track (BeatInfo{} = none); queued for sending.
  void setBeats(int track, const BeatInfo& beats);

 private:
  void handleLine(AudioEngine& engine, const char* line);
  bool sendLine(const char* line);
  void sendWaveChunks();
  void sendProjectList();

  void sendFileList(AudioEngine& engine);

  void startUpload(AudioEngine& engine, const char* args);
  void finishChunk();
  void abortUpload(const char* reason);
  void queueReply(const char* line);
  void serviceUpload();
  size_t binLeft_ = 0;  // raw upload bytes still to read after a W line
  size_t binPos_ = 0;
  uint32_t binSeq_ = 0;
  uint16_t binCrc_ = 0;
  char pendingReply_[72] = {0};  // a Q line the UART could not take yet

  bool filePending_ = false;
  int fileSendIndex_ = 0;
  int fileCount_ = 0;  // -1 = busy (playing)

  ProjectManager* projects_ = nullptr;
  bool listPending_ = true;
  int listSendIndex_ = 0;
  int listCount_ = 0;
  char listNames_[kMaxProjects][kProjectNameMax + 1];
  int lastOpen_ = -1;
  int lastVolume_ = -1;
  char lastName_[kProjectNameMax + 1] = {0};

  char rx_[80];
  size_t rxLen_ = 0;
  uint32_t lastStateMs_ = 0;
  uint32_t lastMixMs_ = 0;
  uint32_t lastRefreshMs_ = 0;
  uint32_t lastClipSig_[kNumTracks] = {0, 0, 0, 0};
  int lastBeatMask_ = -1;
  int lastPan_[kNumTracks] = {2000,2000,2000,2000};
  uint32_t lastFxSig_[kNumTracks] = {0, 0, 0, 0};
  uint32_t lastPatSig_[DrumMachine::kNumPatterns] = {0};
  uint32_t lastPatClipSig_ = 0;
  uint32_t lastPadSig_ = 0;
  uint32_t lastInstSig_ = 0;
  uint32_t lastModSig_[DrumMachine::kMaxInstruments] = {0};
  uint32_t lastSynthSig_[DrumMachine::kMaxInstruments] = {0};
  uint32_t lastNoteSig_[DrumMachine::kNumPatterns][DrumMachine::kMaxInstruments] = {{0}};
  uint32_t lastRhythmSig_ = 0;

  const uint8_t* wave_[kNumTracks] = {nullptr, nullptr, nullptr, nullptr};
  int waveCols_[kNumTracks] = {0, 0, 0, 0};
  int waveOffset_[kNumTracks] = {0, 0, 0, 0};
  bool waveQueued_[kNumTracks] = {false, false, false, false};
  BeatInfo beats_[kNumTracks];
  bool beatsQueued_[kNumTracks] = {false, false, false, false};
};

}  // namespace dubbox
