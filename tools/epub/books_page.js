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