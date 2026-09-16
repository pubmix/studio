# Product specification and provenance

Source: [Dub Box Layout](chatgpt-conversation://6aa8c80d-5b64-83e9-ab30-bf6dec871a3c), 60 available turns retrieved in six pages. Turn identifiers below permit precise retrieval. Later user corrections override earlier assistant descriptions. The source's master handoff is a useful synthesis, not independent proof that every suggestion was approved.

## Locked requirements

| ID | Requirement | Source turn / evidence |
|---|---|---|
| P01 | Touchscreen above four lanes, each top button → push rotary encoder → volume fader → bottom button | 99356c3f-41bb-4464-98c8-d87204981cd7; current brief |
| P02 | Exactly VOCALS, MELODY, BASS, RHYTHM in that order; no public “Other” stem | 14efabc8-80a6-4bc6-9092-ac29fc6ebcca |
| P03 | One lane corresponds to one channel; a lane may contain stereo audio | 80d44c6d-1400-4bac-b332-43bc91b2c15c; stereo is engineering interpretation, not four mono-only tracks |
| P04 | Dub top button toggles selected FX; encoder rotates intensity 0–100%, press browses/selects effects | ca2baffd-2f4a-4168-a0a0-31021e57ecbe; 170664c5-ae0e-41c1-b345-185453483f87; d19f8206-e2a0-415f-9739-9b65f5dbd118 |
| P05 | Fader-down removes source volume without abruptly killing an existing FX tail | 51251811-8e69-4137-b6f7-6332c96f2008 |
| P06 | Four programmable one-shot buttons; playback continues after release; import WAV/MP3 | 8cd2d985-e9a0-4bd7-bfbc-83de0b18e371; f7fb6087-af70-4e53-bc19-b0b643a29fbc |
| P07 | Jam tracks generic; saved sounds load on any lane; touchscreen keyboard/drum interaction and stylus precision | 8ff64068-380d-44b4-a3be-53c44bf4975e; 04399886-87e2-4c76-a88e-da982aaa25d2 |
| P08 | Dedicated instrument; purpose-specific screens; deep editing deliberately opened | a0b7e39a-03e1-441d-bf3d-5247f4055732 |
| P09 | Files exposes familiar operations over Teensy memory card | e76059cd-ee8b-441a-9d08-53ee4968c71a |
| P10 | Transport/tempo, undo, master metering/limiter, headphone preview, recovery, audio/MIDI settings and count-in | 2990c4d3-c840-42ab-b1cc-37a70ab6e17d approves preceding suggestions |
| P11 | Local controllable Stem Engine, prepared synchronized stems; prototype on Apple Silicon; firmware portable to Teensy 4.1 | fa933d6f-f712-4770-8f74-80317e8af988; d9b4ff4b-afb6-41c8-a174-372d6c3364c7; 61c5b48b-1d26-4730-b74e-59a7ecfa4f96 |
| P12 | Eight themes preserve workflow/layout | f98821dd-64dc-4f38-bb49-e3258a3cc2eb; 043de16a-8d01-4db5-9e0d-ad69d5c081bb; 2c3c9955-6cf1-4291-b38d-fe0a91ef0463 |
| P13 | Dub, Jam and DAW are separate environments; default controls carry over unless overridden | 7f249656-9c92-4b84-976d-eb858809650b; 4d271d1c-a666-4f8f-ad10-dbda44b26ae4 |

## Recovered conceptual scope

Home: DUB / JAM / DAW / FILES / SETTINGS. Jam uses TRACK → SOUND → PATTERN, a two-visible-octave keyboard with octave changes, key/scale and optional scale lock, contextual drum pads, live looping and a deeper 16-step sequencer. Sound/FX DESIGN pages expose editable parameters and macro mappings. Sound Paks group sounds, kits, samples and effects. Files includes import/export, preview, assignment, familiar save/rename/duplicate and recovery workflows. Approved theme names: Dark, Bright, Retro, Win, Aero, Analog, Terminal, Pixel.

The detailed master synthesis is source turn d9e7fa79-9238-4d6e-af2e-36b436306c86. These features create hardware capacity obligations, but they do not specify unlimited simultaneous voices/effects.

## Open product decisions, not hardware facts

- DAW workflow, maximum track/voice/loop lengths and simultaneous effects.
- Exact Jam button overrides, sample retrigger/voice stealing and whether sample slots follow lane faders.
- Whether fader-down stops new FX excitation or only removes dry audio; existing tails must survive either way.
- Whether FX OFF preserves existing tails; source explicitly fixes fader behavior, not every bypass case.
- Display size/resolution, capacitive versus resistive touch, active stylus, palm rejection and pressure sensitivity. “Stylus” does not establish active-pen requirements.
- Audio input connector count, balanced versus unbalanced ports, microphone/instrument input and phantom power.
- Battery runtime, charging speed, enclosure dimensions, price ceiling, thermal/acoustic limits.
- Exact user-facing sample-rate/buffer options; a Settings concept is not authorization to expose unsupported rates.
- Wi-Fi was suggested for a superseded Fadr architecture. It is not required for local separation.
- Onboard separation turnaround target. Propose no slower than song duration for a “Fast” profile, and explicitly allow slower quality preparation only after product agreement.

No additional transport buttons or master knob are added to the fixed surface. The power/service control is separate from lane interaction. No cloud accounts, social functions, browser, plugin marketplace or desktop multitasking are introduced.

