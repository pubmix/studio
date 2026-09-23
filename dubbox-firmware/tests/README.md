# Pan audio host checks

Run `python3 dubbox-firmware/tests/test_pan.py` from the repository root on this Mac.
It extracts the actual pan and gain-update methods from audio_engine.cpp and compiles
them with capture mixers and interrupt stubs under AddressSanitizer/UBSan. This
checks center unity, dry/wet hard-left/right, clamping, independent tracks, fader/crop
gains and independent effect returns at zero fader, bypass and crop. It does not simulate Teensy audio scheduling or
replace physical listening/SD persistence tests. The script uses the installed
macOS command-line SDK and clang++. All checks passed on 2026-09-23.
