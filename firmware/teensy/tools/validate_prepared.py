#!/usr/bin/env python3
"""Version 1 STUDIO transfer validation; no ML, decoding or resampling."""
import argparse, hashlib, json, pathlib, wave
ROLES=('VOCALS','MELODY','BASS','RHYTHM')
def validate(folder):
    root=pathlib.Path(folder).resolve()
    m=json.loads((root/'manifest.json').read_text())
    if m.get('api_version')!=1 or not isinstance(m.get('set_id'),int) or m['set_id']<=0:raise ValueError('Unsupported version or missing set_id')
    if m.get('sample_rate')!=44100 or m.get('alignment_offset_frames')!=0:raise ValueError('Require aligned 44100 Hz transfer assets')
    if not isinstance(m.get('frames'),int) or m['frames']<=0:raise ValueError('Invalid frame count')
    stems=m.get('stems',[])
    if len(stems)!=4:raise ValueError('Exactly four stems required')
    result=[]
    for role,item in zip(ROLES,stems):
        if item.get('role')!=role or item.get('file')!=role.lower()+'.wav':raise ValueError('Canonical order or filename mismatch')
        p=root/item['file']
        if p.is_symlink() or p.resolve().parent!=root:raise ValueError('Asset must be inside transfer folder')
        with wave.open(str(p),'rb') as w:
            if (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes(),w.getcomptype())!=(2,2,44100,m['frames'],'NONE'):raise ValueError('Format/frame mismatch: '+role)
            count=0
            while True:
                block=w.readframes(8192)
                if not block:break
                count+=len(block)
            if count!=m['frames']*4:raise ValueError('Truncated audio: '+role)
        h=hashlib.sha256()
        with p.open('rb') as f:
            for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
        digest=h.hexdigest()
        if item.get('sha256') and item['sha256']!=digest:raise ValueError('Checksum mismatch: '+role)
        result.append({'role':role,'sha256':digest})
    return {'api_version':1,'stage':'ready','progress':1.0,'set_id':m['set_id'],'frames':m['frames'],'stems':result}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder');args=p.parse_args()
    try:print(json.dumps(validate(args.folder),indent=2))
    except (ValueError,OSError,KeyError,wave.Error,EOFError) as e:p.exit(1,'Import failed: '+str(e)+'\n')
