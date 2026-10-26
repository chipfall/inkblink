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
// table of contents: NCX or nav (EPUB3)
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
