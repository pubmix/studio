#pragma once
#include <pgmspace.h>

// The page the Dub-Box serves at http://<its address>/ : pick audio files, the browser converts
// each one to a 44.1 kHz 16-bit stereo WAV and uploads it. Everything is inline (no internet needed).
static const char kUploadPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dub-Box upload</title>
<style>
:root{color-scheme:dark}
body{margin:0;font:16px/1.4 system-ui,sans-serif;background:#12151c;color:#e8ebf2}
main{max-width:640px;margin:0 auto;padding:24px 16px 48px}
h1{margin:0 0 4px;font-size:28px;letter-spacing:2px}
h1 span{color:#ff9f3d}
p{margin:6px 0;color:#9aa3b5}
#st{margin:14px 0;padding:10px 14px;border-radius:8px;background:#1d2330}
#drop{display:block;margin:18px 0;padding:36px 16px;text-align:center;border:2px dashed #3d4a66;border-radius:12px;cursor:pointer;color:#c5cce0}
#drop.over{border-color:#ff9f3d;background:#1f2432}
#drop b{display:block;font-size:20px;color:#fff;margin-bottom:4px}
input{display:none}
.item{margin:10px 0;padding:10px 14px;background:#1d2330;border-radius:8px}
.item .n{font-weight:600;word-break:break-all}
.item .s{font-size:14px;color:#9aa3b5;margin-top:2px}
.bar{height:6px;margin-top:8px;background:#2b3345;border-radius:3px;overflow:hidden}
.bar i{display:block;height:100%;width:0;background:#3d7fff}
.ok .bar i{background:#66bb6a}.err .bar i{background:#ff5252}.err .s{color:#ff8a80}
</style></head><body><main>
<h1>DUB<span>-</span>BOX</h1>
<p>Add audio to your Dub-Box over WiFi. Files are converted to WAV in your browser and land on the SD card, ready to put on a track.</p>
<div id="st">Checking the Dub-Box&hellip;</div>
<label id="drop"><b>Choose audio files</b>or drop them here (mp3, wav, m4a, flac, ogg)<input id="pick" type="file" accept="audio/*,.wav,.mp3,.m4a,.flac,.ogg,.aac" multiple></label>
<div id="list"></div>
</main>
<script>
const $=s=>document.querySelector(s),st=$('#st'),list=$('#list'),drop=$('#drop');
const NAMES={busy:'Stop playback on the Dub-Box first',size:'File too large or empty',sd:'SD card error (full?)',seq:'Transfer error - try again',crc:'Transfer error - try again',timeout:'The Dub-Box stopped answering','not connected':'The Dub-Box is not connected'};
async function status(){
  try{const j=await(await fetch('/status',{cache:'no-store'})).json();
    st.textContent=!j.linked?'The Dub-Box is not answering - is it powered on?':j.busy?'The Dub-Box is busy right now.':j.playing?'Playing - stop playback on the Dub-Box before uploading.':'Connected - ready.';
  }catch(e){st.textContent='Cannot reach the Dub-Box.';}
}
const clamp=x=>{x=x<-1?-1:x>1?1:x;return(x<0?x*32768:x*32767)|0};
function toWav(a){
  const n=a.length,L=a.getChannelData(0),R=a.numberOfChannels>1?a.getChannelData(1):L;
  const buf=new ArrayBuffer(44+n*4),v=new DataView(buf);
  const w=(o,s)=>{for(let i=0;i<s.length;i++)v.setUint8(o+i,s.charCodeAt(i))};
  w(0,'RIFF');v.setUint32(4,36+n*4,true);w(8,'WAVE');w(12,'fmt ');v.setUint32(16,16,true);
  v.setUint16(20,1,true);v.setUint16(22,2,true);v.setUint32(24,44100,true);v.setUint32(28,176400,true);
  v.setUint16(32,4,true);v.setUint16(34,16,true);w(36,'data');v.setUint32(40,n*4,true);
  let o=44;for(let i=0;i<n;i++){v.setInt16(o,clamp(L[i]),true);v.setInt16(o+2,clamp(R[i]),true);o+=4}
  return buf;
}
async function decode(file){
  const AC=window.AudioContext||window.webkitAudioContext;
  const ac=new AC({sampleRate:44100});
  try{
    let a=await ac.decodeAudioData(await file.arrayBuffer());
    if(a.sampleRate!==44100){
      const off=new OfflineAudioContext(2,Math.ceil(a.duration*44100),44100),s=off.createBufferSource();
      s.buffer=a;s.connect(off.destination);s.start();a=await off.startRendering();
    }
    return a;
  }finally{ac.close()}
}
function send(name,blob,onp){
  return new Promise((res,rej)=>{
    const x=new XMLHttpRequest();
    x.open('POST','/upload?name='+encodeURIComponent(name)+'&size='+blob.size);
    x.upload.onprogress=e=>e.lengthComputable&&onp(e.loaded/e.total);
    x.onload=()=>{let j={};try{j=JSON.parse(x.responseText)}catch(e){}
      (x.status===200&&j.ok)?res(j):rej(new Error(NAMES[j.error]||j.error||('HTTP '+x.status)))};
    x.onerror=()=>rej(new Error('Connection lost'));
    const f=new FormData();f.append('file',blob,'a.wav');x.send(f);
  });
}
let queue=Promise.resolve();
function add(file){
  const el=document.createElement('div');el.className='item';
  el.innerHTML='<div class="n"></div><div class="s">Waiting&hellip;</div><div class="bar"><i></i></div>';
  el.firstChild.textContent=file.name;list.prepend(el);
  const s=el.querySelector('.s'),bar=el.querySelector('i');
  queue=queue.then(async()=>{
    try{
      s.textContent='Converting to WAV…';
      const wav=toWav(await decode(file));
      const base=file.name.replace(/\.[^.]*$/,'').replace(/[^A-Za-z0-9_-]+/g,'-').slice(0,32)||'audio';
      const mb=(wav.byteLength/1048576).toFixed(1);
      s.textContent='Uploading '+mb+' MB…';
      const j=await send(base,new Blob([wav]),p=>{bar.style.width=(p*100).toFixed(0)+'%';s.textContent='Uploading '+mb+' MB - '+(p*100).toFixed(0)+'%'});
      bar.style.width='100%';el.classList.add('ok');s.textContent='Saved as '+j.name+' - pick it from the Dub-Box track menu.';
    }catch(e){el.classList.add('err');s.textContent=(e&&e.message)||'Failed'}
    status();
  });
}
function take(files){for(const f of files)add(f)}
$('#pick').onchange=e=>{take(e.target.files);e.target.value=''};
['dragover','dragenter'].forEach(t=>drop.addEventListener(t,e=>{e.preventDefault();drop.classList.add('over')}));
['dragleave','drop'].forEach(t=>drop.addEventListener(t,e=>{e.preventDefault();drop.classList.remove('over')}));
drop.addEventListener('drop',e=>take(e.dataTransfer.files));
status();setInterval(()=>{if(!document.querySelector('.item:not(.ok):not(.err)'))status()},4000);
</script></body></html>)HTML";
