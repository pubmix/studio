# Teensy pin and resource budget
Revision H0.1 • RECOMMENDED allocation; not permission to energize an unreviewed circuit.

Pin numbers are **Teensy board numbers**, not RT1062 package balls or CM5 connector numbers. Main reference: [PJRC front card](https://www.pjrc.com/teensy/card11a_rev4_web.pdf), [back card](https://www.pjrc.com/teensy/card11b_rev4_web.pdf). Installed Teensy 1.62.0 Audio/output_i2s_quad.cpp confirms its default pair is pins 7 and 32.

## 1. Complete allocation

| Teensy pin(s) | Signal / direction at Teensy | Allocation and exclusions |
|---|---|---|
| 0 / 1 | RX1 / TX1 | DIN MIDI input/output via electrical interfaces; not SPI1 alternate MISO/CS |
| 2 / 3 | Encoder lane 1 A/B inputs | Direct GPIO |
| 4 / 5 | Encoder lane 2 A/B inputs | Direct GPIO |
| 6 / 9 | Encoder lane 3 A/B inputs | Forbids extra SAI1 TX lanes on 6/9 |
| 10 / 11 | Encoder lane 4 A/B inputs | Forbids normal SPI0 CS/MOSI |
| 7 | SAI1 OUT1A output | Main DAC stereo DIN |
| 8 | SAI1 IN1 input | Stereo ADC DOUT |
| 12 / 13 | Spare, reserved SPI0 alternatives | SPI0 as a whole is unavailable while 10/11 serve encoders; pin 13 has LED load |
| 14 / 15 / 16 / 17 | A0/A1/A2/A3 inputs | Four fader wipers; blocks Wire1 default 17/16 and UART3/4 uses |
| 18 / 19 | SDA / SCL, Wire | Audio/power control, 100 kHz initially; address table below |
| 20 / 21 | LRCLK1 / BCLK1 outputs | Shared by both DACs and ADC |
| 22 | CHARGE_DISABLE output | Charger CE control, default pull-up disables charging until pack policy is valid; verify CE logic levels |
| 23 | MCLK1 output | ADC clock; reserve even if PLL/BCLK alternative is tested |
| 24 / 25 | SCL2 / SDA2, Wire2 | Panel buttons/LED driver, 400 kHz; do not confuse with Wire1 |
| 26 / 27 | MOSI1 / SCK1 reserved | SPI1 option only; would need MISO remap and conflict resolution |
| 28 / 29 | RX7 / TX7 | CM5 command/event UART; crossed TX→RX |
| 30 | POWER_FAULT_N input | Open-drain alarm aggregation only where supported; pull up to local 3.3 V |
| 31 | AUDIO_UNMUTE output | DAC XSMT gating through power-good logic; defaults muted |
| 32 | SAI1 OUT1B output | Headphone DAC stereo DIN |
| 33 | HP_ENABLE output | Amplifier shutdown/enable, proper polarity in HAL |
| 34 | POWER_GOOD input | Qualified 5 V supervisor output, not 5 V direct |
| 35 | POWER_RELEASE output | Power latch KILL via suitable logic; reset must not cause abrupt cut |
| 36 / 37 | RX8 / TX8 reserved | Service UART, not required for normal operation |
| 38 | POWER_BUTTON_EVENT input | Debounced power-controller interrupt |
| 39 | HP_PRESENT input | Jack detect; local pull-up |
| 40 | PANEL_INT_N input | Expander interrupt; scanning remains foreground |
| 41 | Spare A17/GPIO | Battery-divider sense option only after overvoltage/settling design |
| 42–47 | Native SDIO | Dedicated to onboard microSD; not free GPIO/SPI2 |
| 48–54 | QSPI memory pads | Two PSRAMs; not free UART/SPI2/I2C alternate pins |
| USB device pads/connector | D+/D−, VUSB sense | CM5 internal USB host, recovery disconnect/header |
| USB host header | D+/D−, protected 5 V, GND | External USB-A MIDI controller; no arbitrary power backfeed |
| Program, On/Off, VBAT | Dedicated functions | Accessible service pads; not counted as generic GPIO |

42 edge pins: **35 assigned, 7 reserved/spare** (12, 13, 26, 27, 36, 37, 41). Dedicated SD and PSRAM consume all 13 additional numbered pads. No pin is intentionally shared by unrelated functions.

## 2. Controls electronics

Four encoders = eight quadrature signals + four push contacts. Eight lane buttons plus four pushes = twelve switches. Use one TCA9555 at panel address 0x20:
P00–P03 top buttons lanes 1–4; P04–P07 encoder pushes; P10–P13 bottom buttons; P14/P15 spare. All twelve are active-low with external pull-ups selected for the switch's minimum contact current, not only generic weak internal pull-ups.

Direct encoder GPIO avoids losing intermediate edges in an I2C expander. Decode legal quadrature transitions; do not treat every bounce as a step. Candidate EC11E15244G1 has 15 pulses/30 detents: firmware must distinguish pulse count from detents. Begin with ~2.2 kΩ pull-ups for its stated contact-current range, then qualify 3.3 V operation and debounce with the vendor. Small RC filtering/Schmitt buffering may be required.

Faders are **control voltages**, not analog audio paths. Use 10 kΩ linear pots between local 3.3 V and ground, wiper through ~1 kΩ to ADC, initial 10 nF local capacitor. Worst-source resistance near midtravel is ~2.5 kΩ plus series R; confirm ADC acquisition settling. Calibrate endpoints, filter jitter and apply a musical gain taper in DSP. Aim for ≥10 stable effective bits and ≤5 ms settled gesture response; 12-bit register output is not 12-bit usable accuracy.

Fallback external ADC: ADS7953, SPI, with suitable reference/input buffers and redesigned pin allocation. It is not a drop-in alongside the current pin map. Do not specify ADS1115 at 860 total samples/s as though it supplies 1 kHz per fader.

Panel scanning: buttons 1 kHz foreground tick, press qualification initially 3–5 ms, release debounce separately; encoder edges captured promptly and decoded outside audio ISR. No blocking I2C inside GPIO interrupts. LED driver updates ≤100 Hz, batched. If a button read uses 45–54 I2C clocks, it takes ~113–135 µs at 400 kHz: reserve <30% panel bus utilization including status/LED writes.

## 3. Buses and addresses

| Bus | Owner / budget | Candidate address reservations (7-bit) |
|---|---|---|
| Wire 18/19 | Teensy, local audio/power; initial 100 kHz | PCM1863 0x4A subject to straps; TPA6130A2 0x60; BQ25798 0x6B; STUSB4500 0x28; smart battery 0x0B |
| Wire2 25/24 | Teensy, panel 400 kHz | TCA9555 0x20; PCA9685 LED option 0x40, disable unwanted ALLCALL responses |
| Wire1 17/16 | Unavailable | Fader conflict |
| CM5 display I2C | CM5 only | Panel controller-specific address; not on Teensy bus |
| SAI1 | Teensy master | 2 stereo TX lanes, 1 stereo RX lane, shared clocks |
| SAI2 | Unused but not freely exposed | Pins 2–5 consumed by encoders; MCLK2 33 used for amp |
| SPI0 | Unavailable in baseline | 10/11 encoder conflict; 12/13 alone are insufficient |
| SPI1 | Contingency only | 26/27 available but normal alternate MISO paths conflict; redesign required |
| SPI2 | Unavailable | SDIO/PSRAM pads consumed |
| UART1 | DIN MIDI 31,250 baud | Hardware FIFO; timestamp receipt |
| UART7 | AP link, start 1 Mbaud 8N1 | Not raw audio |
| UART8 | Debug reservation | No performance dependencies |
| USB device | Internal bulk/control | Require full-speed/high-speed detection and negotiated maximum packet handling |
| USB host | External MIDI | 500 mA initial external power allowance; host-library compatibility test |
| SDIO | Teensy card | Exclusive filesystem owner; DMA-capable driver |
| QSPI/FlexSPI2 | PSRAM | Two 8 MiB chips, shared data/clock, separate CS |
| Program flash/FlexSPI1 | Firmware execution/assets | No assumption of expansion-pad independence from cache/AXI contention |
| Ethernet/CAN/S/PDIF | No V1 allocation | Do not add PHYs/transceivers by implication |

Address straps and chip revision must be verified against the selected package before schematic release. Smart battery bus voltage and clock-stretching requirements need a qualified level-isolating bridge if incompatible with this local bus. A stuck power bus must not stall audio; transactions have timeouts, recovery and bounded retries.

## 4. DMA, timers, interrupts

[Teensy hardware capabilities](https://www.pjrc.com/store/teensy41.html) include 32 general DMA channels, but controller counts do not imply 32 independent simultaneous data paths.

| Resource | Planned reservation | Important condition |
|---|---|---|
| eDMA | SAI1 TX 1, SAI1 RX 1; 2 contingency = 4 planned | Let driver allocate channels; record actual DMAMUX assignments |
| USDHC ADMA | SD transfer descriptors | Controller DMA is not automatically another general eDMA reservation |
| USB controller DMA | Device/host endpoint buffers | Separate engines; memory/cache contention still shared |
| Fader ADC | Foreground polling first | No ADC audio; later ADC DMA/PDB/XBAR changes need timer audit |
| PIT/IntervalTimer | One short control scheduling tick | Tick sets flags; no scans or storage inside tick |
| SAI audio interrupt | Highest relevant service priority | DSP deadline bounded; no logging, heap, file I/O or mutex waits |
| GPIO interrupts | Encoder transitions, panel/power flags | Lower urgency; bounded work |
| SysTick | OS/timekeeping | Never master musical transport |
| DWT cycle counter | Profiling | Extend/wrap correctly |
| Watchdog | MCU liveness | Fed after progress checks, not blindly by timer |
| PWM | External panel LED driver preferred | Avoid timer reconfiguration collateral damage |

DMA buffers require valid DMA-accessible addresses, 32-byte cache-line alignment, appropriate clean/invalidate operations and explicit producer/consumer ownership. DTCM must not be assumed DMA accessible. Confirm stock library allocations and memory sections before applying this budget.

## 5. CM5 system-level allocation

Use 3.3 V GPIO_VREF and a power-off-safe UART buffer. Internal USB host uses USB3-0-DP/DM (module pins 134/136). UART uses GPIO14/15 (module pins 55/51) with an explicitly configured device-tree route. A hardware handshake is not required; application credits bound traffic.

Reserve one MIPI DSI interface and its panel I2C, plus reset/backlight control. Keep native USB2 pins 103/105 available for USB-C data/recovery; CM5 device-mode firmware support is a validation gate. PCIe remains optional for future model/storage expansion, with its own power and reset budget.

CM5 CC pins must **not** join the external connector's CC wires when STUSB4500 owns negotiation. CM5 always receives regulated 5 V. PMIC_Enable/PWR_Button have 5 V pull-ups: use rated open-drain transistors, not direct Teensy GPIO. Respect CM5 power sequencing and avoid back-power through UART/display links. [CM5 datasheet](https://datasheets.raspberrypi.com/cm5/cm5-datasheet.pdf)
