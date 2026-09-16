# STUDIO UI

Working browser preview of the STUDIO performance interface. Keeps the touchscreen above the fixed VOCALS / MELODY / BASS / RHYTHM lanes, each ordered FX button → push knob → dry fader → one-shot button.

## Run

No dependencies or build step. Serve this directory with Python:

    python3 -m http.server 8765 --bind 127.0.0.1

Open http://127.0.0.1:8765 in a modern browser. Click **Load demo**, then **Play**. A user gesture starts browser audio. Space toggles playback when no control has focus.

## Implemented

- Four shared-clock Web Audio sources, play/pause/stop/seek and waveform display.
- Dry faders, pre-fader echo sends, independent decaying returns, tempo-linked echo and knob editor. FX-off stops new input; stopping stems preserves existing echoes.
- Four one-shot pads with local sample assignment; playback continues after release.
- Files page assigns synchronized audio explicitly to canonical roles. Requires equal decoded frame lengths/sample rates, caps source files at 100 MB total. Browser decoding resamples to its own audio context rate.
- JAM has an eight-note synth audition keyboard. DAW is explicitly a future-work page.
- Eight palette themes preserving layout; local saving of gain/echo/tempo/theme.
- Accessible labels, keyboard-operable native sliders and responsive controls.
- Generated 16-second demo; the VOCALS lane uses a synthetic tone, not recorded singing. No copyrighted music or remote assets.

## Files

- index.html / style.css: touchscreen and control-panel view.
- app.js: local audio graph and UI interactions.
- model.js: canonical roles, safe settings restore and alignment checks.
- model.test.mjs: dependency-free Node tests.

Run tests with `node --test model.test.mjs`.

## Validation

Five Node tests passed: canonical role order, malformed settings recovery, bounds, aligned media acceptance and missing/misaligned media rejection. Browser smoke checks verified demo generation/loading, play state, stop, echo toggle/editor and session save/reload persistence; no browser warning/error logs were reported. Visual inspection performed in the Codex browser. File picker/audio import and subjective sound quality need additional testing with real prepared media.

## Integration and limitations

This app is a browser audio prototype, not firmware and not a CM5 deployment. Teensy status is always disconnected. It sends no commands to firmware and does not mount the card, upload files, run ML separation, or create P1 media. See [system architecture](../../docs/SYSTEM_ARCHITECTURE.md) and [hardware product spec](../../hardware/architecture-v1/01-PRODUCT-SPEC.md). Their no-UI statements describe the pre-UI baseline; this preview adds a frontend only, with no device link.

Browser imports are auditions, not authoritative P1 validation: no manifest/hash/catalog handling, preserved set IDs or transfer service. Audio uses the browser's actual output sample rate, which may differ from Teensy's 44100 Hz. All browser sound shares stereo output; no independent headphone hardware path. Browser compressor is not a verified brick-wall limiter. Audio buffers, samples and transport are not restored after reload. Compressed-file size limits do not guarantee a fixed decoded memory bound; use short prepared stems for this prototype.

JAM sequencing/recording, deep stylus editing, undo/redo, Sound Paks, project audio persistence and DAW arrangement are pending. This is not the complete final product UI. No physical Jam mapping has been decided. The on-screen sliders accompanying knobs provide keyboard/touch access to rotary intensity.

Next: agree the semantic device protocol and capability reporting with firmware, bind a device adapter while retaining this preview engine, then validate prepared-set import, target display/touch performance and full editing workflows.
