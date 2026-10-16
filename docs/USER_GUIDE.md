# InkBlink user guide

## Controls

The board has a **wheel** that rotates and clicks, plus two buttons next to it.

| Key | Physical control | Typical meaning |
|---|---|---|
| UP / DOWN | wheel rotation | move in lists, turn pages |
| OK | wheel click | select / confirm |
| HOME | MENU button | go to the menu, confirm in dialogs |
| BACK | EXIT button | go back |

Black tags on the right edge of the screen show what the three keys (top = HOME, middle = OK, bottom = BACK) do on the current screen. A key without a tag does nothing there.

## Screens

**Menu** - *Continue* (when there is a last book), *Library*, *Upload books*, *Stats*, *Settings*, *Info*. BACK returns straight to the book you were reading (tag **READ**).

**Library** - OK opens a book, BACK returns. Book names are shown without `.txt`; underscores become spaces.

**Reader**

| Key | Action |
|---|---|
| DOWN / UP | next / previous page (hold to accelerate) |
| OK (short) | *Go to page* |
| OK (long) | next font size (you keep your place) |
| BACK (short) | library |
| BACK (long) | **Reading settings** right away (font, size, bold, line spacing, margins, hyphenation). Inside, HOME switches between *Reading* and *Status line*; BACK returns to the book with the new layout |
| HOME (short) | menu |
| HOME (long) | sleep |

**Go to page** - UP/DOWN change the page; OK cycles the step (1, 10, 100, 1000 and, only for books imported from EPUB with a chapter list, **chapter**); HOME = GO; BACK = cancel. In chapter step the chapter title is shown and UP/DOWN jump between chapters.

**Settings** (four groups, OK enters, BACK leaves; changes are saved when you leave)

| Group | Items |
|---|---|
| Reading | Font (Classic / Helvetica / Unifont / Century), size, bold, line spacing, margins, hyphenation |
| Status line | master switch (turns all items on/off), percent, page n/N, time left, progress bar, clock in book, battery (Off / Low / Always) |
| Clock | format (Off / 24 h / 12 h), time zone, **Set time** (opens an editor for date and time) |
| Power & system | sleep after (1 / 5 / 10 / 30 min / never), sleep screen (Book / Image / Keep), page flip (Full / Fast beta), menu flip (Fast / Full), **menu font (Helvetica 12 / Unifont / Classic)**, **hotspot power (Low / Medium / High)**, full refresh every N pages, open last book at start, hotspot password (OK generates a new one) |

**Set time** - UP/DOWN change the highlighted field (year, month, day, hour, minute; DOWN increases, like in *Go to page*), OK moves to the next field, HOME saves, BACK cancels. The time is entered in the chosen time zone and the date matters: it decides whether daylight saving time applies. If the clock has lost its time (after a full power loss) the editor starts from the last known time, or from the firmware's build date.

**Upload books** - starts the reader's own WiFi hotspot. First screen: QR code to join the network (or type the name and password shown). OK switches to the second QR code (the web page). The screen also shows the number of connected phones and received requests, and the name of the last uploaded book. BACK stops the hotspot. Auto-sleep is disabled while uploading.

**Stats** - total reading time, finished books, pages read, speed (approximate words per minute), time for the current book, estimated time left.

**Info** - free space, number of books, battery, time and date, board and firmware version. With no battery pin configured, OK opens the **pin finder** (see [HARDWARE.md](HARDWARE.md)).

## Sleep and wake

HOME (long) sleeps from anywhere except the Upload screen; the reader also sleeps after the time set in *Power & system*. The sleep screen can show a card with the book title, progress and estimated time left, your own picture, or keep the current screen. **Wake with a click of the wheel (OK).** The clock keeps running through sleep, but not after a full power loss.

## Web interface

Open `http://192.168.4.1` while connected to the reader's WiFi. Anyone who knows the WiFi password can use it, so keep the password private (it is shown in *Settings → Power & system*).

- **Books**: drag & drop several files at once, delete books. `.txt` (UTF-8) is uploaded as is. **EPUB** files are converted in your browser to text plus a chapter list (`.txt` and `.toc`), so your phone or computer does the work. Images, fonts and formatting (italics, bold) are dropped; DRM-protected files do not work. The browser must support `DecompressionStream` (recent Chrome, Edge, Safari, Firefox).
- **Sleep image**: pick any picture; it is converted to a 1-bit 250 x 122 image in the browser (dithering, brightness, invert, fit/cover) with a preview.
- **Settings**: every setting from the device, including the status-line master switch and time zone.
- **Clock**: every page load sends your phone's time to the reader.

No external resources are loaded; the pages work without internet.

## Books and limits

- Names are transliterated for storage (for example `Wiedźmin.txt` becomes `Wiedzmin.txt`).
- Up to 60,000 pages per book; about 5.9 MB of storage in total with the supplied partition table.
- The page index of a book is cached per layout and rebuilt when you change font, size, margins, spacing, hyphenation or the status line (the first open after a change takes a moment on large books).
- Characters that a font does not have are replaced by simple ASCII look-alikes.

## Troubleshooting

- **Boot loop after the first upload**: enable *Erase All Flash Before Sketch Upload* once, upload, then disable it.
- **Hotspot not visible on the phone**: it can take 10-15 seconds to appear; the reader starts it before refreshing the screen. Turn the phone's WiFi off and on to force a fresh scan, and check the *Phones* counter on the Upload screen. The password is kept between sessions, so the phone remembers the network. If the network shows up on a PC as *Hidden network* or twice (once without encryption), the reader is probably too close to the receiver: set **Hotspot power** to *Low* and keep at least half a metre away. If it stays invisible, check the power supply first (WiFi start draws short current peaks; use USB or a source that can deliver at least 1 A) and look at the serial monitor (115200): `hotspot OK (proba N)` means the radio started, `proba N nieudana` or `Brownout` lines point to a failed start or a power problem.
- **QR code does not scan**: hold the phone 15-25 cm away, avoid reflections on the glossy screen, or take a photo and tap the code in the photo. You can always type the network name and password.
- **Ghost images after fast page flips**: set *Page flip* to Full or lower *Full refresh every* N pages.
- **No battery indicator**: the battery measurement pin is not configured yet, see [HARDWARE.md](HARDWARE.md).
