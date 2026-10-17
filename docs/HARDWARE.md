# Hardware notes (CrowPanel ESP32 2.13" e-paper, DIE01021S V1.2)

## Board

- ESP32-S3-WROOM-1-N8R8 (8 MB flash, 8 MB PSRAM), USB-C, LiPo connector with charging circuit (SH1.0, 2 pin), UART0 header, 2x10 GPIO area
- E-paper: 122 x 250 px, 1 bit, pixel pitch about 0.227 x 0.226 mm, active area 31.2 x 63.19 mm (manufacturer's wiki)
- No SD card slot (books live in the flash file system)
- Display controller: the manufacturer lists two (SSD1680Z and JD79661). **The V1.2 board used here behaves as JD79661 with an inverted BUSY signal** and does not work with the stock source from the manufacturer's repository; see `firmware/inkblink/driver/README.md`.

## Pins used by the firmware

| Function | GPIO |
|---|---|
| Display SPI: SCK / MOSI / RES / DC / CS / BUSY | 12 / 11 / 10 / 13 / 14 / 9 |
| Display power enable (HIGH = on) | 7 |
| MENU button (HOME in the firmware) | 2 |
| EXIT button (BACK) | 1 |
| Wheel up / down / click (OK, also the deep-sleep wake pin) | 6 / 4 / 5 |
| Free GPIO header | 40, 41 |

All defined in `firmware/inkblink/board_crowpanel.h`.

## Battery

- LiPo connector: SH1.0, 2 pins (red/black wires, check the polarity marks "-" and "+" next to the connector before plugging a cell in). A pack with a third (NTC) wire and a 3-pin plug does not fit.
- Charger: TP4054 (USB-C 5 V -> LiPo). The USB-C socket goes to a CH340K USB-UART bridge, not to the native USB of the ESP32-S3.
- **The board cannot measure the battery voltage.** In the schematic the `BAT` net only connects the battery connector, the TP4054, two Schottky diodes, a PMOS and a capacitor: no voltage divider, no fuel gauge. The unused ESP32-S3 pins (GPIO3, 8, 15-18, 20, 21) are not connected to anything, which matches measurements on a real board (their readings float and do not follow the battery). Two checks that led here: with the battery plugged in, readings on those pins were the same with and without USB.
- Caveat: the schematic file in Elecrow's repository states in its description that it was generated from a 4.3-inch design, so it may not be exactly the V1.2 layout. A question to Elecrow is pending.
- Therefore InkBlink **estimates** the battery (see the user guide, "Battery estimate"). `PIN_BAT_ADC` in `board_crowpanel.h` stays at `-1`; the pin finder on the Info screen is only useful for other boards.

### Measured current draw (CrowPanel V1.2, supply 3.7 V, via the battery connector)

The current is almost independent of the supply voltage (checked at 3.4 V and 4.2 V), so the estimate is done in mAh.

| State | Current |
|---|---|
| Reading a static page / menu, processor 240 MHz | 34-35 mA |
| Fast page flip (1 s) | 47-51 mA |
| Full page flip (2 s) | 55-60 mA |
| Menu navigation (1 s) | 52 mA (58-61 mA on a full refresh) |
| Enter Upload (1 s) / hotspot idle | 129 mA / 108 mA |
| Deep sleep | not measured yet |

Consequence: reading time on a 1200 mAh cell is dominated by the idle current (about 35 h with the screen kept awake), page flips are only 1-3 % of the energy. The *Power saving* setting (80 MHz, light sleep) is there to measure how much the idle current can be reduced.

## Enclosure notes

> **Approximate values.** They were extracted from the manufacturer's STEP model with a custom parser (not a CAD kernel) and have not been checked against a physical board: verify with calipers. The model does not contain the PCB outline or the display glass as solids.

Axes: *length* runs along the long side of the display (about 63.8 mm overall), *width* across it (31.2 mm overall, connectors included), *height* is measured from the PCB's component surface. Length is measured from the tip of the wheel (L = 0).

| Feature | Position / size |
|---|---|
| Overall envelope incl. connectors | about 31.2 x 63.8 mm, about 9.1 mm thick |
| Wheel (short edge) | 13.2 mm wide, 2.8 mm thick, tip at L = 0 |
| MENU / EXIT side switches | actuators 3.0 mm wide at +/- 11.15 mm from the centre line (about 3 mm from the wheel's edge), actuator tips at L about 1.9 mm, press direction along the length axis, actuator centre about 1.7 mm above the PCB surface |
| BOOT / RESET (long edge, -width side) | centres at L about 49.5 and 60.0 mm, i.e. about 10.5 mm apart |
| USB-C receptacle (long edge, +width side) | centre at L about 34.9 mm; shell about 8.9 mm wide x 4.2 mm high (add room for the plug's overmould) |
| LiPo connector (+width side) | centre at L about 44.4 mm, about 5.2 x 4.0 mm footprint, 3 mm high |
| UART0 header (+width side) | L about 11.5-26.6 mm, up to 8 mm above the PCB |
| 4-pin HY2.0 connector (-width side) | L about 32.2-44.2 mm, up to 8.2 mm above the PCB |

Design hints:
- The MENU/EXIT switches sit very close to the wheel. Moving them away needs levers or separate printed button caps that push the side switch actuators along the length axis.
- Leave openings for USB-C, BOOT and RESET (needed for flashing) and keep metal away from the antenna end of the ESP32 module - the hotspot is a core feature.
- Keep the front frame from pressing on the display glass.
