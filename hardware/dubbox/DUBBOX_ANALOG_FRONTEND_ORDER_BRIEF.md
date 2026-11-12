# Dub-Box — Analog Front End / Phantom Power / PSRAM / Internal Mic / Bluetooth Wiring

**Purpose of this file:** a focused handoff for the hardware just ordered (Mouser cart, see below)
— what each part is for and exactly how it wires into the rest of the Dub-Box. Paste this into a
new Claude Code session alongside `DUBBOX_PROJECT_SUMMARY.md` (the full project context doc) when
picking this work back up. Written 2026-09-24, immediately after the Mouser order was placed.

**Status: everything below is ordered hardware, nothing is built or tested yet.** No new firmware
has been written for any of this (the MCP23017 button migration, internal mic capture, or Bluetooth
A2DP source are all still just plans). Treat every wiring/pin claim here as "the verified plan,"
not "confirmed on hardware" — this project's own practice is to re-verify pins with a meter before
trusting them once parts are actually in hand.

---

## 1. What this order covers, in one paragraph

Two identical combo XLR/TRS input channels (mic **or** instrument, selectable per-jack), each with
its own low-noise mic preamp, switchable 48V phantom power (not populated yet — footprint only),
and a negative supply rail the preamps need to run at all. Alongside that: a GPIO reshuffle (moving
4 existing buttons onto an I2C expander) that frees up 3 Teensy pins, which get used for a new
internal digital microphone. A PSRAM upgrade for the Teensy. And the wiring (not the firmware) for
routing Bluetooth audio through the ESP32 instead of a dedicated transmitter module. One 1/4" line
output for external powered speakers.

---

## 2. Full parts list — what each thing is for

### Per input channel (×2 — build this exact chain twice)

| Part | Mouser # | Role |
|---|---|---|
| Neutrik NCJ9FI-H | 568-NCJ9FI-H | Combo XLR + 1/4" TRS jack. XLR pins carry a mic signal; the jack's own 3 switching contacts report whether a 1/4" plug is inserted, used to route between the two signal paths below. |
| INA163UA | 595-INA163UA | Mic preamp. TI's in-production replacement for the now-obsolete INA217 — same architecture, same phantom-preamp reference design, gain set by an external resistor (`G = 1 + 6000/RG`). SOIC-14, no through-hole version exists anymore. |
| TL072CP (shared, ×1 for both channels) | 595-TL072CP | Dual JFET-input op-amp, wired as a simple high-impedance buffer for the 1/4" instrument path. One chip's two op-amps cover both channels. |
| CD4066BE (shared, ×1 for both channels) | 595-CD4066BE | Quad analog switch. Two of its four switches per channel pick between the mic-preamp output and the instrument-buffer output, driven by the combo jack's own switch contact — so only one path is ever live, automatically. |
| 6.8kΩ 1% resistor ×2/channel | MFR-25FBF52-6K8 (Yageo) | Phantom power feed resistors, XLR pins 2/3 to the (not-yet-built) 48V rail. Footprint only for now — see §4. |
| 2.2kΩ resistor ×2/channel | MFR-25FBF52-2K2 (Yageo) | Bias-current return path for the INA163's two inputs — required, not optional, or output offset drifts badly. |
| 33µF 63V polarized electrolytic ×2/channel | MAL202138339E3 (Vishay/BC Components) | DC-blocking caps between the XLR pins and the preamp input. `+` terminal faces the XLR/phantom side. |
| 5Ω-class resistor (4.99Ω actual) ×1/channel | MFR-25FTE52-4R99 (Yageo) | Sets maximum gain (paired with the pot below), per TI's own reference circuit values. |
| 1kΩ potentiometer ×1/channel | P170N-EB15BR1K (BI Technologies) | In series with the 5Ω resistor, sets the adjustable gain range (~17dB to ~62dB). |

### Shared: negative rail for the preamps

| Part | Mouser # | Role |
|---|---|---|
| ICL7660SCBAZ | 968-ICL7660SCBAZ | Charge-pump voltage inverter. Generates roughly -5V from the existing +5V rail — the INA163 needs a genuine bipolar supply (±4.5V minimum) and this board had no negative rail at all before this. Feeds **both** channels' INA163s. Wire pin 1 (**BOOST**, not OSC; OSC is pin 7, leave it open) to V+: raises the pump frequency ~3.5x (about 5 kHz to 17.5 kHz), which moves switching ripple out of the audible range and trims output resistance a little. LV (pin 6) stays open at 5V. Two 10µF polarized capacitors needed (already on hand, not ordered): C1 + to pin 2, - to pin 4; C2 + to GND (pin 3), - to pin 5. **Caution (datasheet, FN3179):** output resistance is 60Ω typical / 100Ω max at 20mA, dominated by internal switch resistance, so -5V sags to about -3.8V at ~20mA. That is below the INA163's ±4.5V guaranteed minimum; bench-measure under load before relying on it. |

### Shared: 48V phantom power (footprint now, populate later)

| Part | Mouser # | Role |
|---|---|---|
| MC34063ACN | 511-MC34063ACN | Boost converter IC, generates 48V from the 5V rail. |
| MBR1060 | 821-MBR1060 | 60V Schottky diode. **Not** the datasheet's own example diode (1N5819, only 40V-rated) — this sees close to the full 48V reverse-biased every switching cycle. |
| 220µH inductor | RCR875DNP-221K (Sumida) | Main boost inductor. |
| 1nF C0G/NP0 ceramic | SR075A102JARTR2 (Kyocera AVX) | Sets ~40kHz switching frequency. |
| 0.75Ω resistor | PAC100007507FA1000 (Vishay/Draloric) | Current-sense, sets the ~406mA peak switch current limit. |
| 1.2kΩ resistor | MFR-25FBF52-1K2 (Yageo) | Feedback divider — sets Vout. |
| 43kΩ resistor | MFR-25FBF52-43K (Yageo) | Feedback divider — sets Vout (lands at ≈46V, within the official P48 ±4V tolerance). |
| 33µF 63V (shared SKU with the DC-blocking caps above) | MAL202138339E3 | Boost output capacitor. |
| 1µH inductor | RCH664NP-1R0M (Sumida) | Optional second-stage output filter — per the datasheet's own measured results, cuts ripple from 400mVpp to 40mVpp. |
| 100µF 63V | ESL107M063AG8AA (KEMET) | Optional second-stage output filter capacitor (pairs with the 1µH above). |

### Internal microphone

| Part | Mouser # | Role |
|---|---|---|
| Adafruit ICS-43434 I2S mic breakout | 485-6049 | Digital MEMS microphone, genuinely independent of the combo-jack inputs and the SGTL5000 codec — runs on the Teensy's *second* I2S bus (I2S2). Picked over the more common INMP441 for ~3dB better SNR and ~20% lower power, at the same interface/pinout. |

### Other

| Part | Mouser # | Role |
|---|---|---|
| APS6404L-3SQR-SN ×2 | 878-APS6404L-3SQR-SN | PSRAM for the Teensy 4.1, 16MB total. Verified via the real schematic to be on completely isolated pads, not sharing any net with the general header pins — no conflict with Encoder 1 or anything else already wired. |
| SJ-63013G ×1 | 490-SJ-63013G | 1/4" line-out jack, wired to the Audio Shield's `LINE_OUT`, for external powered speakers only. (Originally planned as 2 jacks — one for speakers, one for a Bluetooth transmitter module — but the transmitter was dropped in favor of routing Bluetooth through the ESP32 instead, so only one jack is needed now.) |

**Already owned, not in this order:** MCP23017, the 2 charge-pump capacitors for the ICL7660S, perfboard, enclosure/panel-mount hardware, IC sockets, the powered speakers themselves.

---

## 3. How the per-channel input actually connects (signal path)

```
XLR pins 2/3 ──┬── [6.8kΩ ×2, phantom feed — not populated yet] ── future 48V rail
               │
               ├── [33µF 63V, + toward XLR] ── INA163 inputs (pins 4/5)
               │      INA163 gain set by 5Ω + 1kΩ pot (pins 1/8 → RG)
               │      INA163 output (pin 9) ──┐
               │      [2.2kΩ ×2 bias resistors, inputs to GND]         │
1/4" TRS ──────┴── TL072 buffer (high-Z input) ── TL072 output ────────┤
                                                                        │
                                                          CD4066 switch (2 of 4 per channel)
                                                          picks mic-preamp OR instrument-buffer
                                                          output, driven by the combo jack's own
                                                          switch contact (no GPIO, no firmware)
                                                                        │
                                                          → Audio Shield LINE IN
                                                            (channel 1 → L, channel 2 → R)
```

The INA163 needs V+ = +5V (existing rail) and V- = the ICL7660S's new negative rail (shared
between both channels).

---

## 4. GPIO reshuffle: MCP23017 (already owned)

**Why:** the internal mic's I2S2 bus needs Teensy pins 2, 3, 4 — which are currently the fx-select
buttons for tracks 1-3 (`kFxSelectButton[4] = {2, 3, 4, 5}` in `pins.h`). Moving those 4 buttons
onto the MCP23017 (which this project already needed for the transport/power buttons anyway) frees
those pins.

**Wiring:**
| MCP23017 pin | Connects to |
|---|---|
| VDD | 3.3V |
| VSS | GND |
| SCL / SDA | Teensy 19 / 18 (same I2C bus as the SGTL5000 codec) |
| RESET | 3.3V (tied high) |
| A0, A1, A2 | GND (I2C address 0x20) |
| GPA0–GPA3 | The 4 fx-select buttons, one leg to GND each, internal pull-ups enabled in firmware |

**Firmware work needed (not done):** `input_manager.cpp` currently reads these 4 buttons via direct
`Bounce2` GPIO. Needs to move to I2C reads off the expander instead.

---

## 5. Internal mic wiring (I2S2)

| Signal | Teensy pin |
|---|---|
| DOUT (mic data out → Teensy in) | **5** (corrected: pin 2 is the I2S2 *output* pin, the Audio library's `AudioInputI2S2` reads data on pin 5) |
| LRCLK2 | 3 |
| BCLK2 | 4 |

Mic breakout: 3V → 3.3V, GND → GND, SEL → GND (left slot; the firmware reads the left slot only).
Teensy drives BCLK/LRCLK, the mic only listens. Pins 3/4/5 were the old fx-select 2/3/4 and 5
buttons; they're free now that those moved to the MCP23017 (§4, done, address 0x27 on this
board). `AudioInputI2S2` also routes MCLK out of pin 33 (encoder 1 A); `InputManager::begin()`
pinMode()s it back to GPIO afterward, so encoder 1 should still work — recheck it on hardware.

**Firmware work needed (not done):** add an `AudioInputI2S2` object to the audio graph and wire it
into the mix; decide whether the internal mic is its own independently-recordable source or gets
summed with the combo-jack inputs.

---

## 6. Bluetooth-via-ESP32 wiring (not the transmitter module route)

Taps the Teensy's *existing* I2S1 bus (the one already wired to the SGTL5000 codec) in parallel,
no new signals on the Teensy side:

| Signal | Teensy pin (existing I2S1) | ESP32 pin (free, input-only) |
|---|---|---|
| BCLK | 21 | 34 |
| LRCLK | 20 | 35 |
| Data (Teensy → codec) | 7 | 36 |

Both boards are 3.3V logic — no level shifting needed. **Zero new components** — this is 3 wires
only. The ESP32 pins were chosen because they're both free and input-only, which is fine since the
ESP32 only ever listens on these lines, never drives them.

**Firmware work needed (not done, and substantial):** capturing that I2S stream on the ESP32 and
encoding/streaming it as a Bluetooth A2DP source. The wiring is trivial; this part isn't. Also
still true: Bluetooth Classic and the existing WiFi-upload feature share the ESP32's one radio, so
running both at once means they compete for airtime.

---

## 7. Explicitly deferred / out of scope for this order

- **Battery power system** — buck-boost regulator, ESP32's own dedicated regulator, battery
  protection/charging. Still only sketched in `docs/power_system_schematic_brief.md`, no real parts
  chosen.
- **Internal speaker amplifier** — the project's own docs call for this to be a *third* I2S1 tap
  (a MAX98357A-class digital class-D amp), same pattern as the Bluetooth tap above, not an analog
  `LINE_OUT` connection. Not scoped or sourced yet.
- **Enclosure / panel mounting** — user has materials on hand, not detailed here.

---

## 8. Corrections made along the way (worth knowing if re-deriving any of this)

- **INA217 is obsolete** — confirmed via live Mouser stock (`Restricted Availability`), not just
  "an old datasheet." INA163 is the real, current, TI-recommended smaller-package replacement, same
  architecture, same minimum ±4.5V dual-supply requirement.
- **2N5457 (the originally-planned instrument-buffer JFET) is also obsolete/restricted.** Replaced
  with a TL072CP-based buffer instead — more robust sourcing, and a dual op-amp conveniently covers
  both channels with one chip.
- **True non-polarized electrolytic capacitors don't exist above ~50V in real stock** at the values
  needed here. The DC-blocking caps use a standard **polarized** 33µF/63V cap instead, requiring
  correct orientation (`+` toward the XLR/phantom side) since the "flip it either way" convenience
  of a non-polarized cap isn't available at this voltage.
- **PSRAM's apparent pin conflict with Encoder 1 (pins 33/34) was a false alarm** — verified via the
  actual Teensy 4.1 schematic that the PSRAM footprint is on completely isolated pads.

---

## 9. REVISED INPUT CIRCUIT (2026-10-10) — supersedes §3's CD4066 switching

Re-derived from the real TI datasheets (INA163 SBOS177D, TL072 SLOS080W, CD4066B). Two things from §2/§3
turned out not to work on this board's weak negative rail, so the design changed:

1. **TL072CP** ("all other devices" row): recommended supply is **10 V minimum total**, and input voltage
   must stay at least **(VCC-)+4 V** above the negative rail. On +5 V / about -4.4 V that is a 9.4 V supply and
   an input range of about -0.4 V to +5 V, so the buffer input must be biased at about **+1.5 V**, not 0 V.
2. **CD4066B** control pins switch relative to VSS (high needs about 70% of the supply above VSS, low needs
   about 30%). With VSS on the negative rail, 0 V is "undefined", so the jack's switch contact cannot drive it.
   **The CD4066 is no longer used** (keep it as a spare).

**New scheme: both sources feed the same INA163.** The mic arrives balanced through two coupling caps as
before. The instrument goes through a high-impedance TL072 buffer, a coupling cap and a 22 k series resistor
into **VIN+** (INA163 pin 5); the INA163's own 2.2 k bias resistor on pin 5 forms a divider (about -21 dB),
so the INA gain range (G = 1 + 6000/RG, 17 dB to 62 dB with 5 ohm + 1 k pot) covers instruments too.
The combo jack's tip break-contact shorts the buffer input to ground while no 1/4" plug is inserted (stops hum).

Per channel (build twice; channel 1 -> shield LINE IN **L**, channel 2 -> LINE IN **R**):

| From | To |
|---|---|
| XLR pin 1 | analog GND |
| XLR pin 2 | C1 33 uF/63 V (+ toward XLR) -> INA163 pin 5 (VIN+) |
| XLR pin 3 | C2 33 uF/63 V (+ toward XLR) -> INA163 pin 4 (VIN-) |
| INA pin 5 and pin 4 | each to GND through 2.2 k (R4, R5) |
| INA pins 3 and 12 | RG = 4.99 ohm in series with the 1 k pot |
| INA pin 11 (V+) | +5 V, 0.1 uF to GND |
| INA pin 6 (V-) | negative rail, 0.1 uF to GND |
| INA pin 8 (Sense) | tie to pin 9 (VO) |
| INA pin 10 (Ref) | GND |
| INA pins 1, 2, 7, 13, 14 | leave unconnected |
| INA pin 9 (VO) | shield LINE IN (L or R); the shield already has 2.2 uF series caps |
| Jack tip lug | 1 k -> 0.1 uF -> TL072 + input (ch1 pin 3, ch2 pin 5) |
| TL072 + input node | 1 MOhm to Vb (about +1.5 V) |
| Jack tip break-contact lug | GND (identify the lug with a meter; it is closed to tip with no plug) |
| TL072 - input to its output | ch1 pin 2 to pin 1, ch2 pin 6 to pin 7 (unity buffer) |
| TL072 output (pin 1 / pin 7) | 10 uF (+ toward the TL072) -> 22 k -> INA pin 5 |

Shared: TL072 pin 8 -> +5 V (0.1 uF), pin 4 -> negative rail (0.1 uF). Vb = 47 k from +5 V to Vb, 20 k from Vb
to GND (about 1.49 V), plus 10 uF from Vb to GND. Phantom feed (later): 6.8 k from the 48 V rail to XLR pins 2
and 3, exactly as in §2.

**Negative rail: one ICL7660S per INA163** (not one shared). The datasheet's Rout is 60 ohm typ at 20 mA, so a
shared rail sags to about -3.8 V at two INA163s; one per chip at roughly 10 mA each gives about -4.4 V.
Wiring (verified, FN3179): pin 1 (BOOST) -> V+, pin 2 -> C1(+), pin 4 -> C1(-), pin 3 -> GND, pin 5 -> VOUT
and C2(-), C2(+) -> GND, pin 6 (LV) open, pin 7 (OSC) open, pin 8 -> V+. C1 = C2 = 10 uF or more. Put the TL072
on channel 1's rail. The INA163's quiescent current is in the TI table (my text copy was garbled): measure it.

**Already ordered (checked against Cart_Sep24_0527PM.pdf):** ICL7660SCBAZ x2 (exactly one per INA163), INA163UA x2,
TL072CP x2, CD4066BE x2 (spare), MC34063ACN x2, NCJ9FI-H x2, SJ-63013G x2, resistors x5 each (6.8k, 4.99 ohm, 2.2k, 1.2k, 43k).

**To buy (not in the original order):** SOIC-8 to DIP adapters x2 (ICL7660S) and SOIC-14 to DIP adapters x2 (INA163),
1 MOhm x2, 22 k x2, 1 k x2, 47 k x1, 20 k x1, 0.1 uF x10, 10 uF x4 (you may own).

**Firmware:** `AudioEngine` now captures LINE IN (pin 8, `lineIn_`) and prints `in: L R` in the Teensy status
line. Monitoring the inputs through the outputs, and recording from them, are not written yet.

---

## 10. DATASHEET VERIFICATION (2026-10-11) — checked against the user's own PDFs

Checked against INA163 (TI SBOS177D), ICL7660S (Renesas FN3179), MC34063A (ST DocID5257 and TI SLLS636N),
TL072 (Diodes Inc. Rev 1) and the Neutrik NCJ9FI-H sheet.

**CONFIRMED correct:** INA163 pinout and wiring (4 VIN-, 5 VIN+, RG across 3 and 12, 6 V-, 11 V+, 8 tied to 9,
10 Ref to GND, G = 1 + 6000/RG, 2.2 k bias resistors, coupling caps); ICL7660S wiring (pin 1 BOOST to V+,
LV open, C1 + to pin 2, C2 + to GND, ROUT 60 ohm typ / 100 max at 20 mA); MC34063 pin functions (1 switch collector,
2 switch emitter, 3 timing cap, 4 GND, 5 comparator input, 6 VCC, 7 Ipk, 8 driver collector), Vout = 1.25(1+R2/R1),
Rsc = 0.3/Ipk, and a 20 mA total output estimate from the datasheet's own Ipk formula; TL072 pinout.

**PROBLEM 1 (CRITICAL): the 46 V phantom boost exceeds the MC34063's switch rating.** Absolute maximum
switch collector voltage (pin 1) is **40 V** in both the ST and TI datasheets. In a boost the switch node sits at
Vout + diode drop (about 46.5 V) every cycle, so the circuit in the phantom diagram would over-stress pin 1.
DO NOT power up the phantom stage as drawn. Options: (a) run it at 36 V or less (safe for the chip, below the
P48 standard, many but not all condenser mics still work); (b) a high-voltage external NPN switch: NOTE that ST
Figure 19 does NOT solve this, because pin 1 stays on the switch node (it is for higher current, not higher
voltage); a real fix would tie pin 1 to the supply node and let pin 2 drive the base of an 80 V+ NPN (e.g. TIP41C),
which is NOT a datasheet circuit and would need bench testing; (c) a different boost IC or a ready-made module rated
above 48 V. The diode (MBR1060, 60 V) and caps (63 V) are fine. Phantom is optional: instruments, line out and
dynamic mics do not need it.

**P24 option using only parts already ordered (not yet built):** R1 = 2.2 k (the 5th 2.2 k), R2 = 43 k gives
Vout = 1.25(1+43/2.2) = 25.7 V, so the switch node peaks near 26 V (well under 40 V). IEC 61938 P24 uses 1.2 k feed
resistors instead of 6.8 k (the cart has five 1.2 k). Output capability rises to roughly 35 mA. Many P48 condenser
mics will work at 24 V with less headroom, some will not: check the mic's rated phantom range first.

**Datasheet Figures 13 and 19 (user screenshots):** the phantom diagram matches Figure 13 exactly (Rsc between the
VIN node and the pin 7 node; 180 ohm R3 from that node to pin 8; inductor from that node to pin 1; diode from pin 1;
pin 2 and pin 4 to GND; Ct from pin 3 to GND; divider tapped into pin 5; pin 6 straight to VIN). The 180 ohm
resistor is NOT in the Mouser cart: buy a few.

**Neutrik NCJ9FI-H terminal names (user-read from the part, Neutrik convention):** XLR 1, 2, 3 and G (shell);
1/4" T (tip), TS (tip switch), R (ring), RS (ring switch), S (sleeve), SS (sleeve switch). Each XS terminal is
closed to X while no plug is inserted. CORRECTION to section 9: the instrument return needs **S (sleeve) to
analog GND**, which the section 9 diagram left out; also tie XLR G to GND. Leave R, RS and SS unconnected.

**PROBLEM 2: TL072 is far outside its rated supply.** The datasheet's recommended supply is **+/-15 V**; its
guaranteed PSRR spec covers only +/-9 V to +/-15 V; input range is guaranteed only within about 4 V of each
rail. On +5 V / about -4.4 V (9.4 V total, skewed) the guaranteed input window is only about -0.4 V to +1.0 V, so
the instrument buffer may clip or distort. Recommended fix: replace the buffer with a rail-to-rail CMOS dual
op-amp that runs from +5 V alone (candidate: MCP6002-I/P, DIP-8; datasheet NOT yet checked), single supply,
input bias at +2.5 V (100 k + 100 k divider, 10 uF to GND) instead of +1.5 V. Then only the two INA163s use
the negative rails. Pinout is the same standard dual op-amp pinout, but verify it on the real datasheet.

**Smaller items:** INA163 datasheet suggests four 1N4148 diodes per channel from each input to V+ and V- for
ESD and hot-plug protection (not in the order). The 1/4" and XLR switch-lug numbering is not in the Neutrik sheet
(drawings only): identify lugs with a meter. The connector's rated voltage is under 50 V (fine for 46 V).
