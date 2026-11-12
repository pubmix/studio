# STUDIO visual directions

Three visual studies for the existing four-track instrument. These are concept images, not implemented firmware or exact font specimens. Generated with built-in image generation; the final prompts are in [prompts.json](prompts.json).

| Direction | Character | Suggested font | Four-color pack |
|---|---|---|---|
| [Chalk](01-chalk.png) | Quiet, clear, approachable; recommended starting point | [Inter](https://rsms.me/inter/), Regular / Semibold | Chalk #F3F0E9 · Ink #23272B · Lagoon #247A73 · Clay #C27860 |
| [Graphite](02-graphite.png) | Focused dark instrument, amber highlights | [IBM Plex Sans](https://github.com/IBM/plex), Regular / Medium | Graphite #202426 · Ivory #F2EBDD · Moss #899A78 · Amber #DDA657 |
| [Petal](03-petal.png) | Warm, friendly, rounded; blush shell with forest controls | [DM Sans](https://github.com/google/fonts/tree/main/ofl/dmsans), Regular / Semibold | Porcelain #F5EFE8 · Forest #24473F · Rose #D9A7AC · Plum #78516B |

Font links are official project sources. Font recommendations are design judgments; generated lettering only suggests their character. Exact font rasterization, sizing and memory cost need checking on the display.

## Shared interaction proposal

- Project: create, open, and recent projects; one obvious way to start.
- Tracks: preserve four tracks, playlist and mixer controls, with a persistent transport and clear selected track.
- Add track: choose WAV, record audio, or drums/synth → configure the chosen source → confirm. When all four slots are occupied, explicitly choose a track to replace rather than silently adding a fifth.
- Patterns: retain drum sequencing and synth editing.
- Input: keep input selection, monitoring and recording together.
- Settings: keep WiFi upload, MIDI, Bluetooth and device controls available without crowding the main music screen.

Use one accent for selection, a separate recording state, strong text contrast, and plain labels. Preserve existing capabilities while reducing the number of decisions shown at once.

## Before implementation

The illustrations explore hierarchy and finish. They are not pixel-exact engineering specifications. The physical shells, knob counts and pad arrangements are illustrative colorway studies, not approved hardware changes. The EQ label appearing in a concept is illustrative; this proposal does not add an EQ feature. Existing Delay, Flange, Freeverb and Chorus remain the intended effects scope. Refine the selected direction against actual screens, check touch targets and contrast, rasterize the selected font, then test on the physical display.
