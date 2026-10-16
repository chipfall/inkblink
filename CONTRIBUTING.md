# Contributing

Thanks for your interest in InkBlink!

- **Bugs and ideas**: open an issue. For display problems, a photo of the screen and the exact board revision help a lot.
- **Before a pull request**: run `make -C tests test`. Add or extend a test when you change logic (layout, settings, navigation, EPUB conversion, web pages).
- **Web page script**: edit `tools/epub/epub.js` / `tools/epub/books_page.js`, then run `python3 tools/sync_webui_epub.py` (the embedded copy in `firmware/inkblink/webui_assets.h` must match; the test suite checks this).
- **Language**: on-device texts and the web interface are English. Most code comments are still Polish - translating them is a very welcome contribution.
- **Third-party code**: do not add code or fonts without a license that allows redistribution; list them in `THIRD_PARTY_NOTICES.md`. The manufacturer's driver files in `firmware/inkblink/` are included with Elecrow's permission and are not MIT-licensed; keep their status described in `THIRD_PARTY_NOTICES.md`.
- **Hardware testing**: this firmware has been run on one CrowPanel V1.2 only. Reports from other revisions are valuable.
- **Versions**: see "Versioning and releases" in `docs/DEVELOPMENT.md`. Do not bump `FW_VERSION` in ordinary pull requests; describe your change under the newest `CHANGELOG.md` heading.
