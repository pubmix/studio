# Reference and toolchain provenance

Product reference: “Dub Box Layout”, conversation `6aa8c80d-5b64-83e9-ab30-bf6dec871a3c`. Read the newest ten turns and all five older cursor pages through the beginning of the conversation. Earlier cloud/Fadr proposals and initial mute-button ideas were superseded by the current handoff. No ML implementation was duplicated.

Official technical references consulted:

- [PJRC Teensyduino installation](https://www.pjrc.com/teensy/td_download.html): official board-package source.
- [PJRC new Audio objects](https://www.pjrc.com/teensy/td_libs_AudioNewObjects.html): AudioStream object boundary.
- [PJRC Audio library](https://www.pjrc.com/teensy/td_libs_Audio.html): bundled library context.

Local inspection found Apple Clang 16.0.0 and Arduino IDE's bundled Arduino CLI. CMake, Ninja, PlatformIO and a Teensy core were not initially available on PATH. Installed official Teensyduino 1.62.0 and its GCC 15.2.1 toolchain into this task's isolated `work/arduino/` tree after download approval; the user's existing Arduino board installation was not replaced.

The installed Teensy core supplied Audio 1.3, SD 2.0.0, SdFat 2.1.2, SPI 1.0, Wire 1.0 and SerialFlash 0.5. No Faust, DaisySP or ML package was installed for this implementation. The delivered source has no dependency on the isolated toolchain's absolute path.
