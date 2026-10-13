#pragma once
// ============================================================
//  Strona WWW czytnika: wspolne czesci (naglowek, style, skrypty).
//  Wszystko w jednym pliku HTML bez zewnetrznych zasobow (dziala bez internetu).
//  Teksty strony sa po angielsku.
// ============================================================
#include <Arduino.h>

// ---------- Naglowek + style + menu ----------
const char WEB_HEAD[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#2563eb">
<title>InkBlink</title>
<style>
:root{--bg:#f3f4f7;--card:#fff;--ink:#1c2230;--mut:#667085;--acc:#2563eb;--acc2:#1e4fc4;--line:#e4e7ec;--ok:#12a150;--err:#d92d20;--soft:#eef3ff}
@media(prefers-color-scheme:dark){:root{--bg:#0e1116;--card:#171b22;--ink:#e9ecf2;--mut:#98a2b3;--line:#272d38;--soft:#1b2540}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
.top{position:sticky;top:0;z-index:5;display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:10px;padding:12px 16px;background:var(--card);border-bottom:1px solid var(--line)}
.brand{display:flex;align-items:center;gap:10px;font-weight:700;font-size:18px}
.brand svg{width:26px;height:26px;color:var(--acc)}
nav{display:flex;gap:6px}
nav a{padding:7px 13px;border-radius:999px;color:var(--mut);text-decoration:none;font-weight:600;font-size:14px}
nav a.on{background:var(--soft);color:var(--acc)}
main{max-width:640px;margin:0 auto;padding:18px 14px 60px}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:18px;margin-bottom:16px;box-shadow:0 1px 2px rgba(16,24,40,.04)}
.card h2{margin:0 0 4px;font-size:18px}
.card h3{margin:20px 0 6px;font-size:13px;letter-spacing:.06em;text-transform:uppercase;color:var(--mut)}
.sub{margin:0 0 14px;color:var(--mut);font-size:14px}
.chips{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:16px}
.chip{display:flex;align-items:center;gap:6px;padding:6px 12px;border-radius:999px;background:var(--card);border:1px solid var(--line);font-size:13px;color:var(--mut)}
.chip b{color:var(--ink)}
.drop{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;padding:28px 14px;border:2px dashed var(--line);border-radius:14px;text-align:center;color:var(--mut);transition:.15s}
.drop.hot{border-color:var(--acc);background:var(--soft);color:var(--acc)}
.drop svg{width:34px;height:34px;color:var(--acc)}
.drop label,.btn{display:inline-block;padding:9px 18px;border-radius:10px;border:0;background:var(--acc);color:#fff;font:600 15px system-ui,sans-serif;cursor:pointer}
.drop label:hover,.btn:hover{background:var(--acc2)}
.btn.sec{background:var(--soft);color:var(--acc)}
.btn:disabled{opacity:.45;cursor:default}
.drop input{display:none}
.q{display:grid;grid-template-columns:1fr 70px;gap:4px 10px;margin-top:12px;font-size:14px}
.q .n{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.q .s{text-align:right;color:var(--mut)}
.q .bar{grid-column:1/3;height:6px;border-radius:99px;background:var(--line);overflow:hidden}
.q .bar i{display:block;height:100%;width:0;background:var(--acc);transition:width .2s}
.q.ok .s{color:var(--ok)}.q.ok .bar i{background:var(--ok)}
.q.err .s{color:var(--err)}.q.err .bar i{background:var(--err)}
.books{list-style:none;margin:0;padding:0}
.books li{display:flex;align-items:center;gap:12px;padding:12px 0;border-top:1px solid var(--line)}
.books li:first-child{border-top:0}
.books .ic{flex:none;width:34px;height:42px;border-radius:5px;background:var(--soft);color:var(--acc);display:flex;align-items:center;justify-content:center;font-size:11px;font-weight:700}
.books .t{flex:1;min-width:0}
.books .t div:first-child{font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.books .t div:last-child{font-size:13px;color:var(--mut)}
button.del{border:1px solid var(--line);background:transparent;color:var(--err);border-radius:8px;padding:6px 12px;font:600 13px system-ui,sans-serif;cursor:pointer}
button.del:hover{background:rgba(217,45,32,.08)}
.empty{padding:14px 0 4px;color:var(--mut);text-align:center}
label.f{display:block;font-size:14px;font-weight:600;margin:12px 0 4px}
select{width:100%;padding:10px 12px;border-radius:10px;border:1px solid var(--line);background:var(--card);color:var(--ink);font:15px system-ui,sans-serif}
label.c{display:flex;align-items:center;gap:10px;padding:8px 0;font-size:15px}
label.c input{width:18px;height:18px;accent-color:var(--acc)}
.note{margin:6px 0 0;font-size:13px;color:var(--mut)}
.toast{padding:10px 14px;border-radius:10px;background:rgba(18,161,80,.12);color:var(--ok);font-weight:600;margin-bottom:14px}
canvas{width:100%;max-width:500px;image-rendering:pixelated;border:1px solid var(--line);border-radius:8px;background:#fff}
.row{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px}
input[type=range]{width:100%;accent-color:var(--acc)}
input[type=file].plain{width:100%;font-size:14px}
#msg{margin-top:12px;font-size:14px;font-weight:600}
</style></head><body>
<header class="top">
<div class="brand"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 5.5A2.5 2.5 0 0 1 6.5 3H20v16H6.5A2.5 2.5 0 0 0 4 21.5z"/><path d="M4 21.5V5.5"/><path d="M9 8h7M9 12h5"/></svg><span>InkBlink</span></div>
<nav><a href="/">Books</a><a href="/wallpaper">Sleep image</a><a href="/config">Settings</a></nav>
</header>
<main>
)HTML";

// ---------- Zakonczenie ----------
const char WEB_TAIL[] PROGMEM = R"HTML(</main>
<script>document.querySelectorAll('nav a').forEach(function(a){if(a.getAttribute('href')===location.pathname)a.classList.add('on');});
try{fetch('/time',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'t='+Math.floor(Date.now()/1000)}).then(function(r){return r.text();}).then(function(t){var e=document.getElementById('devtime');if(e)e.textContent=t;}).catch(function(){});}catch(e){}</script>
</body></html>
)HTML";

// ---------- Strona "Books": wysylanie plikow i usuwanie ----------
const char WEB_JS_BOOKS[] PROGMEM = R"JS(<script>
(function(){
'use strict';
var TD=new TextDecoder('utf-8'),TE=new TextEncoder();
function u8len(s){var n=0;for(var i=0;i<s.length;i++){var c=s.charCodeAt(i);if(c<128)n+=1;else if(c<2048)n+=2;else if(c>=55296&&c<=56319){n+=4;i++;}else n+=3;}return n;}
async function inflate(u8){var ds=new DecompressionStream('deflate-raw'),w=ds.writable.getWriter();w.write(u8).catch(function(){});w.close().catch(function(){});return new Uint8Array(await new Response(ds.readable).arrayBuffer());}
function readZip(b){var dv=new DataView(b.buffer,b.byteOffset,b.byteLength),e=b.length-22,lim=Math.max(0,b.length-70000);
while(e>=lim&&dv.getUint32(e,true)!==0x06054b50)e--;
if(e<lim)throw new Error('not a ZIP/EPUB file');
var n=dv.getUint16(e+10,true),p=dv.getUint32(e+16,true),files={};
for(var i=0;i<n;i++){if(dv.getUint32(p,true)!==0x02014b50)break;
var nl=dv.getUint16(p+28,true),el=dv.getUint16(p+30,true),cl=dv.getUint16(p+32,true);
files[TD.decode(b.subarray(p+46,p+46+nl))]={m:dv.getUint16(p+10,true),cs:dv.getUint32(p+20,true),off:dv.getUint32(p+42,true)};
p+=46+nl+el+cl;}
return files;}
async function zipGet(b,files,name){var f=files[name];if(!f)return null;var dv=new DataView(b.buffer,b.byteOffset,b.byteLength),o=f.off;
if(dv.getUint32(o,true)!==0x04034b50)throw new Error('damaged EPUB');
var s=o+30+dv.getUint16(o+26,true)+dv.getUint16(o+28,true),d=b.subarray(s,s+f.cs);
if(f.m===0)return d;if(f.m===8)return inflate(d);throw new Error('unsupported ZIP compression');}
var ENT={amp:'&',lt:'<',gt:'>',quot:'"',apos:"'",nbsp:' ',mdash:'\u2014',ndash:'\u2013',hellip:'\u2026',laquo:'\u00ab',raquo:'\u00bb',lsquo:'\u2018',rsquo:'\u2019',ldquo:'\u201c',rdquo:'\u201d',bdquo:'\u201e',sbquo:'\u201a',copy:'\u00a9',reg:'\u00ae',deg:'\u00b0',times:'\u00d7',middot:'\u00b7',bull:'\u2022',shy:'',zwj:'',zwnj:''};
function dec(s){return s.replace(/&(#x[0-9a-f]+|#[0-9]+|[a-z]+);/gi,function(m,x){var c;if(x[0]==='#'){c=x[1]==='x'||x[1]==='X'?parseInt(x.slice(2),16):parseInt(x.slice(1),10);return c>0&&c<1114112?String.fromCodePoint(c):'';}return ENT.hasOwnProperty(x)?ENT[x]:ENT.hasOwnProperty(x.toLowerCase())?ENT[x.toLowerCase()]:'';});}
function clean(s){return s.replace(/[\u00ad\u200b\u200c\u200d\ufeff]/g,'').replace(/[\u00a0\u2007\u202f]/g,' ');}
function attr(tag,n){var m=new RegExp('[\\s]'+n+'\\s*=\\s*("([^"]*)"|\'([^\']*)\')','i').exec(tag);return m?(m[2]!==undefined?m[2]:m[3]):null;}
function strip(s){return clean(dec(s.replace(/<[^>]*>/g,' '))).replace(/\s+/g,' ').trim();}
function norm(p){var o=[];p.split('/').forEach(function(s){if(s==='..')o.pop();else if(s!=='.'&&s!=='')o.push(s);});return o.join('/');}
function resolve(base,href){var h=href.split('#')[0].split('?')[0];try{h=decodeURIComponent(h);}catch(e){}var d=base.indexOf('/')>=0?base.slice(0,base.lastIndexOf('/')+1):'';return norm(h.charAt(0)==='/'?h.slice(1):d+h);}
var BLOCK=/^(p|div|li|tr|blockquote|section|article|pre|table|ul|ol|dd|dt|dl|figure|figcaption|header|footer|nav|aside|center|address|hr|body|html|title|h[1-6])$/;
var SKIP=/^(head|script|style|svg|math|noscript)$/;
function html2text(html){var out='',pend=0,pa=[],anchors={},m,re=/<!--[\s\S]*?-->|<[^>]*>|[^<]+/g,skip=null;
function trimEnd(){out=out.replace(/[ ]+$/,'');}
function flush(){if(pend>0&&out.length){trimEnd();var nl=out.match(/\n*$/)[0].length,want=pend>=2?2:1;while(nl<want){out+='\n';nl++;}}pend=0;while(pa.length)anchors[pa.pop()]=out.length;}
while((m=re.exec(html))){var t=m[0];
if(t.charAt(0)!=='<'){if(skip)continue;t=clean(dec(t)).replace(/\s+/g,' ');if(t===' '||t===''){if(t===' '&&out.length&&!/[\s]$/.test(out)&&pend===0)out+=' ';continue;}
if(pend>0||pa.length)flush();if(!out.length||/[\n ]$/.test(out))t=t.replace(/^ /,'');out+=t;continue;}
if(t.indexOf('<!--')===0||t.charAt(1)==='?'||t.charAt(1)==='!')continue;
var cl=t.charAt(1)==='/',nm=/^<\/?\s*([a-zA-Z][a-zA-Z0-9:]*)/.exec(t);if(!nm)continue;var n=nm[1].toLowerCase(),self=/\/>$/.test(t);
if(skip){if(cl&&n===skip)skip=null;continue;}
if(!cl&&SKIP.test(n)&&!self){skip=n;continue;}
if(!cl){var id=attr(t,'id')||attr(t,'name');if(id)pa.push(id);}
if(n==='br'){trimEnd();if(pend>0||pa.length)flush();out+='\n';continue;}
if(BLOCK.test(n)){var lvl=/^h[12]$/.test(n)?2:1;if(pend<lvl)pend=lvl;}}
flush();out=out.replace(/[ ]+\n/g,'\n').replace(/\n{3,}/g,'\n\n');return {text:out,anchors:anchors};}
function tagList(xml,name){return xml.match(new RegExp('<'+name+'\\b[^>]*>','gi'))||[];}
async function epubToBook(file){
var buf=new Uint8Array(await file.arrayBuffer()),zf=readZip(buf);
async function getText(n){var d=await zipGet(buf,zf,n);return d?TD.decode(d):null;}
var cont=await getText('META-INF/container.xml');if(!cont)throw new Error('no container.xml - not an EPUB');
var opfPath=attr(tagList(cont,'rootfile')[0]||'','full-path');if(!opfPath)throw new Error('no OPF file');
var opf=await getText(opfPath);if(!opf)throw new Error('missing '+opfPath);
function meta(n){var r=new RegExp('<(?:dc:)?'+n+'\\b[^>]*>([\\s\\S]*?)</(?:dc:)?'+n+'>','i').exec(opf);return r?strip(r[1]):'';}
var title=meta('title'),author=meta('creator'),man={},navId=null,ncxId=null;
tagList(opf,'item').forEach(function(t){var id=attr(t,'id');if(!id)return;var pr=attr(t,'properties')||'';man[id]={href:attr(t,'href')||'',type:attr(t,'media-type')||'',nav:/\bnav\b/.test(pr)};if(man[id].nav)navId=id;if(/dtbncx/.test(man[id].type))ncxId=id;});
var sp=tagList(opf,'spine')[0]||'',sref=attr(sp,'toc');if(sref&&man[sref])ncxId=sref;
var spine=[];tagList(opf,'itemref').forEach(function(t){var id=attr(t,'idref'),lin=attr(t,'linear');if(id&&man[id]&&lin!=='no'&&/html|xml/.test(man[id].type))spine.push(resolve(opfPath,man[id].href));});
if(!spine.length)throw new Error('empty book (no text chapters)');
// spis tresci: NCX albo nav (EPUB3)
var toc=[];
if(navId){var nv=await getText(resolve(opfPath,man[navId].href));if(nv){var nb=/<nav\b[^>]*epub:type\s*=\s*["']toc["'][^>]*>([\s\S]*?)<\/nav>/i.exec(nv);var seg=nb?nb[1]:nv,base=resolve(opfPath,man[navId].href),ar=/<a\b([^>]*)>([\s\S]*?)<\/a>/gi,mm;while((mm=ar.exec(seg))){var hr=attr('<a '+mm[1]+'>','href');if(hr)toc.push({file:resolve(base,hr),frag:hr.indexOf('#')>=0?hr.split('#')[1]:'',title:strip(mm[2])});}}}
if(!toc.length&&ncxId){var nx=await getText(resolve(opfPath,man[ncxId].href));if(nx){var nbase=resolve(opfPath,man[ncxId].href),nr=/<navLabel>\s*<text>([\s\S]*?)<\/text>\s*<\/navLabel>\s*<content\b([^>]*)>/gi,nm2;while((nm2=nr.exec(nx))){var src=attr('<c '+nm2[2]+'>','src');if(src)toc.push({file:resolve(nbase,src),frag:src.indexOf('#')>=0?src.split('#')[1]:'',title:strip(nm2[1])});}}}
var head=(title||'').trim(),out='',chapters=[],docs={};
if(head)out+=head+'\n';if(author)out+=author+'\n';if(out)out+='\n';
for(var i=0;i<spine.length;i++){var raw=await getText(spine[i]);if(raw==null)continue;var cv=html2text(raw),tx=cv.text.replace(/^\n+/,'');
var lead=cv.text.length-tx.length;if(tx.replace(/\s/g,'').length<2)continue;
var start=u8len(out);docs[spine[i]]={start:start,lead:lead,anchors:cv.anchors,text:tx,len:tx.length};
var hm=/<h[1-3]\b[^>]*>([\s\S]*?)<\/h[1-3]>/i.exec(raw),ti=/<title\b[^>]*>([\s\S]*?)<\/title>/i.exec(raw);
docs[spine[i]].auto=strip(hm?hm[1]:ti?ti[1]:'')||('Part '+(i+1));
out+=tx;if(!/\n$/.test(tx))out+='\n';out+='\n';}
function off(d,frag){var ci=0;if(frag&&d.anchors.hasOwnProperty(frag))ci=Math.max(0,d.anchors[frag]-d.lead);return d.start+u8len(d.text.slice(0,ci));}
toc.forEach(function(e){var d=docs[e.file];if(d&&e.title)chapters.push({off:off(d,e.frag),title:e.title});});
if(!chapters.length)Object.keys(docs).forEach(function(k){var d=docs[k];if(d.len>=400)chapters.push({off:d.start,title:d.auto});});
chapters.sort(function(a,b){return a.off-b.off;});
var seen={},fin=[];chapters.forEach(function(c){if(seen[c.off])return;seen[c.off]=1;fin.push(c);});
out=out.replace(/\n+$/,'\n');
var tocTxt=fin.map(function(c){return c.off+'\t'+c.title.replace(/[\t\r\n]+/g,' ').slice(0,60);}).join('\n')+(fin.length?'\n':'');
var nm=((author?author+' - ':'')+(title||file.name.replace(/\.epub$/i,''))).replace(/[\\\/:*?"<>|\u0000-\u001f]/g,' ').replace(/\s+/g,' ').trim().slice(0,80)||'book';
return {name:nm,text:new Blob([TE.encode(out)],{type:'text/plain'}),toc:fin.length>1?new Blob([TE.encode(tocTxt)],{type:'text/plain'}):null,chapters:fin.length,bytes:u8len(out),title:title,author:author,_out:out,_toc:fin};}
window.epubToBook=epubToBook;window._epubHtml2Text=html2text;
})();
(function(){
var drop=document.getElementById('drop'),input=document.getElementById('file'),queue=document.getElementById('queue');
function addRow(name){var r=document.createElement('div');r.className='q';r.innerHTML='<span class="n"></span><span class="s">waiting</span><div class="bar"><i></i></div>';r.querySelector('.n').textContent=name;queue.appendChild(r);return r;}
function fail(row,msg){row.className='q err';row.querySelector('.s').textContent=msg;}
function post(blob,fname,row,from,to){return new Promise(function(done){
var fd=new FormData();fd.append('book',blob,fname);
var x=new XMLHttpRequest();x.open('POST','/upload');
x.upload.onprogress=function(e){if(e.lengthComputable){var p=from+Math.round(e.loaded*(to-from)/e.total);row.querySelector('i').style.width=p+'%';row.querySelector('.s').textContent=p+'%';}};
x.onload=function(){done(x.status>=200&&x.status<300?{ok:true}:{ok:false,msg:x.responseText||'failed'});};
x.onerror=function(){done({ok:false,msg:'lost'});};
x.send(fd);});}
async function send(file,row){
var n=file.name;
if(/\.epub$/i.test(n)){
if(typeof DecompressionStream==='undefined'){fail(row,'browser too old for EPUB');return false;}
row.querySelector('.s').textContent='converting...';
var b;try{b=await window.epubToBook(file);}catch(e){fail(row,'EPUB: '+(e&&e.message?e.message:e));return false;}
row.querySelector('.n').textContent=b.name+' ('+b.chapters+' chapters)';
var r=await post(b.text,b.name+'.txt',row,0,b.toc?92:100);
if(!r.ok){fail(row,r.msg);return false;}
if(b.toc){var r2=await post(b.toc,b.name+'.toc',row,92,100);if(!r2.ok)row.querySelector('.n').title='Chapter list not saved: '+r2.msg;}
}else if(/\.txt$/i.test(n)){
var r3=await post(file,n,row,0,100);
if(!r3.ok){fail(row,'failed');row.querySelector('.n').title=r3.msg;return false;}
}else{fail(row,'.txt or .epub only');return false;}
row.className='q ok';row.querySelector('i').style.width='100%';row.querySelector('.s').textContent='done';return true;}
async function run(files){var any=false;for(var i=0;i<files.length;i++){var r=addRow(files[i].name);if(await send(files[i],r))any=true;}if(any)setTimeout(function(){location.reload();},900);}
input.addEventListener('change',function(){run(Array.from(input.files));input.value='';});
['dragenter','dragover'].forEach(function(t){drop.addEventListener(t,function(e){e.preventDefault();drop.classList.add('hot');});});
['dragleave','drop'].forEach(function(t){drop.addEventListener(t,function(e){e.preventDefault();drop.classList.remove('hot');});});
drop.addEventListener('drop',function(e){run(Array.from(e.dataTransfer.files));});
document.querySelectorAll('button.del').forEach(function(b){b.addEventListener('click',function(){if(!confirm('Delete this book?'))return;fetch('/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'f='+encodeURIComponent(b.dataset.f)}).then(function(){location.reload();});});});
window._booksRun=run;
})();
</script>
)JS";

// ---------- Strona "Sleep image": konwersja obrazu do 1 bitu i wyslanie ----------
const char WEB_JS_WALLPAPER[] PROGMEM = R"JS(<script>
(function(){
var W=250,H=122,BYTES=3904;
var file=document.getElementById('img'),cv=document.getElementById('pv'),ctx=cv.getContext('2d');
var fit=document.getElementById('fit'),dith=document.getElementById('dith'),bri=document.getElementById('bri'),inv=document.getElementById('inv');
var send=document.getElementById('send'),reset=document.getElementById('reset'),msg=document.getElementById('msg');
var img=null,bits=null;
function render(){
if(!img){ctx.fillStyle='#fff';ctx.fillRect(0,0,W,H);bits=null;send.disabled=true;return;}
var c=document.createElement('canvas');c.width=W;c.height=H;var g=c.getContext('2d');g.fillStyle='#fff';g.fillRect(0,0,W,H);
var s=fit.value==='cover'?Math.max(W/img.width,H/img.height):Math.min(W/img.width,H/img.height);
var dw=img.width*s,dh=img.height*s;g.drawImage(img,(W-dw)/2,(H-dh)/2,dw,dh);
var d=g.getImageData(0,0,W,H).data,lum=new Float32Array(W*H),b=parseInt(bri.value,10);
for(var i=0;i<W*H;i++){var v=0.299*d[i*4]+0.587*d[i*4+1]+0.114*d[i*4+2]+b;lum[i]=v<0?0:(v>255?255:v);}
bits=new Uint8Array(BYTES);var fs=dith.value==='fs';
for(var y=0;y<H;y++){for(var x=0;x<W;x++){var k=y*W+x,old=lum[k],nw=old<128?0:255;
if(fs){var e=old-nw;if(x+1<W)lum[k+1]+=e*7/16;if(y+1<H){if(x>0)lum[k+W-1]+=e*3/16;lum[k+W]+=e*5/16;if(x+1<W)lum[k+W+1]+=e/16;}}
var black=nw===0;if(inv.checked)black=!black;if(black)bits[y*32+(x>>3)]|=(0x80>>(x&7));}}
var out=ctx.createImageData(W,H);
for(var y2=0;y2<H;y2++)for(var x2=0;x2<W;x2++){var bl=(bits[y2*32+(x2>>3)]&(0x80>>(x2&7)))!==0,o=(y2*W+x2)*4,val=bl?0:255;out.data[o]=out.data[o+1]=out.data[o+2]=val;out.data[o+3]=255;}
ctx.putImageData(out,0,0);send.disabled=false;}
file.addEventListener('change',function(){var f=file.files[0];if(!f)return;var im=new Image();im.onload=function(){img=im;msg.textContent='';render();};im.src=URL.createObjectURL(f);});
[fit,dith,bri,inv].forEach(function(el){el.addEventListener('input',render);});
send.addEventListener('click',function(){if(!bits)return;send.disabled=true;msg.style.color='';msg.textContent='Sending...';
var fd=new FormData();fd.append('wp',new Blob([bits],{type:'application/octet-stream'}),'wallpaper.bin');
fetch('/wp',{method:'POST',body:fd}).then(function(r){return r.text().then(function(t){msg.style.color=r.ok?'var(--ok)':'var(--err)';msg.textContent=r.ok?'Saved. It will appear when the reader goes to sleep.':'Error: '+t;send.disabled=false;});}).catch(function(){msg.style.color='var(--err)';msg.textContent='Connection lost.';send.disabled=false;});});
reset.addEventListener('click',function(){fetch('/wp_reset',{method:'POST'}).then(function(){msg.style.color='var(--ok)';msg.textContent='Back to the built-in image.';});});
render();
})();
</script>
)JS";

// ---------- Strona "Books": karta wysylania plikow ----------
const char WEB_BOOKS_UPLOAD[] PROGMEM = R"HTML(<div class="card"><h2>Add books</h2>
<p class="sub">Plain text (.txt, UTF-8) or EPUB. EPUB files are converted to text in your browser (with a chapter list), so your phone or computer does the work.</p>
<div class="drop" id="drop"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 16V4M7 9l5-5 5 5"/><path d="M4 16v3a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-3"/></svg>
<div>Drag &amp; drop .txt or .epub files here</div><div>or</div>
<label>Choose files<input id="file" type="file" accept=".txt,.epub,text/plain,application/epub+zip" multiple></label></div>
<div id="queue"></div></div>
)HTML";

// ---------- Strona "Sleep image": formularz ----------
const char WEB_WP_BODY[] PROGMEM = R"HTML(<div class="card"><h2>Sleep image</h2>
<p class="sub">Shown when the reader goes to sleep (Settings &rarr; Sleep screen: Image). Any picture works; it is converted to black &amp; white at 250&times;122 pixels.</p>
<input id="img" class="plain" type="file" accept="image/*">
<label class="f">Picture size</label><select id="fit"><option value="cover">Fill the screen (crop)</option><option value="contain">Fit inside (white borders)</option></select>
<label class="f">Black &amp; white style</label><select id="dith"><option value="fs">Dithered (photos)</option><option value="th">Sharp (drawings, text)</option></select>
<label class="f">Brightness</label><input id="bri" type="range" min="-80" max="80" value="0">
<label class="c"><input id="inv" type="checkbox"> Invert colours</label>
<h3>Preview</h3><canvas id="pv" width="250" height="122"></canvas>
<div class="row"><button id="send" class="btn" disabled>Send to reader</button><button id="reset" class="btn sec" type="button">Use built-in image</button></div>
<div id="msg"></div></div>
)HTML";
