# Roadmap

Ideas and open items, roughly in the order they matter. Nothing here is a promise.

**Near term**
- Settle the driver licensing with the manufacturer, then include the driver (or replace it with independent code) so the repository builds out of the box; add a compile check for the ESP32-S3 target to CI
- Check battery measurement on real hardware and ship the confirmed pin
- Review the fonts on the actual screen and drop the weak ones
- Manual time setting on the device (for use without the web page)
- Larger QR code mode for easier scanning
- Translate code comments to English

**Features**
- KOReader progress sync (needs home WiFi credentials and a matching file identity; converted books are a different file than the original EPUB, so filename matching or storing the original would be needed)
- Compressed book storage (block-wise deflate) for 2-3 times more books
- EPUB: keep italics/bold as simple markers, better handling of tables and footnotes
- Line justification, per-book settings
- Second board: LilyGo T5 2.13" through GxEPD2 (`board_lilygo.h`)

**Hardware**
- Enclosure with relocated MENU/EXIT buttons (levers or separate caps), openings for USB-C / BOOT / RESET, battery bay
