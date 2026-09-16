export const ROLES = ['VOCALS','MELODY','BASS','RHYTHM'];
export const THEMES = ['Dark','Bright','Retro','Win','Aero','Analog','Terminal','Pixel'];
export const clamp = (x, a=0, b=1) => Math.max(a,Math.min(b,Number(x)||0));
export function validateBuffers(buffers) {
 if(buffers.length!==4) throw Error('Choose exactly four stems, in lane order.');
 const first=buffers[0];
 if(!first || !first.length) throw Error('Empty audio is not supported.');
 if(buffers.some(b=>b.length!==first.length || b.sampleRate!==first.sampleRate)) throw Error('All four stems must have the same duration and sample rate. Export synchronized stems first.');
 return first.duration;
}
export function defaults(){return {theme:'Dark',bpm:100,levels:[.8,.8,.8,.8],amounts:[.35,.35,.35,.35],fx:[false,false,false,false]};}
export function restore(raw) {
 const s=defaults(); try {const v=JSON.parse(raw);
 if(THEMES.includes(v.theme))s.theme=v.theme;
 if(Number.isFinite(v.bpm))s.bpm=clamp(v.bpm,20,300);
 for(const k of ['levels','amounts']) if(Array.isArray(v[k])&&v[k].length===4)s[k]=v[k].map(x=>clamp(x));
 if(Array.isArray(v.fx)&&v.fx.length===4)s.fx=v.fx.map(x=>x===true);
 }catch{} return s;
}