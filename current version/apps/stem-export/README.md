# Offline stem exporter for wired Studio transfer

Implemented desktop adapter; **no USB transfer or device playback verified**. No song has been downloaded or submitted to Fadr. This component does not open USB, flash firmware, contact a separation service or write to the SD card.

## Purpose and files

`export_stems.py` converts four already-separated local files into 44.1 kHz stereo PCM16 WAVs and the existing stem-set v1 manifest. `test_export.py` tests format, relative gain, publication/refusal behavior and compatibility with the repository's prepared-set validator.

Inputs map explicitly to VOCALS, MELODY, BASS, RHYTHM. Fadr's instrumental remainder belongs in MELODY; drums/percussion belong in RHYTHM. Do not put a full instrumental mix in MELODY if it also contains bass/drums present in the other stems.

## Dependencies actually checked

On this Mac: Python 3.10, NumPy and built-in `/usr/bin/afconvert` are available. SciPy, SoundFile, imageio-ffmpeg and jsonschema are not in this Python; ffmpeg and MTP command-line tools are not on PATH. No OpenMTP or Android File Transfer app was found in `/Applications`, and no matching Homebrew libmtp/ffmpeg package was found. These checks do not prove there is no alternative installation elsewhere.

Uses macOS Core Audio's converter for float decoding/resampling and NumPy for shared gain and PCM encoding. No extra dependencies were installed. Other platforms need a separate decoder adapter; this script is Mac-specific.

## Use

From this directory:

```sh
python3 export_stems.py --vocals /path/vocals.wav --melody /path/other.wav \
  --bass /path/bass.wav --rhythm /path/drums.wav \
  --output /path/new-prepared-folder --set-id 1 --title 'Song title' --prefix song01
python3 -m unittest -v
```

Choose stems exported from the same separation job, starting at the same time. The script checks length, not musical alignment. It refuses differences beyond two resampled frames; it pads at most two end frames for rounding, never trims starts or independently normalizes stems. One gain preserves relative balance and leaves 1 dB of sample-peak headroom for both individual stems and their unity-gain sum. It never boosts quiet inputs. This is sample-peak protection, not a true-peak limiter. Conversion rounds to PCM16 without dither. Source files remain untouched.

The output contains canonical `vocals.wav`, `melody.wav`, `bass.wav`, `rhythm.wav`, `manifest.json`, conversion provenance/hashes in `export.json`, and `sd-root-files/song01_<role>.wav` copies. An existing output is refused. Processing uses memory proportional to song length; this implementation is not a streaming converter. MP3 decoding may be supported by Core Audio, but only WAV inputs were tested; converting lossy sources does not restore lossless quality.

## Critical firmware boundary

The shared manifest and `firmware/teensy/tools/validate_prepared.py` describe a prepared-set importer, **not evidence that the currently flashed Dubbox automatically imports that manifest**. Current `dubbox-firmware/src/storage.cpp` lists root-level WAVs only; `storage.h` caps the returned list at 16 files. Therefore the four files inside `sd-root-files` are the practical manual-copy candidates for the current browser. Copy them to the SD root with unique names; do not assume a nested stem folder appears or automatically assigns all four tracks. Existing root files may already fill the 16-entry list. No files should be deleted automatically to make room.

The firmware exposes `Dub-Box SD` via MTP. MTP service currently runs only while audio is paused and scans/recording are idle, per the firmware team's transfer boundary. A transfer requires the Teensy's data USB connection and compatible Mac MTP client; it is not ordinary Finder disk mounting. Do not start a transfer during flashing, recording, scanning or playback. A reliable transfer lock/resume workflow is not implemented by this offline exporter.

## Proposed wired workflow, still unverified

1. Obtain four synchronized local stem files. Fadr's official documentation distinguishes free MP3 downloads from Plus lossless WAV downloads; account entitlement has not been inspected.
2. Run the exporter into a new local folder and validate output.
3. After flashing has finished, stop playback/recording and let scanning finish.
4. Use a compatible Mac MTP client to copy the uniquely named four WAV files to `Dub-Box SD` root. OpenMTP is a candidate, **not tested with this Teensy**.
5. Read the copied files back and compare SHA-256 hashes; then refresh the device file browser and assign the four tracks in the specified order.
6. Confirm simultaneous playback and synchronization on hardware before calling the pipeline complete.

A direct SD-card reader is another wired route if MTP compatibility fails, but remove the card only with the device safely powered down.

## Validation performed

Two synthetic automated tests pass on this Mac: 48 kHz mono source conversion produces 44.1 kHz stereo PCM16, identical frame counts, common attenuation and safe summed sample peaks; output passes the existing prepared-set validator; existing folders and substantially mismatched stem lengths are rejected without publishing a partial output. No real song, MTP connection or hardware playback was tested. Next: actual synchronized source selection, transfer-client installation/compatibility check after device availability, read-back hashes, then playback verification.

Primary references: [Fadr stems guide](https://fadr.com/help/stems), [Fadr Plus WAV entitlement](https://fadr.com/plus), [official OpenMTP site](https://openmtp.ganeshrvel.com/), [official OpenMTP repository](https://github.com/ganeshrvel/openmtp).
