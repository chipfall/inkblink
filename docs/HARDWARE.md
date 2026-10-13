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

## Battery indicator setup

The battery voltage pin is not documented by the manufacturer, so the indicator stays hidden until you configure it once:

1. Plug in a charged LiPo battery.
2. Open **Info** and press OK (*PINS*). The screen lists candidate ADC pins with their voltage in mV.
3. The pin whose reading is roughly **1700-2150 mV** (marked with `*`) is the battery divider. Put its number into `PIN_BAT_ADC` in `board_crowpanel.h` and re-upload. If the divider ratio differs from 2.0, adjust `BAT_DIV_RATIO`.
4. The percentage uses a LiPo curve (4.2 V = 100 %, 3.7 V = 28 %, 3.3 V = 0 %). While USB power is connected the reading reflects charging, not the cell alone.

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
