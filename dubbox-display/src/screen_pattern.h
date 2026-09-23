#pragma once

// Pattern screen: the drum-machine step grid (DRUMS) and the synth instruments. Each instrument (SYN 1,
// SYN 2, ... added with the "+") has its own sound and its own notes in every pattern, and opens on
// three views: SOUND (presets, oscillators, envelope, filter, effects and a small keyboard), ROLL (the
// piano roll for this pattern) and KEYS (a full-width keyboard with a sustain pedal). Pads and keys
// play the instrument you are looking at. The chip row (P1-P8) picks the pattern, and the bottom
// PLAY button is split: PATTERN loops the pattern on its own, REC records into it, PLAY SONG plays
// the whole song. Patterns are placed on the song timeline from the playlist screen.
namespace pattern {

// Redraws the whole screen body onto the visible layer.
void enter();
// Called when the screen is left: stops the loop preview.
void leave();
// Call every loop. Consumes link updates; only draws when `visible`.
void update(bool visible);
void touchDown(int x, int y);
void touchMove(int x, int y);
void touchUp();
// Every finger currently down (0 when none), once per loop: the on-screen keyboard plays chords.
void touchPoints(const int* xs, const int* ys, int n);

// Fill colour of pattern `index` (0-7), used for its chip and for its clips in the playlist.
unsigned patternRgb(int index);

}  // namespace pattern
