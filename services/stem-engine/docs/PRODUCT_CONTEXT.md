# STUDIO product context

Source: the STUDIO master product handoff supplied during engine development. Historical statements about saved memory, finished work, performance, or licensing are not treated as verified facts.

STUDIO is a dedicated portable musical instrument: simple at the surface, depth when deliberately opened. It has four physical lanes, each with top button, rotary push encoder, volume fader, and bottom button. Top-level areas are DUB, JAM, DAW, FILES, SETTINGS. Avoid desktop DAW density, accounts, social features, or unnecessary settings.

## Fixed stem contract

1. VOCALS: lead, backing vocals, harmonies, doubles, all vocal layers.
2. MELODY: guitar, piano, keys, synths, strings, brass, pads, leads, melodic samples, remaining harmonic/melodic middle.
3. BASS: bass guitar, synth bass, sub and 808 bass.
4. RHYTHM: drums and percussion.

Every successful split contains vocals.wav, melody.wav, bass.wav, rhythm.wav on the same sample grid. Underlying source labels are internal. Generic “other” is mapped into MELODY and never exposed as a fifth user stem. Six-source guitar+piano+other recombine into MELODY. Source attribution is an ML estimate: a kick and an 808 can overlap spectrally, and this vocabulary cannot guarantee perfect semantic assignment.

Separation happens before performance. OPEN IN DUB maps the four files to the four hardware lanes. No neural model is needed during four-file playback.

## Instrument behavior beyond this implementation

DUB: top buttons toggle selected effects without resetting settings; encoders control 0–100% intensity macros and open the FX browser on press. DESIGN exposes advanced parameters and mappings; save custom effects to My Effects. Faders control dry channel level while an independent send/return preserves existing echo and reverb tails. Bottom buttons trigger programmable one-shot samples; retrigger behavior is unresolved.

JAM: generic TRACK → SOUND → PATTERN hierarchy, any Sound on any lane. Contextual two-octave keyboard or drum pads, octave controls, key/scale and optional scale lock. Live looping plus deeper 16-step sequencing and sound design. Proposed physical control mapping remains provisional. XY performance is not confirmed. Sound Paks collect portable sounds, samples, drum kits, and FX presets.

DAW exists for deeper arrangement but its detailed workflow remains unresolved. FILES provides familiar storage actions, preview, Recents/Search/Recently Deleted, contextual assignment and loading. Save/Save As/Rename/Duplicate, autosave/recovery and undo are intended. Shared transport, tempo, tap tempo, metronome/count-in and eventual MIDI clock. Main output differs from headphone/preview. Master stays minimal, with volume/meters/clipping/safety limiter.

Themes change visual tokens, never workflow: Dark, Bright, Retro, Win, Aero, Analog, Terminal, Pixel. Settings cover Audio, MIDI, Storage, Display, Controls, System, About. The product should feel like an instrument rather than a computer.

## Current task boundary

This repository implements local Mac stem preparation, CLI, Python API, adapters, correctness checks, experimental staged/ensemble strategies, and benchmarking. It does not implement STUDIO UI, Teensy firmware, real-time DSP, DAW, or the rest of the instrument.

Apple Silicon Mac is the initial ML host. Future application processor/NPU may perform separation; Teensy 4.1 is intended for playback, mixing, effects, synthesis, sequencing and I/O. Model licensing is recorded for provenance, not optimized as the current prototype constraint. A future trained STUDIO model should produce the four canonical stems directly using categorized, authorized multitracks.
