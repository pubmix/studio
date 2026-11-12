# Bench test files

Optional: to seed a "DEMO" project on first boot, copy 4 WAV files to the **root** of the microSD card, named exactly:

```
TRACK1.WAV
TRACK2.WAV
TRACK3.WAV
TRACK4.WAV
```

Format: 16-bit PCM, 44.1kHz, mono or stereo (`AudioPlaySdWav` supports both).
Files in this `data/` folder are for reference only — PlatformIO does not
copy them to the device; they need to be moved onto the SD card by hand
(card reader, drag-and-drop) before each bench test.
