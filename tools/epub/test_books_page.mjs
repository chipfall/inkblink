// Smoke test of the Books page script as shipped in firmware/inkblink/webui_assets.h,
// run against a mocked browser. Usage: node test_books_page.mjs SAMPLES_DIR
import fs from 'fs'; import vm from 'vm'; import path from 'path'; import { fileURLToPath } from 'url';
const here = path.dirname(fileURLToPath(import.meta.url));
const dir = process.argv[2] || 'samples';
const assets = fs.readFileSync(path.join(here, '../../firmware/inkblink/webui_assets.h'), 'utf8');
const tag = 'const char WEB_JS_BOOKS[] PROGMEM = R"JS(';
const i = assets.indexOf(tag) + tag.length, j = assets.indexOf(')JS";', i);
const html = assets.slice(i, j).replace(/^<script>\n/, '').replace(/\n<\/script>\n$/, '');
let ok = 0, bad = 0; const CHECK = (c, m) => { if (c) ok++; else { bad++; console.log('  FAIL:', m); } };
function mkEl() { const el = { className: '', classList: { add() {}, remove() {} }, children: [], listeners: {}, style: {}, title: '', textContent: '', dataset: {},
  appendChild(c) { el.children.push(c); return c; }, addEventListener(t, f) { (el.listeners[t] = el.listeners[t] || []).push(f); },
  querySelector(sel) { el._q = el._q || {}; return el._q[sel] = el._q[sel] || { style: {}, textContent: '', title: '' }; }, set innerHTML(v) { el._q = {}; } }; return el; }
function run() {
  const posts = []; const els = { drop: mkEl(), file: mkEl(), queue: mkEl() }; let reloaded = 0;
  class XHR { constructor() { this.upload = {}; } open(m, u) { this.u = u; } send(fd) { const f = fd.get('book'); posts.push({ url: this.u, name: f.name, size: f.size });
    this.status = 200; this.responseText = 'OK'; setImmediate(() => { this.upload.onprogress && this.upload.onprogress({ lengthComputable: true, loaded: f.size / 2, total: f.size }); this.onload(); }); } }
  const ctx = { document: { getElementById: id => els[id], createElement: () => mkEl(), querySelectorAll: () => [] }, XMLHttpRequest: XHR, FormData, Blob, File,
    setTimeout: () => { reloaded++; }, location: { reload() {} }, window: null, TextDecoder, TextEncoder, DecompressionStream, Response, Uint8Array, DataView, console, Array, Promise,
    fetch: async () => ({}), confirm: () => true, decodeURIComponent, Object, String, Math, RegExp, Error, JSON, setImmediate };
  ctx.window = ctx; vm.createContext(ctx); vm.runInContext(html, ctx);
  return { ctx, els, posts, reloaded: () => reloaded };
}
const mkFile = p => { const b = fs.readFileSync(p); return new File([b], path.basename(p)); };
let t = run(); let rows = t.els.queue.children;
await t.ctx._booksRun([mkFile(path.join(dir, 'epub2.epub'))]);
CHECK(t.posts.length === 2 && t.posts[0].name === 'Andrzej Sapkowski - Ostatnie życzenie.txt' && t.posts[1].name === 'Andrzej Sapkowski - Ostatnie życzenie.toc', 'EPUB uploads .txt then .toc');
CHECK(t.posts.every(p => p.url === '/upload'), 'upload url');
CHECK(rows[0].className === 'q ok' && /4 chapters/.test(rows[0]._q['.n'].textContent), 'row done with chapter count');
CHECK(t.reloaded() === 1, 'page reload scheduled');
t = run(); await t.ctx._booksRun([mkFile(path.join(dir, 'nocont.epub'))]);
CHECK(t.posts.length === 0 && t.els.queue.children[0].className === 'q err' && /EPUB:/.test(t.els.queue.children[0]._q['.s'].textContent), 'broken EPUB reported, nothing uploaded');
fs.writeFileSync(path.join(dir, 'Pan Tadeusz.txt'), 'Litwo, ojczyzno moja'); fs.writeFileSync(path.join(dir, 'x.pdf'), 'pdf');
t = run(); await t.ctx._booksRun([mkFile(path.join(dir, 'Pan Tadeusz.txt')), mkFile(path.join(dir, 'x.pdf')), mkFile(path.join(dir, 'single.epub')), mkFile(path.join(dir, 'epub3.epub'))]);
rows = t.els.queue.children;
CHECK(t.posts.map(p => p.name).join('|') === 'Pan Tadeusz.txt|Jeden plik.txt|Jeden plik.toc|Jan Autor - Book Three.txt|Jan Autor - Book Three.toc', 'queue order');
CHECK(rows[1].className === 'q err' && /only/.test(rows[1]._q['.s'].textContent), 'pdf rejected');
CHECK(rows[0].className === 'q ok' && rows[2].className === 'q ok' && rows[3].className === 'q ok', 'other files done');
console.log(`==== BOOKS PAGE SCRIPT: ${ok} OK, ${bad} FAILED ====`);
process.exit(bad ? 1 : 0);
