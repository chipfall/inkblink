#!/usr/bin/env python3
"""Generate small synthetic EPUB files used by the EPUB converter tests.

Usage: python3 make_samples.py OUT_DIR
Creates: epub2.epub (NCX + fragment), epub3.epub (nav), single.epub (one file, anchors),
notoc.epub (no table of contents), big.epub (40 chapters), notzip.epub, nocont.epub (broken).
"""
import os, sys, zipfile

OUT = sys.argv[1] if len(sys.argv) > 1 else "samples"
os.makedirs(OUT, exist_ok=True)

def mk(name, files, stored=("mimetype",)):
    with zipfile.ZipFile(os.path.join(OUT, name), "w") as z:
        for n, d in files:
            if isinstance(d, str):
                d = d.encode("utf-8")
            z.writestr(n, d, compress_type=zipfile.ZIP_STORED if n in stored or n.endswith(".jpg") else zipfile.ZIP_DEFLATED)

CONT = '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'
CONT_ROOT = CONT.replace("OEBPS/content.opf", "content.opf")

def xh(body, title="x"):
    return ('<?xml version="1.0" encoding="utf-8"?><!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">'
            '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>%s</title><style>p{margin:0}</style><script>var x="<p>NIE</p>";</script></head><body>%s</body></html>' % (title, body))

# ---- EPUB2: NCX, percent-encoded file name, fragment entry, stored + deflated parts ----
ch1 = xh('<h1>Rozdzia&#x142; I</h1><p>Wiedźmin szedł długo&nbsp;przez las &mdash; Zażółć gęślą jaźń&hellip;</p><p>To jest <i>kursywa</i> w <b>środku</b>.</p><!-- komentarz --><p>wy&shy;raz i&nbsp;&nbsp;spacje<br/>druga linia po br</p><p>&amp; &lt;tag&gt; &quot;cytat&quot; &#8222;polski&#8221;</p>', "Ch1")
ch2 = xh('<h1>Rozdzia\u0142 II</h1><p>Pierwsza czesc drugiego rozdzialu. ' + 'Lorem ipsum dolor sit amet. ' * 20 + '</p><h2 id="czesc2">Cz\u0119\u015b\u0107 druga</h2><p>Tekst drugiej czesci zaczyna sie tutaj.</p><div><p>W dywie.</p></div>', "Ch2")
ch3 = xh('<h1>Zako\u0144czenie</h1><p>Koniec ksi\u0105\u017cki.</p>', "Ch3")
cover = xh('<div><img src="cover.jpg" alt=""/></div>', "Cover")
opf2 = '''<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="2.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Ostatnie &#380;yczenie</dc:title><dc:creator opf:role="aut" xmlns:opf="http://www.idpf.org/2007/opf">Andrzej Sapkowski</dc:creator></metadata><manifest><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/><item id="c0" href="Text/cover.xhtml" media-type="application/xhtml+xml"/><item id="c1" href="Text/ch1.xhtml" media-type="application/xhtml+xml"/><item id="c2" href="Text/Rozdzia%C5%82_2.xhtml" media-type="application/xhtml+xml"/><item id="c3" href="Text/ch3.xhtml" media-type="application/xhtml+xml"/><item id="img" href="Images/cover.jpg" media-type="image/jpeg"/></manifest><spine toc="ncx"><itemref idref="c0"/><itemref idref="c1"/><itemref idref="c2"/><itemref idref="c3"/></spine></package>'''
ncx = '''<?xml version="1.0"?><ncx xmlns="http://www.daisy.org/z3986/2005/ncx/"><navMap><navPoint id="n1"><navLabel><text>Rozdzia&#322; I</text></navLabel><content src="Text/ch1.xhtml"/></navPoint><navPoint id="n2"><navLabel><text>Rozdzia\u0142 II</text></navLabel><content src="Text/Rozdzia%C5%82_2.xhtml"/></navPoint><navPoint id="n3"><navLabel><text>Cz\u0119\u015b\u0107 druga</text></navLabel><content src="Text/Rozdzia%C5%82_2.xhtml#czesc2"/></navPoint><navPoint id="n4"><navLabel><text>Zako\u0144czenie</text></navLabel><content src="Text/ch3.xhtml"/></navPoint></navMap></ncx>'''
mk("epub2.epub", [("mimetype", "application/epub+zip"), ("META-INF/container.xml", CONT), ("OEBPS/content.opf", opf2), ("OEBPS/toc.ncx", ncx),
                  ("OEBPS/Text/cover.xhtml", cover), ("OEBPS/Text/ch1.xhtml", ch1), ("OEBPS/Text/Rozdzia\u0142_2.xhtml", ch2), ("OEBPS/Text/ch3.xhtml", ch3),
                  ("OEBPS/Images/cover.jpg", b"\xff\xd8\xff" + bytes(range(256)) * 12)])

# ---- EPUB3: nav document instead of NCX, files in the archive root ----
opf3 = '''<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="3.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title id="t">Book Three</dc:title><dc:creator id="c">Jan Autor</dc:creator></metadata><manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/><item id="a" href="a.xhtml" media-type="application/xhtml+xml"/><item id="b" href="b.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="a"/><itemref idref="b"/></spine></package>'''
nav = '''<html xmlns:epub="http://www.idpf.org/2007/ops"><body><nav epub:type="toc"><ol><li><a href="a.xhtml">One &amp; first</a></li><li><a href="b.xhtml">Two</a></li></ol></nav><nav epub:type="landmarks"><ol><li><a href="a.xhtml">Start</a></li></ol></nav></body></html>'''
mk("epub3.epub", [("mimetype", "application/epub+zip"), ("META-INF/container.xml", CONT_ROOT), ("content.opf", opf3), ("nav.xhtml", nav),
                  ("a.xhtml", xh("<h1>One</h1><p>" + "Alpha beta gamma. " * 40 + "</p>")), ("b.xhtml", xh("<h1>Two</h1><p>" + "Delta epsilon. " * 40 + "</p>"))])

# ---- one big file, table of contents entries point at anchors ----
big_one = xh('<h1 id="p1">Pierwszy</h1><p>' + "Tekst jeden. " * 50 + '</p><h1 id="p2">Drugi</h1><p>' + "Tekst dwa. " * 50 + '</p><h2 id="p3">Trzeci</h2><p>Koniec.</p>')
opfs = '''<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="2.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Jeden plik</dc:title></metadata><manifest><item id="n" href="toc.ncx" media-type="application/x-dtbncx+xml"/><item id="b" href="all.html" media-type="application/xhtml+xml"/></manifest><spine toc="n"><itemref idref="b"/></spine></package>'''
ncxs = '''<ncx><navMap><navPoint><navLabel><text>Pierwszy</text></navLabel><content src="all.html#p1"/></navPoint><navPoint><navLabel><text>Drugi</text></navLabel><content src="all.html#p2"/></navPoint><navPoint><navLabel><text>Trzeci</text></navLabel><content src="all.html#p3"/></navPoint></navMap></ncx>'''
mk("single.epub", [("mimetype", "application/epub+zip"), ("META-INF/container.xml", CONT_ROOT), ("content.opf", opfs), ("toc.ncx", ncxs), ("all.html", big_one)])

# ---- no table of contents: chapter titles come from headings ----
mk("notoc.epub", [("mimetype", "application/epub+zip"), ("META-INF/container.xml", CONT_ROOT),
                  ("content.opf", opf3.replace('<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>', '')),
                  ("a.xhtml", xh("<h1>Alfa</h1><p>" + "Alpha beta gamma. " * 40 + "</p>")), ("b.xhtml", xh("<h2>Beta</h2><p>" + "Delta epsilon. " * 40 + "</p>"))])

# ---- broken files ----
open(os.path.join(OUT, "notzip.epub"), "wb").write(b"to nie jest zip" * 100)
mk("nocont.epub", [("a.txt", "x")])

# ---- 40 chapters (used by the firmware chapter-list tests) ----
words = "Wiedźmin szedł długo przez ciemny las, gdzie powietrze falowało od zapachów. Zażółć gęślą jaźń, pchnąć w tę łódź jeża lub ośm skrzyń fig. Czarodzieje i czarodziejki poznosili do sali fotele, krzesła i zydle. ".split(". ")
N = 40
items, spine, navp = [], [], []
files = [("mimetype", "application/epub+zip"), ("META-INF/container.xml", CONT)]
for i in range(1, N + 1):
    paras = "".join("<p>" + (". ".join(words) * 3) + " " + str(i * 10 + j) + ".</p>" for j in range(8))
    files.append(("OEBPS/c%02d.xhtml" % i, xh("<h1>Rozdział %d: Wędrówka numer %d</h1>%s" % (i, i, paras))))
    items.append('<item id="c%d" href="c%02d.xhtml" media-type="application/xhtml+xml"/>' % (i, i))
    spine.append('<itemref idref="c%d"/>' % i)
    navp.append('<navPoint id="n%d"><navLabel><text>Rozdział %d: Wędrówka numer %d</text></navLabel><content src="c%02d.xhtml"/></navPoint>' % (i, i, i, i))
opf = ('<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="2.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Długa podróż</dc:title><dc:creator>Test Autor</dc:creator></metadata><manifest><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>'
       + "".join(items) + '</manifest><spine toc="ncx">' + "".join(spine) + '</spine></package>')
files.append(("OEBPS/content.opf", opf))
files.append(("OEBPS/toc.ncx", '<ncx><navMap>' + "".join(navp) + '</navMap></ncx>'))
mk("big.epub", files)
print("samples written to", OUT)
