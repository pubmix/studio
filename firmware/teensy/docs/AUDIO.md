# Audio graph and constraints

```text
Prepared VOCALS ─┬─ dry fader ─────────────────┐
Prepared MELODY ─┼─ dry fader ─────────────────┤
Prepared BASS ───┼─ dry fader ─────────────────┤
Prepared RHYTHM ─┴─ dry fader ─────────────────┤
Each source ── FX enable/send ── stateful DSP ─┤
                       wet return (no fader) ─┤
Four complete-play sample slots ──────────────┤
Jam voices / patterns / event loops ──────────┤
Metronome / count-in ─────────────────────────┤
                                             ▼
                                    Master volume
                                    Linked safety gain
                                    L/R meters + clip latch
                                             │
                          Main L/R ──────────┤
                                             ▼
Preview sample ───────────────────► Headphone/Preview sum
                                    Independent ceiling
```

Each Dub source is forked **before its fader**. The dry fader never multiplies the wet return. Turning FX off gates new input but keeps delay buffers processing. Existing echo/reverb persists after a fader cut or disabled send. While an FX send stays on, new source content still enters it even at zero dry fader; toggle FX off to let only the existing tail decay. This is deliberate pre-fader prototype routing.

The four sources share a source frame cursor. They are read only while Dub is playing and count-in has finished. A failure in any lane mutes the complete four-lane frame and advances the whole set together. Source EOF does not stop the global clock or truncate returns. No time stretching is implemented; BPM affects clock/sequencing, not a prepared song's playback rate.

## Working DSP prototypes

| Product ID | Actual implementation | Limit |
|---|---|---|
| Dub Echo | Filtered input, saturating feedback delay, width | ≤8191 samples, about 185.7 ms |
| Digital Delay | Short stereo feedback delay | Same fixed buffer |
| Room Reverb | Two-tap feedback approximation | Not a production room model |
| Low-pass Send | One-pole wet parallel path | Dry remains present |
| High-pass Send | One-pole residual wet parallel path | Dry remains present |
| Drive Send | Tanh saturation return | Parallel send effect |

Intensity maps independently to wet, feedback, drive, tone and width using linear, square, square-root or smoothstep curves. DESIGN edits these mappings and time. Feedback is capped at .95. Supported delay time is clamped to allocated memory, even though the data model reserves up to eight seconds for future implementations. No tempo-synced delay-time mapping, modulation, convolution, pitch shifting, granular engine or full effect families are included yet.

A fader cut is tested with a fed delay. FX switching presently reuses the lane's state buffer; no crossfade or algorithm-specific tail handoff is claimed. Filters are **parallel send filters**, not full channel insert filters. Those choices must be revisited with musical testing and CPU/RAM benchmarks.

## Jam

Four generic tracks each own one Sound, one 16-step row, and up to 64 captured note-on/off loop events. Eight voices per track support simple sine, unbandlimited saw, synthesized noise drum and WAV sample playback. Sample instruments use note 60 as native playback pitch and nearest-frame resampling. A one-pole filter, attack/release envelope, drive and Sound macro are supplied. This is not a high-quality sampled Rhodes, drum kit mapper or bandlimited synth library.

Two-octave input maps 24 adjacent semitones; octave moves the window. Major, Minor, Pentatonic, Blues, Dorian, Mixolydian and Chromatic are supported. Scale lock snaps to the nearest valid pitch, resolving ties downward. Loop length rounds up to a bar, with ticks relative to recording start. Event-loop capture is not microphone/audio looping. Multi-row drum patterns, automation lanes, loop overdub/erase policy, voice stealing refinement and sample interpolation remain extensions.

## Master and preview

Main stereo metering is pre-limiter absolute peak with decay; clipping latches until cleared. Master safety uses instantaneous stereo-linked gain at .98 and rejects nonfinite samples. It is a safety ceiling, not a lookahead mastering limiter. Preview goes to the headphone sum by default; `previewToMain` is explicit. The Teensy bridge exports channels 0/1 Main and 2/3 Headphones, but only the optional Main I²S connection is present. H0.1 recommends a second independent DAC on SAI1 TX32; physical driver, mute sequencing and measurements remain unimplemented. See [shared baseline](../../../docs/SYSTEM_ARCHITECTURE.md).

## Budgets

Four stereo PCM16 stems require **705,600 bytes/second** of sustained source payload, excluding one-shots, seeks and writes. Host per-WAV cache is 4096 bytes; SD per-asset cache is 2048 bytes for up to 16 registered assets. Four DSP buffers consume 262,144 bytes. Three queued 128-frame blocks provide approximately 8.7 ms of foreground scheduling margin at 44.1 kHz.

These are arithmetic/compile-time budgets, not SD or CPU measurements. Production readiness needs worst-case card-stall tests, audio callback timing, long-duration playback, thermal/power testing, voice/FX saturation, analog output inspection and click/pop measurements. Autosave can block longer than the queue margin; schedule or chunk it before live use.
