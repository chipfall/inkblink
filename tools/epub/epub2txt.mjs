#!/usr/bin/env node
// Command-line EPUB -> TXT (+ chapter list) converter: the same code the web page runs in your browser.
// Usage: node epub2txt.mjs book.epub OUT_DIR [file-name-without-extension]
// Upload the resulting .txt and .toc files on the InkBlink web page (Books tab).
import fs from 'fs';
import vm from 'vm';
import path from 'path';
import { fileURLToPath } from 'url';

const here = path.dirname(fileURLToPath(import.meta.url));
globalThis.window = globalThis;
vm.runInThisContext(fs.readFileSync(path.join(here, 'epub.js'), 'utf8'));

const [,, input, outDir = '.', nameArg] = process.argv;
if (!input) { console.error('usage: node epub2txt.mjs book.epub OUT_DIR [name]'); process.exit(2); }
const buf = fs.readFileSync(input);
const book = await epubToBook({ name: path.basename(input), arrayBuffer: async () => buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength) });
fs.mkdirSync(outDir, { recursive: true });
const name = nameArg || book.name;
fs.writeFileSync(path.join(outDir, name + '.txt'), Buffer.from(await book.text.arrayBuffer()));
if (book.toc) fs.writeFileSync(path.join(outDir, name + '.toc'), Buffer.from(await book.toc.arrayBuffer()));
console.log(`${name}: ${book.chapters} chapters, ${book.bytes} bytes of text`);
