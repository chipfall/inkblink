// Tests for the EPUB converter (tools/epub/epub.js). Usage: node test_epub.mjs SAMPLES_DIR
import fs from 'fs'; import vm from 'vm'; import path from 'path'; import { fileURLToPath } from 'url';
const here = path.dirname(fileURLToPath(import.meta.url));
const dir = process.argv[2] || 'samples';
globalThis.window = globalThis; vm.runInThisContext(fs.readFileSync(path.join(here, 'epub.js'), 'utf8'));
const mkFile = f => { const p = path.join(dir, f); return { name: f, arrayBuffer: async () => { const b = fs.readFileSync(p); return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength); } }; };
let ok = 0, bad = 0; const CHECK = (c, m) => { if (c) ok++; else { bad++; console.log('  FAIL:', m); } };
const at = (s, off, n) => Buffer.from(s, 'utf8').subarray(off).toString('utf8').slice(0, n);

console.log('== EPUB2 (NCX, stored + deflated parts, percent-encoded names, fragment entry)');
let b = await epubToBook(mkFile('epub2.epub')); const T = b._out;
CHECK(b.name === 'Andrzej Sapkowski - Ostatnie życzenie', 'name ' + b.name);
CHECK(T.startsWith('Ostatnie życzenie\nAndrzej Sapkowski\n\nRozdział I\n'), 'header + first title');
CHECK(T.includes('Wiedźmin szedł długo przez las — Zażółć gęślą jaźń…'), 'entities / nbsp / dash / ellipsis');
CHECK(T.includes('To jest kursywa w środku.'), 'inline tags');
CHECK(T.includes('wyraz i spacje\ndruga linia po br'), 'soft hyphen / nbsp collapse / br');
CHECK(T.includes('& <tag> "cytat" „polski”'), 'basic entities');
CHECK(!T.includes('komentarz') && !T.includes('NIE') && !T.includes('margin'), 'comments, scripts and styles skipped');
CHECK(!/\u00ad|\u00a0/.test(T), 'no soft hyphens or nbsp left');
CHECK(!/\n\n\n/.test(T), 'no triple newlines');
CHECK(b.chapters === 4, 'chapter count ' + b.chapters);
for (const c of b._toc) CHECK(at(T, c.off, c.title.length) === c.title, `toc offset points at "${c.title}"`);
CHECK(b._toc.map(c => c.title).join('|') === 'Rozdział I|Rozdział II|Część druga|Zakończenie', 'toc titles');
CHECK(b.toc && b.toc.size > 20, 'toc blob');
CHECK(!T.includes('cover.jpg'), 'cover page skipped');

console.log('== EPUB3 (nav document, no NCX, files in archive root)');
b = await epubToBook(mkFile('epub3.epub'));
CHECK(b.name === 'Jan Autor - Book Three', 'name3');
CHECK(b._toc.length === 2 && b._toc[0].title === 'One & first' && b._toc[1].title === 'Two', 'nav titles (landmarks ignored)');
for (const c of b._toc) CHECK(at(b._out, c.off, 3) === c.title.slice(0, 3), 'nav offset ' + c.title);

console.log('== one file, table of contents with anchors');
b = await epubToBook(mkFile('single.epub'));
CHECK(b._toc.length === 3, '3 anchors');
for (const c of b._toc) CHECK(at(b._out, c.off, c.title.length) === c.title, 'anchor offset ' + c.title);

console.log('== no table of contents: titles from headings');
b = await epubToBook(mkFile('notoc.epub'));
CHECK(b._toc.length === 2 && b._toc[0].title === 'Alfa' && b._toc[1].title === 'Beta', 'auto titles');

console.log('== broken files');
for (const f of ['notzip.epub', 'nocont.epub']) {
  try { await epubToBook(mkFile(f)); CHECK(false, f + ' should fail'); } catch (e) { console.log('  ' + f + ' ->', e.message); CHECK(true, ''); }
}
console.log(`==== EPUB CONVERTER: ${ok} OK, ${bad} FAILED ====`);
process.exit(bad ? 1 : 0);
