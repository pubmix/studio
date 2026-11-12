# Display host tests

`theme_persistence.cpp` compiles the production theme manager against the in-memory
Preferences stub. It verifies all three selections survive reload, invalid IDs
fall back safely, repeated selection avoids writes, and failed storage writes
leave the active theme unchanged. This does not validate physical NVS or touch.

From the display folder, use a C++17 compiler with `-Itests/theme-stubs -Isrc`
and compile `tests/theme_persistence.cpp src/theme.cpp`. Run the resulting binary.
Address and undefined-behavior sanitizers passed on the development Mac.
The firmware itself builds with PlatformIO; device interaction is a separate check.

`keyboard_geometry.cpp` exercises production `keyboard.cpp`: full-width drawing
bounds, black/white note hit mapping at every supported key count and octave,
three-finger chords, debounce and inactive-view note release. The Arduino stub
only supplies a controlled clock; UI drawing is bounds-checked and outgoing
notes counted. From the display folder:

```sh
c++ -std=c++17 -fsanitize=address,undefined -Itests/keyboard-stubs -Isrc \
  '-I.pio/libdeps/esp32dev/Adafruit GFX Library' \
  tests/keyboard_geometry.cpp src/keyboard.cpp -o /tmp/studio-keyboard-test
/tmp/studio-keyboard-test
```

Passed on the development Mac. Hardware touch and combined firmware build are
separate checks.
