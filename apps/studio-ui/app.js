import {ROLES,THEMES,clamp,defaults,restore,validateBuffers} from './model.js';
const $=s=>document.querySelector(s);
let state=defaults();try{state=restore(localStorage.getItem('studio-ui-v1'));}catch{}
let ctx, master, analyser, buses=[], buffers=[], sources=[], playing=false, offset=0, started=0, duration=0, busy=false, selectedFx=0;
const samples=[null,null,null,null];
const status=message=>$('#status').textContent=message;
const time=t=>Math.floor(t/60).toString().padStart(2,'0')+':'+Math.floor(t%60).toString().padStart(2,'0');
function mode(name){document.querySelectorAll('.page').forEach(p=>p.hidden=p.id!==name.toLowerCase());document.querySelectorAll('nav button').forEach(b=>b.setAttribute('aria-current',String(b.textContent===name)));}
for(const name of ['DUB','JAM','DAW','FILES','SETTINGS']){const b=document.createElement('button');b.textContent=name;b.onclick=()=>mode(name);$('#modes').append(b);}
ROLES.forEach((role,i)=>{
 const color='var(--lane'+i+')';
 const row=document.createElement('div');row.className='wave-row';row.style.setProperty('--color',color);row.innerHTML='<span>'+role+'</span><canvas aria-label="'+role+' waveform"></canvas>';$('#waves').append(row);
 const lane=document.createElement('article');lane.className='lane';lane.style.setProperty('--color',color);
 lane.innerHTML='<div class="lane-title"><span>'+role+'</span><small>0'+(i+1)+'</small></div><button class="fx-toggle" aria-pressed="false" aria-label="'+role+' echo toggle">DUB ECHO · OFF</button><div class="knob-wrap"><button class="knob" aria-label="Edit '+role+' echo"></button><input class="amount" aria-label="'+role+' echo intensity" type="range" min="0" max="100"><span class="tiny amount-label"></span></div><div class="fader-wrap"><div class="ticks"><span>0</span><span>−6</span><span>−12</span><span>−∞</span></div><input class="fader" aria-label="'+role+' dry volume" type="range" min="0" max="100" value="80"><div class="ticks"><span>—</span><span>—</span><span>—</span><span>—</span></div></div><output class="level"></output><button class="sample" aria-label="Trigger '+role+' one-shot">↗ ONE SHOT '+(i+1)+'</button><button class="sample-config">Assign sample</button><input type="file" accept="audio/*">';
 lane.querySelector('.fx-toggle').onclick=()=>{state.fx[i]=!state.fx[i];sync();};
 lane.querySelector('.amount').oninput=e=>{state.amounts[i]=e.target.value/100;sync();};
 lane.querySelector('.knob').onclick=()=>{selectedFx=i;$('#fx-title').textContent=role+' / Dub echo';$('#fx-amount').value=state.amounts[i]*100;$('#fx-dialog').showModal();};
 lane.querySelector('.knob').onwheel=e=>{e.preventDefault();state.amounts[i]=clamp(state.amounts[i]+(e.deltaY<0?.01:-.01));sync();};
 lane.querySelector('.fader').oninput=e=>{state.levels[i]=e.target.value/100;sync();};
 lane.querySelector('.sample').onclick=()=>oneShot(i);
 lane.querySelector('.sample-config').onclick=()=>lane.querySelector('input[type=file]').click();
 lane.querySelector('input[type=file]').onchange=async e=>{try{await audio();const f=e.target.files[0];if(!f)return;if(f.size>25e6)throw Error('One-shot limit is 25 MB.');samples[i]=await ctx.decodeAudioData(await f.arrayBuffer());lane.querySelector('.sample').textContent='↗ '+f.name.slice(0,18);status('Assigned '+f.name+' to '+role+'.');}catch(e){status(e.message);}};
 $('#lanes').append(lane);
 const label=document.createElement('label');label.textContent=role;const input=document.createElement('input');input.type='file';input.accept='audio/*';input.id='stem-'+i;label.append(input);$('#imports').append(label);
});
for(const theme of THEMES){const option=document.createElement('option');option.value=option.textContent=theme;$('#theme').append(option);}
$('#theme').onchange=e=>{state.theme=e.target.value;sync();};
$('#bpm').onchange=e=>{state.bpm=clamp(e.target.value,20,300);sync();status('Echo tempo updated. Stem playback speed stays unchanged.');};
$('#fx-amount').oninput=e=>{state.amounts[selectedFx]=e.target.value/100;sync();};
$('#close-fx').onclick=()=>$('#fx-dialog').close();
$('#fx-dialog').onclick=e=>{if(e.target===$('#fx-dialog'))$('#fx-dialog').close();};
function sync(){
 document.body.dataset.theme=state.theme;$('#theme').value=state.theme;$('#bpm').value=state.bpm;
 document.querySelectorAll('.lane').forEach((lane,i)=>{
 lane.querySelector('.fx-toggle').setAttribute('aria-pressed',state.fx[i]);lane.querySelector('.fx-toggle').textContent='DUB ECHO · '+(state.fx[i]?'ON':'OFF');
 lane.querySelector('.amount').value=state.amounts[i]*100;lane.querySelector('.amount-label').textContent=Math.round(state.amounts[i]*100)+'% INTENSITY';
 lane.querySelector('.knob').style.setProperty('--angle',(-135+state.amounts[i]*270)+'deg');
 lane.querySelector('.fader').value=state.levels[i]*100;lane.querySelector('.level').textContent=state.levels[i]>0?(20*Math.log10(state.levels[i])).toFixed(1)+' dB':'−∞ dB';
 if(ctx){const b=buses[i],t=ctx.currentTime;b.dry.gain.setTargetAtTime(state.levels[i],t,.015);b.send.gain.setTargetAtTime(state.fx[i]?.45:0,t,.015);b.feedback.gain.setTargetAtTime(.15+state.amounts[i]*.6,t,.02);b.wet.gain.setTargetAtTime(.15+state.amounts[i]*.65,t,.02);b.delay.delayTime.setTargetAtTime(60/state.bpm*.75,t,.1);}
 });$('#saved').textContent='LOCAL';
}
async function audio(){
 if(!ctx){ctx=new AudioContext();master=ctx.createGain();master.gain.value=.65;const limiter=ctx.createDynamicsCompressor();limiter.threshold.value=-6;limiter.knee.value=6;limiter.ratio.value=16;limiter.attack.value=.003;limiter.release.value=.15;analyser=ctx.createAnalyser();analyser.fftSize=256;master.connect(limiter).connect(analyser).connect(ctx.destination);
 buses=ROLES.map(()=>{const input=ctx.createGain(),dry=ctx.createGain(),send=ctx.createGain(),delay=ctx.createDelay(3),feedback=ctx.createGain(),wet=ctx.createGain();input.connect(dry).connect(master);input.connect(send).connect(delay);delay.connect(feedback).connect(delay);delay.connect(wet).connect(master);return {input,dry,send,delay,feedback,wet};});sync();}
 await ctx.resume();
}
function cursor(){return playing?Math.max(0,Math.min(duration,offset+ctx.currentTime-started)):offset;}
function halt(reset=false){offset=cursor();playing=false;for(const s of sources){try{s.stop();}catch{}}sources=[];if(reset)offset=0;$('#play').textContent='▶';$('#play').setAttribute('aria-label','Play');}
async function play(){if(busy)return;if(!buffers.length){status('Load the demo or four stems first.');return;}await audio();if(playing){halt();return;}if(offset>=duration)offset=0;started=ctx.currentTime+.04;playing=true;
 sources=buffers.map((buffer,i)=>{const s=ctx.createBufferSource();s.buffer=buffer;s.connect(buses[i].input);s.start(started,offset);s.onended=()=>s.disconnect();return s;});$('#play').textContent='Ⅱ';$('#play').setAttribute('aria-label','Pause');}
$('#play').onclick=play;$('#stop').onclick=()=>halt(true);
$('#seek').oninput=async e=>{const was=playing;halt();offset=Number(e.target.value)*duration;if(was)await play();};
function install(next,title){validateBuffers(next);halt(true);buffers=next;duration=next[0].duration;$('#track-name').textContent=title;$('#duration').textContent=' / '+time(duration);mode('DUB');draw();status('Ready. Press play; try an echo, then lower its dry fader.');}
$('#demo').onclick=async()=>{if(busy)return;busy=true;try{await audio();const rate=ctx.sampleRate,len=rate*16;const next=ROLES.map((_,lane)=>{const b=ctx.createBuffer(2,len,rate);for(let ch=0;ch<2;ch++){const out=b.getChannelData(ch);let seed=12345;for(let n=0;n<len;n++){const t=n/rate,beat=t%.6,step=Math.floor(t/.3),env=Math.exp(-beat*13);let v=0;
 if(lane===0)v=Math.sin(2*Math.PI*220*t)*Math.sin(2*Math.PI*2*t)*.11*(Math.sin(t*1.7)>.2?1:0);
 if(lane===1){const f=[261.63,329.63,392,493.88,392,329.63,293.66,329.63][step%8];v=Math.sin(2*Math.PI*f*t)*Math.exp(-(t%.3)*12)*.12;}
 if(lane===2)v=Math.sin(2*Math.PI*[65.41,65.41,87.31,73.42][Math.floor(t/2.4)%4]*t)*env*.25;
 if(lane===3){seed=(Math.imul(seed,1664525)+1013904223)|0;v=Math.sin(2*Math.PI*(48*beat+5*(1-Math.exp(-beat*30))))*Math.exp(-beat*25)*.3+(seed/2147483648)*Math.exp(-(t%.15)*130)*.065;}out[n]=v;}}return b;});install(next,'AFTER HOURS / 16-second synthetic demo · vocal lane uses a tone');}catch(e){status(e.message);}finally{busy=false;}};
$('#load-files').onclick=async()=>{if(busy)return;const fs=ROLES.map((_,i)=>$('#stem-'+i).files[0]);if(fs.some(f=>!f)){status('Assign a file to each of the four roles.');return;}if(fs.reduce((s,f)=>s+f.size,0)>100e6){status('Browser preview limit: 100 MB total.');return;}busy=true;$('#load-files').disabled=true;status('Decoding four stems…');try{await audio();const next=[];for(const f of fs)next.push(await ctx.decodeAudioData(await f.arrayBuffer()));install(next,'LOCAL STEM SET / '+fs[0].name);}catch(e){status('Could not load: '+e.message);}finally{busy=false;$('#load-files').disabled=false;}};
async function note(freq,seconds=.5){await audio();const osc=ctx.createOscillator(),gain=ctx.createGain(),now=ctx.currentTime;osc.type='triangle';osc.frequency.value=freq;gain.gain.setValueAtTime(0,now);gain.gain.linearRampToValueAtTime(.2,now+.012);gain.gain.exponentialRampToValueAtTime(.001,now+seconds);osc.connect(gain).connect(master);osc.start();osc.stop(now+seconds+.03);osc.onended=()=>{osc.disconnect();gain.disconnect();};}
async function oneShot(i){await audio();if(samples[i]){const s=ctx.createBufferSource();s.buffer=samples[i];s.connect(master);s.start();s.onended=()=>s.disconnect();}else{await note([523.25,659.25,783.99,1046.5][i],.7);status('Demo one-shot. Use Assign sample for your own audio.');}}
['C','D','E','F','G','A','B','C⁺'].forEach((name,i)=>{const b=document.createElement('button');b.textContent=name;b.onclick=()=>note(261.63*2**([0,2,4,5,7,9,11,12][i]/12));$('#pads').append(b);});
$('#save').onclick=()=>{try{localStorage.setItem('studio-ui-v1',JSON.stringify(state));$('#saved').textContent='SAVED';status('Mix, echo, tempo and appearance saved. Reload audio files after reopening.');}catch{status('Local storage unavailable in this browser.');}};
function draw(){document.querySelectorAll('canvas').forEach((canvas,i)=>{const box=canvas.getBoundingClientRect();canvas.width=Math.max(1,Math.round(box.width*devicePixelRatio));canvas.height=29*devicePixelRatio;const c=canvas.getContext('2d');c.strokeStyle=getComputedStyle(canvas.parentElement).color;c.globalAlpha=.75;c.lineWidth=devicePixelRatio;const data=buffers[i]?.getChannelData(0);for(let x=0;x<canvas.width;x+=3*devicePixelRatio){let peak=.025;if(data){const a=Math.floor(x/canvas.width*data.length),b=Math.floor((x+3*devicePixelRatio)/canvas.width*data.length),stride=Math.max(1,Math.floor((b-a)/40));for(let j=a;j<Math.min(b,data.length);j+=stride)peak=Math.max(peak,Math.abs(data[j]));}const h=Math.min(.48,peak*1.8)*canvas.height;c.beginPath();c.moveTo(x,canvas.height/2-h);c.lineTo(x,canvas.height/2+h);c.stroke();}});}
window.addEventListener('resize',draw);$('#theme').addEventListener('change',draw);
document.addEventListener('keydown',e=>{if(e.code==='Space'&&!['INPUT','SELECT','BUTTON','TEXTAREA'].includes(e.target.tagName)&&!$('#fx-dialog').open){e.preventDefault();play();}});
const readings=new Float32Array(256);
function frame(){const pos=Math.max(0,cursor());$('#position').textContent=time(pos);$('#seek').value=duration?pos/duration:0;if(playing&&pos>=duration)halt(true);if(analyser){analyser.getFloatTimeDomainData(readings);$('#meter').value=Math.max(...readings.map(Math.abs));}requestAnimationFrame(frame);}
mode('DUB');sync();draw();frame();
