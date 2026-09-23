# Preliminary candidate BOM
Revision 0.1 • Quantities per instrument unless marked bench-only. These are candidates, not an order or a complete procurement BOM. Passive values, protection ratings, footprints and current availability require schematic/procurement review. Costs are deliberately unquoted; no live quantity pricing was obtained.

## Compute, storage and display

| Candidate manufacturer part / product | Qty | Purpose, interface and selection reason | Alternative / status |
|---|---:|---|---|
| PJRC/SparkFun TEENSY41_NE | 1 | 600 MHz realtime controller; SDIO, SAI, USB; no Ethernet PHY needed | TEENSY41 for bench availability; RECOMMENDED |
| Raspberry Pi CM5008032 (SC1571) | 1 | 8 GB RAM, 32 GB eMMC, no wireless; UI/files/preparation candidate | CM5016032 16 GB if measured RAM requires; CM5108032 if wireless explicitly adopted |
| AP Memory APS6404L-3SQR-SN | 2 | 64 Mbit each, SOP-8, 3 V QSPI; 16 MiB combined buffer/effect storage | PJRC-qualified 8 MB PSRAM supply; qualify exact revision |
| Kingston SDCIT2/32GB | 1 candidate | Removable 32 GB industrial microSD; performance asset store | Qualify at least two card models; endurance class is not a latency guarantee |
| Raspberry Pi Touch Display 2, 7-inch version | 1 bench-only | DSI/touch module for UI timing and power tests | 5-inch version; final panel/stylus OPEN |
| Raspberry Pi CM5 IO Board | 1 bench-only | Known carrier for early AP/USB/display work | Custom carrier only after Phase B gates |

Sources: [CM5 ordering table](https://datasheets.raspberrypi.com/cm5/cm5-datasheet.pdf), [Teensy](https://www.pjrc.com/store/teensy41.html), [PSRAM datasheet](https://www.pjrc.com/store/APS6404L_3SQR.pdf), [Touch Display 2](https://www.raspberrypi.com/products/touch-display-2/), [Kingston exact card](https://www.kingston.com/en/memory/search?partid=SDCIT2%2F32GB). Current stock and lifecycle still require procurement confirmation.

Touch Display 2's portrait-native 720×1280 panel can be software-rotated for bench use; it is not a locked industrial design. A capacitive passive stylus may not meet precise step-editing needs. Display + Controls must compare active pen versus resistive precision and multitouch playing, report worst-case touch latency and select the final panel.

## Audio

| Candidate | Qty | Purpose/interface | Alternative / reason |
|---|---:|---|---|
| TI PCM1863DBTR | 1 | Two-channel ADC, I2S + I2C, PGA, 3.3 V system | TAA5212 for modern higher-performance front end; new driver required |
| TI PCM5102APWR | 2 | Two stereo I2S DACs: main and independent headphones; hardware straps | TI TAD5242, potentially integrates headphone drive |
| TI TPA6130A2RTJR | 1 | Stereo headphone amplifier, I2C volume | Integrated TAD5242 headphone stage after comparison |
| TI PCM3168APAP | 0 baseline /1 alternative | 6-in/8-out TDM codec, differential analog | More I/O/analog effort; not interchangeable with dual I2S DACs |
| PJRC TEENSY4_AUDIO (Rev D/D2) | 0 baseline /1 bench optional | SGTL5000 shield for basic 2-channel bring-up | Does not establish independent main + HP routing by itself |
| Connector/filter/coupling/mute parts | 1 set | ADC input conditioning; DAC output RF/ESD; HP attenuation | Values must follow selected datasheets and measured headroom |

Sources: [PCM1863](https://www.ti.com/product/PCM1863), [PCM5102A](https://www.ti.com/product/PCM5102A), [TPA6130A2](https://www.ti.com/product/TPA6130A2), [TAD5242](https://www.ti.com/product/TAD5242), [PCM3168A](https://www.ti.com/lit/ds/symlink/pcm3168a.pdf), [shield](https://www.pjrc.com/store/teensy3_audio.html). Ordering suffixes must be checked against distributor/package records at release.

## Controls and interconnect logic

| Candidate | Qty | Purpose/interface | Alternative / limitation |
|---|---:|---|---|
| Alps Alpine EC11E15244G1 | 4 | Quadrature push encoder; 15 pulses, 30 detents | Bench candidate; 15k rotation/20k push life and 6 N press may be inadequate for final feel/durability |
| Bourns PTA3043-2015CPB103 | 4 | 10 kΩ linear, single-gang slide pot; ADC control voltage | PTA4543-2015CPB103 longer travel candidate; both mechanical choices OPEN |
| Omron B3F-1000 | 8 | Tactile lane buttons, dry contact | B3F-1000-G microload option; production cap/feel/durability study required |
| TI TCA9555PWR | 1 | 16-bit I2C switch expander; 12 used | MCP23017 alternative with revision/errata review |
| NXP PCA9685PW,118 | 0–1 | I2C PWM for optional lane-state LEDs | Discrete driver if only a few LEDs; LED count/color not locked |
| TI SN74LVC2G125DCUR | 2 candidate | Power-off-safe UART/interface buffering | Confirm direction-specific power domain and enable sequencing; not generic voltage conversion |
| TI ADS7953SBRHBR | 0 baseline /1 alternative | External multiplexed 12-bit SPI ADC | Only if internal fader ADC fails; requires reference/buffer and pin-budget revision |
| Pull-ups, RC filters, Schmitt buffers | 1 set | Clean switches/encoders and ADC settling | Size against contact current, cable capacitance and measured noise |

Sources: [Alps exact encoder](https://tech.alpsalpine.com/e/products/detail/EC11E15244G1/), [Bourns ordering specification](https://www.bourns.com/docs/product-datasheets/pta.pdf), [Omron B3F](https://components.omron.com/us-en/datasheet_pdf/A070-E1.pdf), [TCA9555](https://www.ti.com/product/TCA9555), [ADS7953](https://www.ti.com/product/ADS7953), [buffer](https://www.ti.com/product/SN74LVC2G125). Bourns PTA's 15k cycle life is a prototype limitation, not a claim of professional mixer durability.

## Power and protection

| Candidate | Qty | Purpose/interface | Alternative / release condition |
|---|---:|---|---|
| RRC power solutions RRC2057 | 1 | Protected smart 2S pack, 7.2 V/6.9 Ah, SMBus | RRC2037 smaller-energy candidate; final pack fit/runtime OPEN |
| ST STUSB4500QTR | 1 | Standalone Type-C PD sink, I2C status/config | TI TPS25750 system-level alternative; not footprint compatible |
| TI BQ25798RQMR | 1 | Buck-boost charger/power path, I2C | BQ25792 alternative pending full datasheet review |
| TI TPS543620RPYR | 1 | 5.1 V/6 A main buck design | TPSM843620 integrated-inductor module for prototype |
| TI TPS62132RGTR | 1 | 3.3 V peripheral buck | TPS62903 adjustable alternative |
| TI TPS7A2033PDBVR | 2 | Separate low-noise 3.3 V analog LDOs | Check thermal margin; package/value variant confirmation required |
| ADI LTC2954CTS8-1#TRMPBF | 1 | Pushbutton power latch/interrupt/shutdown | Equivalent supervised latch; polarity and startup timing must be validated |
| TI TPS3808G01DBVR | 1 | Adjustable rail supervisor / power-good | Divider threshold tolerance must protect CM5 minimum voltage |
| TI TPS2553DBVR | 1 | USB host 5 V current limit/fault | Existing Teensy host protection must be accounted for, not blindly cascaded |
| TI TPD2EUSB30DRTR | 2 candidate | External USB data ESD arrays | Choose exact placement/capacitance from SI/ESD review |
| TI TPS25947 family | 0–1 | Optional low-voltage eFuse/reverse-blocking rail | Not for direct 15 V VBUS; select exact variant after rail strategy |
| PD-rated TVS, input FETs, fuse, inductors, capacitors | 1 set | VBUS surge/inrush and regulator energy storage | Orderable parts/ratings OPEN pending circuit design |
| Battery mating connector/NTC or pack thermal interface | 1 set | Pack current and smart-battery safety | Obtain exact RRC-approved mating specification; no generic low-current JST substitution |

Sources: [RRC2057](https://www.rrc-ps.cn/fileadmin/documents/Manuals/Manual_RRC2057_H.pdf), [STUSB4500](https://www.st.com/en/interfaces-and-transceivers/stusb4500.html), [BQ25798](https://www.ti.com/lit/ds/symlink/bq25798.pdf), [main buck](https://www.ti.com/product/TPS543620), [3.3 V buck](https://www.ti.com/product/TPS62132), [LDO](https://www.ti.com/product/TPS7A20), [latch](https://www.analog.com/en/products/ltc2954.html), [supervisor](https://www.ti.com/product/TPS3808), [USB current switch](https://www.ti.com/product/TPS2553), [USB ESD](https://www.ti.com/product/TPD2EUSB30), [eFuse](https://www.ti.com/product/TPS25947).

## Ports, debug and mechanical components

| Candidate | Qty | Purpose/interface | Status |
|---|---:|---|---|
| GCT USB4105-GF-A | 1 | USB-C USB2 receptacle; PD power plus proposed AP device data | [Manufacturer specification](https://gct.co/files/specs/usb4105-spec.pdf); current/retention qualification |
| USB-A host receptacle | 1 | MIDI controller connection | Exact MPN and mounting OPEN |
| DIN 5-pin sockets | 2 | MIDI IN and OUT | Exact MPN/port location OPEN |
| Vishay 6N138 | 1 candidate | MIDI receive optocoupler | Electrical timing/supply/pull-up validation required |
| Audio jacks | Main stereo pair + input stereo + HP | Main/HP independence | Connector format, balanced option and exact MPN OPEN |
| Keyed panel harness / board connectors | 1 set | Controls, power, ground | Exact current/retention and pinout follow board partition |
| Test header/pads | 1 set | Teensy Program/USB, UART, CM5 recovery, clocks/rails | Accessible without removing fragile controls |
| Heat spreader, optional controlled fan | 1 set | AP thermal path | Size/acoustic budget after representative enclosure test |

A real production BOM also needs all decoupling, ESD, resistor networks, regulator magnetics, connector mates, mechanical supports and assembly specifications. This phase establishes concrete major candidates and honest remaining procurement gaps; it is not fabrication-ready.

Optional LED controller reference: [NXP PCA9685 datasheet](https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf). All source checks in this package were made on 14 September 2026 Pacific; catalogue status does not guarantee stock.
