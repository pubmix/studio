// Executes the actual embedded page with simulated DOM/HTTP and real browser compression APIs.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),zlib=require('node:zlib'),path=require('node:path');
const script=fs.readFileSync(path.join(__dirname,'../src/upload_page.h'),'utf8').split('<script>')[1].split('</script>')[0];
class Element{
 constructor(){this.children=[];this.style={};this.value='';this.checked=false;this.textContent='';this.parts={};this.classes=new Set();this.classList={add:x=>this.classes.add(x),remove:x=>this.classes.delete(x)};}
 set innerHTML(x){this.firstChild=new Element();for(const key of ['.s','i','.assets'])this.parts[key]=new Element()}
 querySelector(x){return this.parts[x]||null} prepend(x){this.children.unshift(x)} append(x){this.children.push(x)} replaceChildren(...x){this.children=x} add(x){this.children.push(x)} addEventListener(){} remove(){} click(){}
}
const nodes=new Map();const get=x=>{if(!nodes.has(x))nodes.set(x,new Element());return nodes.get(x)};
get('#split').value='none';get('#transfer').value='wav';get('#model').value='';
let sent=[],requests=[],failAt=0;const wavRole='';
const context={console,Blob,File,FormData,Response,CompressionStream,AbortSignal,URL,URLSearchParams,performance,DataView,ArrayBuffer,Uint8Array,Math,Error,Promise,Number,String,Option:Element,location:{origin:'http://dubbox.local'},setTimeout,clearTimeout,setInterval:()=>0,
 document:{querySelector:get,createElement:()=>new Element()},
 fetch:async(url,opts={})=>{
  requests.push({url,opts});
  if(url==='/status')return Response.json({linked:true,busy:false,playing:false,transfer_modes:['wav','deflate-blocks-v1']});
  assert.equal(opts.headers.Authorization,'Bearer test-key');
  if(url.endsWith('/v1/models'))return Response.json({models:{'demucs-four':{},'demucs-six':{}}});
  if(url.includes('/v1/panel/sources?'))return Response.json({id:'source123'});
  if(url.includes('/v1/uploads?'))return Response.json({id:'abc'});
  if(url.endsWith('/v1/jobs/abc'))return Response.json({state:'ready',message:'Ready',stems:['VOCALS','MELODY','BASS','RHYTHM'].map(role=>({role})),extended_stems:[{role:'GUITAR',file:'guitar.wav'}]});
  if(url.includes('/stems/'))return new Response(fixture);
  throw Error('Unexpected URL '+url);
 },
 XMLHttpRequest:class{
  constructor(){this.upload={};this.status=200;}
  open(method,url){this.url=url;}
  async send(form){const blob=form.get('file'),params=new URL(this.url,'http://device').searchParams,bytes=Buffer.from(await blob.arrayBuffer());sent.push({params,bytes});this.upload.onprogress({lengthComputable:true,loaded:bytes.length,total:bytes.length});this.responseText=JSON.stringify(sent.length===failAt?{ok:false,error:'sd'}:{ok:true,name:params.get('name')+'.wav'});this.onload();}
 }
};
vm.createContext(context);vm.runInContext(script,context);
const run=code=>vm.runInContext(code,context);
let fixture;
(async()=>{
 await run('status()');
 fixture=Buffer.from(run('toWav({length:22050,numberOfChannels:2,getChannelData:()=>new Float32Array(22050).fill(.125)})'));
 context.fixture=fixture.buffer.slice(fixture.byteOffset,fixture.byteOffset+fixture.byteLength);
 assert.equal(run('compatibleWav(fixture)'),true);
 const invalid=Buffer.from(fixture);invalid.writeUInt32LE(48000,24);context.invalid=invalid.buffer.slice(invalid.byteOffset,invalid.byteOffset+invalid.length);assert.equal(run('compatibleWav(invalid)'),false);
 const result=await run("prepareTransfer(fixture,'deflate-blocks-v1',()=>{})");
 const encoded=Buffer.from(await result.blob.arrayBuffer());let decoded=[];
 for(let o=0;o<encoded.length;){const n=encoded.readUInt16LE(o),size=encoded.readUInt16LE(o+2),part=zlib.inflateSync(encoded.subarray(o+4,o+4+n));assert.equal(part.length,size);decoded.push(part);o+=4+n}
 assert.deepEqual(Buffer.concat(decoded),fixture);assert.ok(encoded.length<fixture.length);
 if(process.argv[2]){fs.writeFileSync(process.argv[2]+'.blocks',encoded);fs.writeFileSync(process.argv[2]+'.wav',fixture)}
 await run("add(new File([fixture],'original.wav'));queue");assert.deepEqual(sent[0].bytes,fixture);assert.equal(sent[0].params.get('encoding'),'wav');
 get('#transfer').value='deflate-blocks-v1';await run("add(new File([fixture],'compressed.wav'));queue");assert.equal(sent[1].params.get('encoding'),'deflate-blocks-v1');assert.deepEqual(sent[1].bytes,encoded);
 get('#split').value='extended';await run("add(new File([fixture],'unpaired.wav'));queue");assert.equal(sent.length,2);assert.match(get('#list').children[0].querySelector('.s').textContent,/Connect the engine/);
 get('#companion').value='http://127.0.0.1:8766';get('#key').value='test-key';await get('#connect').onclick();get('#rights').checked=true;
 sent=[];await run("add(new File([fixture],'split.wav'));queue");assert.equal(sent.length,4);assert.deepEqual(sent.map(s=>s.params.get('name')),['split-VOCALS','split-MELODY','split-BASS','split-RHYTHM']);assert.ok(requests.some(r=>r.url.includes('mode=extended')));
 get('#split').value='stage';sent=[];await run("add(new File([fixture],'staged.wav'));queue");assert.equal(sent.length,0);assert.match(get('#list').children[0].querySelector('.s').textContent,/Staged/);get('#split').value='extended';
 sent=[];failAt=2;await run("add(new File([fixture],'partial.wav'));queue");assert.equal(sent.length,2);assert.match(get('#list').children[0].querySelector('.s').textContent,/Confirmed on SD: partial-VOCALS.wav/);
 context.noise=Uint8Array.from(require('node:crypto').randomBytes(200)).buffer;
 const expanded=await run("prepareTransfer(noise,'deflate-blocks-v1',()=>{})");assert.equal(expanded.mode,'wav');assert.match(expanded.note,/did not shrink/);
 await run("deviceCaps=['wav']");await assert.rejects(()=>run("prepareTransfer(fixture,'deflate-blocks-v1',()=>{})"),/does not advertise/);
 await run("deviceCaps=['wav','deflate-blocks-v1'];CompressionStream=undefined");await assert.rejects(()=>run("prepareTransfer(fixture,'deflate-blocks-v1',()=>{})"),/browser cannot compress/);
 console.log('Upload page tests passed: original preservation, compression roundtrip, capability/browser gating, pairing, extended split/four-track delivery, partial failure.');
})().catch(e=>{console.error(e);process.exitCode=1});
