/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// This is a project-local fork of PJRC's AudioPlaySdWav (Teensy Audio
// Library, play_sd_wav.h/.cpp), adding a seek() method the upstream class
// doesn't have. It's a full copy rather than a subclass because every
// field seek() needs (wavfile, buffer_length/offset, data_length,
// total_length, bytes2millis, state, leftover_bytes) is private in the
// original, with no protected access point to build on.
//
// Deliberately vendored into the project (not patched in place under
// .platformio/packages) — that global package cache is shared machine-wide
// and gets silently overwritten on any framework reinstall/update, which
// would make the seek feature vanish without an obvious cause. Keeping the
// fork in src/ means it travels with the project and survives a fresh
// PlatformIO environment.
//
// See audio_play_sd_wav_seekable.cpp's seek() for what was added; the rest
// (begin/play/stop/togglePlayPause/update/consume/parse_format/the
// is*/positionMillis/lengthMillis accessors) is unchanged from upstream.

#ifndef audio_play_sd_wav_seekable_h_
#define audio_play_sd_wav_seekable_h_

#include <Arduino.h>
#include <AudioStream.h>
#include <SD.h>

class AudioPlaySdWavSeekable : public AudioStream {
 public:
  AudioPlaySdWavSeekable(void)
      : AudioStream(0, NULL), block_left(NULL), block_right(NULL) {
    begin();
  }
  void begin(void);
  bool play(const char *filename);
  void togglePlayPause(void);
  void stop(void);
  bool isPlaying(void);
  bool isPaused(void);
  bool isStopped(void);
  uint32_t positionMillis(void);
  uint32_t lengthMillis(void);
  // Jumps playback to `ms` milliseconds into the file. A no-op if the WAV
  // header hasn't finished parsing yet or the file isn't open (i.e.
  // isStopped() would be true) — otherwise works whether currently
  // playing or paused. Not present in upstream AudioPlaySdWav, which has
  // no seek capability at all.
  void seek(uint32_t ms);
  virtual void update(void);

 private:
  File wavfile;
  bool consume(uint32_t size);
  bool parse_format(void);
  uint32_t header[10];    // temporary storage of wav header data
  uint32_t data_length;   // number of bytes remaining in current section
  uint32_t total_length;  // number of audio data bytes in file
  uint32_t bytes2millis;
  // Absolute file offset of the first byte of PCM data — captured once,
  // the moment header parsing finishes (see consume()'s STATE_PARSE3
  // case), so seek() can compute an absolute file position from a
  // data-relative byte offset without needing to re-derive it.
  uint32_t data_start_file_pos = 0;
  audio_block_t *block_left;
  audio_block_t *block_right;
  uint16_t block_offset;    // how much data is in block_left & block_right
  uint8_t buffer[512];      // buffer one block of data
  uint16_t buffer_offset;   // where we're at consuming "buffer"
  uint16_t buffer_length;   // how much data is in "buffer" (512 until last read)
  uint8_t header_offset;    // number of bytes in header[]
  uint8_t state;
  uint8_t state_play;
  uint8_t leftover_bytes;
};

#endif
