# Dubbox on another computer

This repository contains the matching ESP32 touchscreen firmware, Teensy audio firmware,
and Studio Stems companion. AI separation runs on the computer; the device receives
WAVs and plays four tracks. You do not need to reflash just to change computers.

## Companion: macOS / Linux

The current launcher uses POSIX file locking. macOS is hardware-tested; Linux is a
supported installation path but has not received the same hardware validation. Native
Windows is not yet supported by the companion launcher. Firmware can be built with
PlatformIO on Windows; do not assume WSL networking/USB pairing works without setup.

Install Git and `uv` using their official installers, then:

```sh
git clone https://github.com/pubmix/studio.git
cd studio
services/stem-engine/scripts/setup-server
services/stem-engine/scripts/studio-server --host 0.0.0.0 --device http://dubbox.local
```

`setup-server` creates an isolated Python 3.11 environment and installs the web,
YouTube and Demucs dependencies. Internet access and several GB of free disk are
needed. First inference downloads model weights; weights are not committed to Git.
Simple mode uses four-source Demucs; extended/dynamic can use `htdemucs_6s`.
RoFormer is optional and needs separately configured compatible checkpoints; it is
not required for this quick start. There is no new trained proprietary model.

On macOS, after setup you can double-click
`services/stem-engine/Start Studio Stems.command`. It works from the cloned folder;
it contains no paths or secrets from the development Mac. Keep it running and the
computer awake. Stop with Ctrl-C in its terminal. A local pairing key is generated
on first launch. Existing jobs/key are reused on later starts. Use `--data /path`
if you want a different private storage location.

For YouTube sources, install FFmpeg and a JavaScript runtime supported by yt-dlp
(e.g. Deno) on PATH if acquisition reports either missing. Provider restrictions
may change; see the error shown by the companion. Local WAV/MP3 upload does not
require a YouTube login. Only process audio you have permission to use.

## Pair the touchscreen

1. Join Wi-Fi on the device at MENU → WIFI UPLOAD. Connect the computer to the same LAN.
2. Open the device upload page at `http://dubbox.local`. If mDNS is unavailable, use
   the IP shown on the device and restart the companion with that exact `--device` origin.
3. In the companion browser page, reveal its pairing key. In the device upload page,
   connect the engine using `http://COMPUTER_LAN_IP:8766` and that key.
4. Select **Enable touchscreen controls on this Dub-Box**. Do not use `localhost`:
   the ESP32 needs the computer's LAN address. Allow local-network/firewall access
   to the companion on port 8766 if your operating system asks.
5. Open **MENU → STEM SPLITTER** on the device. Search YouTube or stage a file from
   the upload/companion page. Choose a split mode, confirm rights, and start.
6. When ready, choose original WAV or compressed lossless transfer and save. Assign
   the saved files with the existing track file picker.

Pair again when changing computers or when the companion IP/key changes. This is
an authenticated HTTP service for your trusted LAN; do not expose port 8766 to the
internet. Six-source output stays available on the companion; device delivery is
VOCALS, MELODY, BASS, RHYTHM. Compressed delivery restores WAV bytes on the ESP32,
so it changes transfer size, not audio quality or SD file format. Savings vary and
the serial/SD part still handles the full WAV. See [touchscreen details](../dubbox-display/STEM_TOUCHSCREEN.md).

## Build and upload new firmware

Install PlatformIO Core (or the PlatformIO IDE extension). From the repository:

```sh
pio run -d dubbox-display
pio run -d dubbox-firmware
pio device list
```

Identify the ESP32 and Teensy separately from their USB descriptions/serial numbers.
The tested device uses CP2102 ESP32 USB `10C4:EA60` and a Teensy 4.1. Ports differ
between computers; do not assume COM6 or a previous Mac port is correct. Stop serial
monitors/recorders and playback before uploading. Then substitute the actual ports:

```sh
pio run -d dubbox-display -t upload --upload-port YOUR_ESP32_PORT
pio run -d dubbox-firmware -t upload --upload-port YOUR_TEENSY_PORT
```

PlatformIO downloads board tools and libraries. A Teensy may need its Program button
pressed if automatic reboot is unavailable. Confirm the uploader succeeds, then verify
touch startup, Teensy link, and MENU → STEM SPLITTER. Do not erase flash/NVS or format
the SD card; ordinary updates preserve device settings/projects. Keep a backup of the SD.
The active firmware is in `dubbox-display/` and `dubbox-firmware/`; `firmware/teensy/`
is a separate older prototype and is not the current device application.

## Validation and limits

Before publishing, the companion suite, upload-page tests, firmware core tests,
and firmware builds were checked. The installed device completed pairing and four
compressed test-stem SD saves. See [release validation](PUBLISH_VALIDATION.md).
Physical audio quality, every OS/toolchain, and fresh model downloads on a new machine
are not claimed tested. Setup does not copy personal recordings, model caches, jobs,
Wi-Fi credentials, or pairing keys; these stay local. The optional hardware monitor
has its own [setup notes](../hardware-monitor/README.md).
