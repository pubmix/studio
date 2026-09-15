# Data model and files

| Type | Content / ownership |
|---|---|
| Project | Version, ID/name, stem set ID, BPM/master, key/scale, count-in/metronome, theme, four Dub lanes, four Jam tracks, four slot asset IDs |
| Track | Generic Sound, 16-step Pattern, LiveLoop, level and arm state |
| Sound | Asset ID, kind, macro, attack/release, cutoff and drive |
| Pattern / Step | Sixteen enable/note/velocity/gate entries; no fixed instrument taxonomy |
| LiveLoop | Up to 64 tick/note/velocity/on-off events, length, playing/capture flags |
| Fx / Mapping | Stable product effect ID, enabled/intensity/time, five endpoint/curve mappings |
| StemSet / StemAsset | Version/ID/title, four actual asset IDs, rate/frames/offset/channels/bits/filename |
| Session | Current screen/mode/lane/browser state, last music mode, dirty flag |
| Settings / Status | Device policy separate from projects; live audio/storage/MIDI/version status |
| SoundPak | Named ID and fixed arrays of Sound/sample/FX preset asset IDs; catalog boundary |
| DawRegion | Asset, track, start/length tick interface; no finalized arrangement UX |

## Project wire format

`.studio` uses magic `STU1` (little endian 0x31555453), explicitly serialized fields, and a trailing FNV-1a integrity checksum. Integer/enumeration/boolean fields use little-endian 32-bit units. Floats use IEEE-754 32-bit encoding; fixed strings include a NUL terminator. The version is 1. Field order is defined in `Persistence.cpp::Wire::project`. There is no raw structure dump, compiler padding or pointer in the file.

Decode stages into a temporary Project, checks version, byte count, checksum, enum/bool ranges, finite values and key musical bounds, then replaces the destination. Active capture is not resumed on load. Unknown versions and malformed/truncated payloads fail. The 8192-byte envelope is a prototype bound. The checksum detects corruption; it is not cryptographic authentication. Some advanced mapping endpoint ranges remain a UI-validation responsibility.

Sound/FX presets currently reuse that envelope with distinct type tags and only their relevant payload field. It is intentionally simple but space-inefficient. A future version can add compact asset-specific envelopes. Factory/My Sounds and My Effects are logical file categories; no finished preset editor is supplied.

## Storage adapters

```text
STUDIO/
  Projects/
  Sounds/Factory/
  Sounds/My Sounds/
  Samples/Factory/
  Samples/My Samples/
  Sound Paks/
  Recordings/
  Imports/
  Exports/
  System/
  Recently Deleted/
```

HostStorage creates this conceptual tree under the chosen root. It supports file read, atomic replacement, browse, name-substring search, copy/duplicate, move/rename, trash, restore, recents and capacity. Filesystem paths reject parent traversal, absolute paths and resolved symlink escapes. Browse hides System and symlinks. Copy/move do not overwrite an existing target. Trash preserves a uniquely numbered file; restore asks the caller for a destination (original-path metadata is not persisted). Recents are session-local. Recursive directory copy/move, name sorting preferences and search indexing are not implemented.

Host save writes a unique temporary file, syncs it, renames over the target and syncs the directory. Save As is the same save operation at another path. SD uses JournalStorage with `.a` / `.b` blobs: magic, generation, length, checksum over header and payload, and encoded Project. It writes the older slot, flushes, verifies, then future reads select the newest valid generation. A failed/truncated newest slot leaves the previous valid copy usable. Generation wrap is rejected.

A journal mitigates interrupted payload writes. It cannot guarantee FAT filesystem survival or SD controller durability during power loss. Physical power-cut testing remains required. The SD adapter provides read/save and prepared audio access; the host file browser's rename/search/trash UI has not been ported to a touchscreen SD browser.

## Recovery and assets

Autosave interval defaults to 30 seconds while dirty. Autosave does not clear dirty state or overwrite the user's named project. The sketch can display a recovery state and accepts serial `y` to restore it. Device Settings, navigation, undo history, sample data and runtime DSP buffers are not saved in a project. Project load does not resolve arbitrary asset IDs; a real catalog must restore the prepared set and referenced samples before playback.

Sound Paks currently have a bounded manifest data type; packaging/import/export, content dependency resolution and a graphical Pak browser remain unimplemented. Recordings and Exports folders reserve space for future audio capture/render jobs; their existence does not imply an audio recorder.
