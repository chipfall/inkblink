# Roadmap

Ideas and open items, roughly in the order they matter. Nothing here is a promise.

**Near term**
- Check the first release build from CI on the device (browser installer, update keeps books)
- Battery: a week of reading to tune the estimate; optional board modification (voltage divider or fuel gauge)
- README photos / GIF, enclosure (separate files, separate license)
- Review the fonts on the actual screen and drop the weak ones
- Larger QR code mode for easier scanning
- PlatformIO build; translate the test messages to English

**Features**
- KOReader progress sync (needs home WiFi credentials and a matching file identity; converted books are a different file than the original EPUB, so filename matching or storing the original would be needed)
- Compressed book storage (block-wise deflate) for 2-3 times more books
- EPUB: keep italics/bold as simple markers, better handling of tables and footnotes
- Line justification, per-book settings
- Second board: LilyGo T5 2.13" through GxEPD2 (`board_lilygo.h`)

**Hardware**
- Enclosure with relocated MENU/EXIT buttons (levers or separate caps), openings for USB-C / BOOT / RESET, battery bay
