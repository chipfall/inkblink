#!/usr/bin/env python3
"""Checks that FW_VERSION is well formed and matches the newest CHANGELOG.md heading."""
import os, re, sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ino = open(os.path.join(root, "firmware/inkblink/inkblink.ino"), encoding="utf-8").read()
log = open(os.path.join(root, "CHANGELOG.md"), encoding="utf-8").read()

m = re.search(r'#define FW_VERSION "([^"]*)"', ino)
if not m:
    sys.exit("FW_VERSION not found in inkblink.ino")
fw = m.group(1)
if not re.fullmatch(r"\d+\.\d+\.\d+(-dev)?", fw):
    sys.exit("FW_VERSION '%s' must look like 1.2.3 or 1.2.3-dev" % fw)

h = re.search(r"^## (\S+)", log, re.M)
if not h:
    sys.exit("no version heading in CHANGELOG.md")
if h.group(1) != fw:
    sys.exit("FW_VERSION is %s but the newest CHANGELOG.md heading is %s" % (fw, h.group(1)))
print("version OK:", fw)
