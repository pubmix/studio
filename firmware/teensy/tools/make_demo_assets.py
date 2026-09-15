#!/usr/bin/env python3
"""Original synthetic fixtures, not separated music or finished factory recordings."""
import json, math, pathlib, random, struct, sys, wave
root=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'demo-assets')
root.mkdir(parents=True,exist_ok=True)
sr=44100
rng=random.Random(7)
def write(path,samples):
    path.parent.mkdir(parents=True,exist_ok=True)
    with wave.open(str(path),'wb') as w:
        w.setparams((2,2,sr,0,'NONE','not compressed'))
        w.writeframes(b''.join(struct.pack('<hh',int(max(-.98,min(.98,x))*32767),int(max(-.98,min(.98,x))*32767)) for x in samples))
for name,hz in zip(('vocals','melody','bass','rhythm'),(440,330,110,55)):
    write(root/'prepared'/f'{name}.wav',(.12*math.sin(2*math.pi*hz*n/sr)*(1 if n%22050<11025 else 0) for n in range(sr*4)))
manifest={'api_version':1,'set_id':1,'title':'Synthetic synchronization fixture','sample_rate':sr,'frames':sr*4,'alignment_offset_frames':0,'stems':[{'role':name.upper(),'file':f'{name}.wav'} for name in ('vocals','melody','bass','rhythm')]}
(root/'prepared'/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
for name in ('Kick','Snare','Hi-Hat','Clap','Guitar Skank','Piano Clash','Airhorn'):
    audio=[]
    for n in range(sr):
        t=n/sr;noise=rng.uniform(-1,1)
        if name=='Kick':x=.7*math.sin(2*math.pi*(48*t+7*(1-math.exp(-30*t))))*math.exp(-12*t)
        elif name=='Snare':x=(.5*noise+.2*math.sin(2*math.pi*180*t))*math.exp(-18*t)
        elif name=='Hi-Hat':x=.3*noise*math.exp(-55*t)
        elif name=='Clap':x=.4*noise*math.exp(-18*t)*(1 if n%1800<700 else .1)
        elif name=='Airhorn':x=.15*sum(math.sin(2*math.pi*440*h*t)/h for h in range(1,9))*min(1,30*t)*max(0,1-t)
        else:x=.14*sum(math.sin(2*math.pi*f*t) for f in (261.63,329.63,392))*math.exp(-(12 if name=='Guitar Skank' else 6)*t)
        audio.append(x)
    write(root/'samples'/f'{name}.wav',audio)
print(root.resolve())
