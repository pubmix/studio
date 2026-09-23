#!/usr/bin/env python3
"""Version 1 STUDIO transfer validation; no ML, decoding or resampling."""
import argparse, hashlib, json, pathlib, re, wave
ROLES=('VOCALS','MELODY','BASS','RHYTHM')
def validate(folder):
    root=pathlib.Path(folder).resolve()
    m=json.loads((root/'manifest.json').read_text())
    if not isinstance(m,dict) or type(m.get('api_version')) is not int or m['api_version']!=1 or type(m.get('set_id')) is not int or not 1<=m['set_id']<=0xffffffff:raise ValueError('Unsupported version or missing set_id')
    if type(m.get('sample_rate')) is not int or m['sample_rate']!=44100 or type(m.get('alignment_offset_frames')) is not int or m['alignment_offset_frames']!=0:raise ValueError('Require aligned 44100 Hz transfer assets')
    if type(m.get('frames')) is not int or m['frames']<=0:raise ValueError('Invalid frame count')
    stems=m.get('stems',[])
    if not isinstance(stems,list) or len(stems)!=4:raise ValueError('Exactly four stems required')
    result=[]
    for role,item in zip(ROLES,stems):
        if not isinstance(item,dict) or item.get('role')!=role or item.get('file')!=role.lower()+'.wav':raise ValueError('Canonical order or filename mismatch')
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
        if 'sha256' in item and (not isinstance(item['sha256'],str) or not re.fullmatch(r'[0-9a-f]{64}',item['sha256']) or item['sha256']!=digest):raise ValueError('Checksum mismatch: '+role)
        result.append({'role':role,'sha256':digest})
    return {'api_version':1,'stage':'ready','progress':1.0,'set_id':m['set_id'],'frames':m['frames'],'stems':result}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder');args=p.parse_args()
    try:print(json.dumps(validate(args.folder),indent=2))
    except (ValueError,OSError,KeyError,wave.Error,EOFError) as e:p.exit(1,'Import failed: '+str(e)+'\n')
