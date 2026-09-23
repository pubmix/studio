#pragma once
#include <pgmspace.h>

// The page the Dub-Box serves at http://<its address>/ : pick audio files, the browser converts
// audio to compatible WAV, optionally splits through the existing companion, and uploads it.
// Ordinary upload and lossless transfer compression work offline with browser APIs.
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
input[type=file]{display:none}
input,select,button{font:inherit;padding:8px;background:#202837;color:#fff;border:1px solid #566074;border-radius:5px;max-width:100%;box-sizing:border-box}
label.control{display:block;margin:12px 0}details{padding:12px;background:#1d2330;border-radius:8px}button{cursor:pointer;margin:5px}button:disabled{opacity:.5}a{color:#ffb86b} .s{white-space:pre-line}
.item{margin:10px 0;padding:10px 14px;background:#1d2330;border-radius:8px}
.item .n{font-weight:600;word-break:break-all}
.item .s{font-size:14px;color:#9aa3b5;margin-top:2px}
.bar{height:6px;margin-top:8px;background:#2b3345;border-radius:3px;overflow:hidden}
.bar i{display:block;height:100%;width:0;background:#3d7fff}
.ok .bar i{background:#66bb6a}.err .bar i{background:#ff5252}.err .s{color:#ff8a80}
</style></head><body><main>
<h1>DUB<span>-</span>BOX</h1>
<p>Add audio to your Dub-Box over WiFi. Compatible WAVs are preserved; other audio is converted in your browser. Files land on the SD card, ready to put on a track. Choose transfer and stem options, then add files.</p>
<div id="st">Checking the Dub-Box&hellip;</div>

<label class="control">Transfer mode <select id="transfer"><option value="wav">Original / uncompressed WAV</option><option value="deflate-blocks-v1">Compressed transfer · lossless · smaller Wi-Fi upload</option></select></label>
<p id="transferinfo">Compatible 44.1 kHz PCM16 WAVs are preserved byte-for-byte. Other audio is converted to 44.1 kHz PCM16 stereo. Compression restores exactly the same WAV on the device; serial delivery and SD size are unchanged. Total speedup is not guaranteed.</p>
<label class="control">Stem splitting <select id="split"><option value="none">Off · upload audio as usual</option><option value="stage">Stage source for touchscreen</option><option value="four">Simple · four stems</option><option value="extended">Extended · deeper model sources</option><option value="dynamic">Dynamic · fold quiet sources</option></select></label>
<details id="setup"><summary>Connect the existing Studio stem engine</summary>
<p>Run Studio’s companion on your computer with --device matching this page’s address. Keep it awake. Splitting runs there, not on the Dub-Box.</p>
<label class="control">Companion address <input id="companion" type="url" value="http://127.0.0.1:8766" autocomplete="off"></label>
<label class="control">Pairing key <input id="key" type="password" autocomplete="off"></label><button id="connect" type="button">Connect engine</button>
<button id="pairdevice" type="button">Enable touchscreen controls on this Dub-Box</button><p>For touchscreen controls, use the computer’s LAN address above (not 127.0.0.1). Pair once, then use MENU → STEM SPLITTER on the device.</p>
<p id="engine" role="status">Not connected. Ordinary uploads work without the companion.</p>
<label class="control">Model <select id="model"><option value="">Automatic</option></select></label>
<label class="control"><input id="rights" type="checkbox"> I have permission to separate this audio.</label>
<p>Deeper sources depend on the model. This device plays four tracks: vocals, melody, bass, rhythm. Deeper WAVs remain available to download. YouTube search remains in the companion.</p>
<a id="youtube" target="_blank" rel="noopener noreferrer" hidden>Open companion for YouTube</a>
</details>
<label id="drop"><b>Choose audio files</b>or drop them here (mp3, wav, m4a, flac, ogg)<input id="pick" type="file" accept="audio/*,.wav,.mp3,.m4a,.flac,.ogg,.aac" multiple></label>
<div id="list" aria-live="polite"></div>
</main>
<script>
const $=s=>document.querySelector(s),st=$('#st'),list=$('#list'),drop=$('#drop');
const NAMES={busy:'Stop playback on the Dub-Box first',size:'File too large or empty',sd:'SD card error (full?)',seq:'Transfer error - try again',crc:'Transfer error - try again',timeout:'The Dub-Box stopped answering','not connected':'The Dub-Box is not connected'};
let deviceCaps=[];
async function status(){
  try{const j=await(await fetch('/status',{cache:'no-store'})).json();
    deviceCaps=j.transfer_modes||['wav'];
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
// Pass through only a structurally valid playback-compatible PCM WAV.
function compatibleWav(buf){
  if(buf.byteLength<44)return false;
  const v=new DataView(buf),tag=o=>String.fromCharCode(...new Uint8Array(buf,o,4));
  if(tag(0)!=='RIFF'||tag(8)!=='WAVE'||v.getUint32(4,true)+8!==buf.byteLength)return false;
  let fmt=false,data=false,align=0,dataSize=0;
  for(let o=12;o+8<=buf.byteLength;){
    const size=v.getUint32(o+4,true),end=o+8+size;
    if(end>buf.byteLength)return false;
    if(tag(o)==='fmt '){
      if(fmt||size<16)return false;
      const channels=v.getUint16(o+10,true);align=channels*2;
      fmt=v.getUint16(o+8,true)===1&&(channels===1||channels===2)&&v.getUint32(o+12,true)===44100&&v.getUint16(o+22,true)===16&&v.getUint16(o+20,true)===align&&v.getUint32(o+16,true)===44100*align;
      if(!fmt)return false;
    }
    if(tag(o)==='data'){if(data)return false;data=true;dataSize=size;}
    o=end+(size%2);
    if(o===buf.byteLength)return fmt&&data&&dataSize>0&&dataSize%align===0;
  }
  return false;
}
const sizeText=n=>(n/1048576).toFixed(2)+' MiB';
async function prepareTransfer(wav,mode,progress){
  const original=new Blob([wav],{type:'audio/wav'});
  if(mode==='wav')return {blob:original,mode:'wav',note:'Uncompressed WAV'};
  if(!deviceCaps.includes(mode))throw Error('Device does not advertise compressed transfer. Choose Original WAV or update the display firmware.');
  if(typeof CompressionStream==='undefined')throw Error('This browser cannot compress files. Choose Original WAV or use a browser with Compression Streams.');
  const parts=[];
  for(let o=0;o<wav.byteLength;o+=8192){
    const raw=wav.slice(o,o+8192);
    const encoded=await new Response(new Blob([raw]).stream().pipeThrough(new CompressionStream('deflate'))).arrayBuffer();
    if(encoded.byteLength>8256)throw Error('Compressed block is too large. Choose Original WAV.');
    const header=new ArrayBuffer(4),v=new DataView(header);v.setUint16(0,encoded.byteLength,true);v.setUint16(2,raw.byteLength,true);
    parts.push(header,encoded);progress(Math.min(1,(o+raw.byteLength)/wav.byteLength));
  }
  const blob=new Blob(parts,{type:'application/octet-stream'});
  return blob.size<original.size?{blob,mode,note:'Lossless compressed transfer'}:{blob:original,mode:'wav',note:'Compression did not shrink this file; sending uncompressed WAV'};
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
function send(name,blob,onp,size=blob.size,mode='wav'){
  return new Promise((res,rej)=>{
    const x=new XMLHttpRequest();
    x.open('POST','/upload?name='+encodeURIComponent(name)+'&size='+size+'&encoding='+mode+'&wire_size='+blob.size);
    x.upload.onprogress=e=>e.lengthComputable&&onp(e.loaded/e.total);
    x.onload=()=>{let j={};try{j=JSON.parse(x.responseText)}catch(e){}
      (x.status===200&&j.ok)?res(j):rej(new Error(NAMES[j.error]||j.error||('HTTP '+x.status)))};
    x.onerror=()=>rej(new Error('Connection lost. Inspect the device SD before retrying.'));
    x.timeout=30*60*1000;x.ontimeout=()=>rej(new Error('Transfer timed out. Inspect the SD before retrying.'));
    x.onabort=()=>rej(new Error('Transfer cancelled'));
    const f=new FormData();f.append('file',blob,'a.wav');x.send(f);
  });
}
let engine=null;
async function api(connection,path,options={}){
  let r;
  try{r=await fetch(connection.url+path,{...options,headers:{Authorization:'Bearer '+connection.key,...options.headers},signal:options.signal||AbortSignal.timeout(30000)})}
  catch(e){throw Error('Cannot reach companion. Check its address, pairing, browser local-network permission and --device '+location.origin+'. '+e.message)}
  if(!r.ok){let j={};try{j=await r.json()}catch{}throw Error(typeof j.detail==='string'?j.detail:'Companion HTTP '+r.status)}return r;
}
$('#connect').onclick=async()=>{
  const button=$('#connect');button.disabled=true;engine=null;
  try{
    const url=new URL($('#companion').value);
    if(!['http:','https:'].includes(url.protocol)||url.username||url.password||url.pathname!=='/'||url.search||url.hash)throw Error('Enter only the companion origin, such as http://192.168.1.2:8766');
    const connection={url:url.origin,key:$('#key').value.trim()};
    const data=await(await api(connection,'/v1/models')).json();
    $('#model').replaceChildren(new Option('Automatic',''));
    for(const name of Object.keys(data.models))$('#model').add(new Option(name,name));
    engine=connection;$('#engine').textContent='Engine connected. Extended mode defaults to the six-source model; first use may download weights.';
    $('#youtube').href=connection.url;$('#youtube').hidden=false;
  }catch(e){$('#engine').textContent=e.message}finally{button.disabled=false}
};
$('#pairdevice').onclick=async()=>{
  const button=$('#pairdevice');button.disabled=true;
  try{
    if(!engine)throw Error('Connect the engine first using the computer LAN address.');
    const r=await fetch('/stem/configure',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({origin:engine.url,key:engine.key})});
    const result=await r.json();if(!r.ok||!result.ok)throw Error(result.error||'Could not pair device');
    $('#engine').textContent='Touchscreen paired. On Dub-Box, open MENU → STEM SPLITTER.';
  }catch(e){$('#engine').textContent=e.message}finally{button.disabled=false}
};
$('#split').onchange=()=>{if($('#split').value!=='none')$('#setup').open=true};
let queue=Promise.resolve();
function add(file){
  // Snapshot controls and credentials so changing them cannot redirect queued audio.
  const opts={mode:$('#split').value,transfer:$('#transfer').value,model:$('#model').value,connection:engine,rights:$('#rights').checked};
  const el=document.createElement('div');el.className='item';
  el.innerHTML='<div class="n"></div><div class="s">Waiting…</div><div class="bar"><i></i></div><div class="assets"></div>';
  el.firstChild.textContent=file.name;list.prepend(el);
  const s=el.querySelector('.s'),bar=el.querySelector('i'),assets=el.querySelector('.assets');
  const saved=[];
  queue=queue.then(async()=>{
    try{
      if(!file.size||file.size>0x7fff0000)throw Error('File is empty or exceeds the device upload limit.');
      if(opts.mode!=='none'&&file.size>100*1024*1024)throw Error('Choose a source up to 100 MiB for splitting.');
      if(opts.mode!=='none'&&(!opts.connection||!opts.rights))throw Error('Connect the engine and confirm permission before choosing a file to split.');
      const base=file.name.replace(/\.[^.]*$/,'').replace(/[^A-Za-z0-9_-]+/g,'-').slice(0,24)||'audio';
      async function deliver(name,wav){
        await status();
        const result=await prepareTransfer(wav,opts.transfer,p=>{s.textContent='Compressing losslessly… '+Math.round(p*100)+'%';bar.style.width=p*100+'%'});
        const reduction=Math.round((1-result.blob.size/wav.byteLength)*100);
        const detail=result.note+' · WAV '+sizeText(wav.byteLength)+' → transfer '+sizeText(result.blob.size)+' ('+reduction+'% smaller)';
        s.textContent=detail+' · uploading…';
        const started=performance.now();
        const reply=await send(name,result.blob,p=>{bar.style.width=p*100+'%';s.textContent=detail+' · '+Math.round(p*100)+'% sent'+(p===1?' · waiting for SD confirmation…':'')},wav.byteLength,result.mode);
        const seconds=(performance.now()-started)/1000;
        saved.push(reply.name);
        const summary=document.createElement('p');summary.textContent=reply.name+' · '+detail+' · '+seconds.toFixed(1)+' s to SD confirmation';assets.append(summary);
      }
      if(opts.mode==='none'){
        s.textContent='Reading '+sizeText(file.size)+'…';
        const original=await file.arrayBuffer();
        const wav=compatibleWav(original)?original:toWav(await decode(file));
        await deliver(base,wav);
      }else{
        s.textContent='Sending source to stem engine…';
        let source=file,title=file.name.slice(0,120);
        if(!/\.(mp3|wav)$/i.test(file.name)){source=new Blob([toWav(await decode(file))]);title=base+'.wav'}
        if(source.size>100*1024*1024)throw Error('Converted source exceeds the engine 100 MiB limit.');
        if(opts.mode==='stage'){
          await api(opts.connection,'/v1/panel/sources?'+new URLSearchParams({title}),{method:'POST',body:source,signal:AbortSignal.timeout(10*60*1000)});
          bar.style.width='100%';el.classList.add('ok');s.textContent='Staged. On Dub-Box open MENU → STEM SPLITTER → REFRESH LIBRARY, then choose this track.';return;
        }
        const query=new URLSearchParams({title,mode:opts.mode,stem_depth:'16'});if(opts.model)query.set('model',opts.model);
        const job=await(await api(opts.connection,'/v1/uploads?'+query,{method:'POST',body:source,signal:AbortSignal.timeout(10*60*1000)})).json();
        const cancel=document.createElement('button');cancel.textContent='Cancel split';assets.append(cancel);
        cancel.onclick=async()=>{cancel.disabled=true;try{await api(opts.connection,'/v1/jobs/'+job.id+'/cancel',{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'})}catch(e){s.textContent=e.message;cancel.disabled=false}};
        let ready;
        try{
          const deadline=Date.now()+6*60*60*1000;
          for(;;){
            if(Date.now()>deadline)throw Error('Split is taking longer than six hours. Check its status in the companion.');
            ready=await(await api(opts.connection,'/v1/jobs/'+job.id)).json();
            s.textContent=ready.message+(ready.percent==null?'':' · '+ready.percent+'%');bar.style.width=(ready.percent||0)+'%';
            if(ready.state==='ready')break;
            if(['failed','cancelled'].includes(ready.state))throw Error(ready.message||ready.state);
            await new Promise(r=>setTimeout(r,1500));
          }
        }finally{cancel.remove()}
        const info=document.createElement('p');info.textContent=(ready.extended_stems?.length||4)+' available sources. Sending four-track mixdown in vocals, melody, bass, rhythm order.'+(ready.fallback?' Selected backend failed; Demucs fallback was used.':'');assets.append(info);
        for(const stem of ready.extended_stems||[]){
          const button=document.createElement('button');button.textContent='Download '+stem.role;assets.append(button);
          button.onclick=async()=>{button.disabled=true;try{const blob=await(await api(opts.connection,'/v1/jobs/'+job.id+'/extended/'+stem.role.toLowerCase())).blob();const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=stem.file;a.click();setTimeout(()=>URL.revokeObjectURL(url),60000)}catch(e){s.textContent=e.message}finally{button.disabled=false}};
        }
        for(const stem of ready.stems){
          s.textContent='Retrieving '+stem.role+'…';
          const wav=await(await api(opts.connection,'/v1/jobs/'+job.id+'/stems/'+stem.role.toLowerCase())).arrayBuffer();
          if(!compatibleWav(wav))throw Error('Engine returned an incompatible WAV; transfer stopped.');
          await deliver(base+'-'+stem.role,wav);
        }
      }
      bar.style.width='100%';el.classList.add('ok');s.textContent='Saved '+saved.join(', ')+'. Pick these files from the Dub-Box track menu.';
    }catch(e){el.classList.add('err');s.textContent=((e&&e.message)||'Failed')+(saved.length?' Confirmed on SD: '+saved.join(', ')+'.':'')+' Inspect the SD before retrying a failed transfer.'}
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
