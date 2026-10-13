#!/usr/bin/env python3
"""Keep the JavaScript embedded in firmware/inkblink/webui_assets.h in sync with its sources.

The Books page script is tools/epub/epub.js (EPUB converter) + tools/epub/books_page.js
(drag & drop and upload queue). Edit those files, then run:

    python3 tools/sync_webui_epub.py          # rewrite the embedded copy
    python3 tools/sync_webui_epub.py --check  # exit 1 if the embedded copy is out of date
"""
import os, sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
epub = open(os.path.join(root, "tools/epub/epub.js"), encoding="utf-8").read().rstrip()
page = open(os.path.join(root, "tools/epub/books_page.js"), encoding="utf-8").read()
block = "<script>\n" + epub + "\n" + page + "\n</script>\n"
assert ')JS"' not in block
path = os.path.join(root, "firmware/inkblink/webui_assets.h")
text = open(path, encoding="utf-8").read()
start_tag = 'const char WEB_JS_BOOKS[] PROGMEM = R"JS('
i = text.index(start_tag) + len(start_tag)
j = text.index(')JS";', i)
if "--check" in sys.argv:
    if text[i:j] != block:
        print("webui_assets.h is out of date - run: python3 tools/sync_webui_epub.py")
        sys.exit(1)
    print("webui_assets.h is up to date")
else:
    open(path, "w", encoding="utf-8").write(text[:i] + block + text[j:])
    print("webui_assets.h updated")
