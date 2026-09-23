"""API/job lifecycle tests: no network or ML model required."""
import json
from pathlib import Path
import sys
import time
import pytest
pytest.importorskip('fastapi')
pytest.importorskip('httpx')
from fastapi.testclient import TestClient
from studio_stem_engine.cloud import Jobs, NewJob, create_app
from studio_stem_engine.media import video_id, decode

TOKEN = 'a' * 64
AUTH = {'Authorization': 'Bearer ' + TOKEN}
PAYLOAD = dict(video_id='abcdefghijk', rights_confirmed=True, request_id='request0000000001')


def wait(jobs, identifier):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        data = jobs.get(identifier)
        if data['state'] in ('ready', 'failed', 'cancelled'):
            return data
        time.sleep(.03)
    raise AssertionError('Worker did not finish')


def test_auth_rights_idempotency_and_cancel(tmp_path):
    manager = Jobs(tmp_path, command=[sys.executable, '-c', 'import time;time.sleep(30)'])
    with TestClient(create_app(token=TOKEN, manager=manager, searcher=lambda q: [])) as c:
        assert c.get('/healthz').status_code == 200
        assert c.get('/').status_code == 200
        assert c.get('/v1/search?q=hello').status_code == 401
        assert c.post('/v1/jobs', headers=AUTH, json={**PAYLOAD, 'rights_confirmed': False}).status_code == 400
        assert c.post('/v1/jobs', headers=AUTH, json={**PAYLOAD, 'rights_confirmed': 'true'}).status_code == 422
        result = c.post('/v1/jobs', headers=AUTH, json=PAYLOAD).json()
        assert c.post('/v1/jobs', headers=AUTH, json=PAYLOAD).json()['id'] == result['id']
        assert c.post('/v1/jobs', headers=AUTH, json={**PAYLOAD, 'request_id': 'request0000000002'}).status_code == 409
        assert c.get('/v1/jobs/'+result['id']+'/stems/vocals', headers=AUTH).status_code == 409
        assert c.post('/v1/jobs/'+result['id']+'/cancel', headers=AUTH, json={}).status_code == 200
        assert wait(manager, result['id'])['state'] == 'cancelled'


def test_timeout_restart_and_limits(tmp_path):
    manager = Jobs(tmp_path, command=[sys.executable, '-c', 'import time;time.sleep(30)'], timeout=.1)
    result = manager.create(NewJob(**PAYLOAD))
    assert wait(manager, result['id'])['state'] == 'failed'
    manager.close()
    result['state'] = 'separating';manager.save(result)
    restarted = Jobs(tmp_path)
    assert restarted.get(result['id'])['state'] == 'failed'
    with TestClient(create_app(token=TOKEN, manager=restarted, searcher=lambda q: [])) as c:
        assert c.post('/v1/jobs', headers=AUTH, content=b'x'*2049).status_code == 413
        assert c.post('/v1/uploads?title=evil.exe', headers=AUTH, content=b'x').status_code == 400
        assert c.post('/v1/uploads?title=song.wav', headers=AUTH, content=b'').status_code == 400
        for _ in range(6): assert c.get('/v1/search?q=song', headers=AUTH).status_code == 200
        assert c.get('/v1/search?q=song', headers=AUTH).status_code == 429


def test_verified_output_and_download(tmp_path, monkeypatch):
    worker=tmp_path/'fake_worker.py'
    worker.write_text('''import sys,json,wave,hashlib
from pathlib import Path
p=Path(sys.argv[1])/'prepared';p.mkdir()
m={'api_version':1,'set_id':1,'sample_rate':44100,'alignment_offset_frames':0,'frames':100,'title':'Test','stems':[]}
for role in ['vocals','melody','bass','rhythm']:
 f=p/(role+'.wav')
 with wave.open(str(f),'wb') as w:
  w.setnchannels(2);w.setsampwidth(2);w.setframerate(44100);w.writeframes(b'\\0'*400)
 m['stems'].append({'role':role.upper(),'file':f.name,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
(p/'manifest.json').write_text(json.dumps(m))
''')
    manager=Jobs(tmp_path/'data',command=[sys.executable,str(worker)])
    def transfer(folder, device, progress):
        files=[]
        for lane, role in enumerate(('VOCALS','MELODY','BASS','RHYTHM'),1):
            entry=dict(lane=lane,role=role,file=f'S1-{role}.wav')
            progress(dict(stage='uploaded',**entry));files.append(entry)
        return files
    monkeypatch.setattr('studio_stem_engine.device.upload_transfer',transfer)
    with TestClient(create_app(token=TOKEN,manager=manager,device_url='http://dubbox.local')) as c:
        reply=c.post('/v1/uploads?title=song.mp3',headers=AUTH,content=b'test fixture input')
        assert reply.status_code == 202
        identifier=reply.json()['id']
        assert wait(manager,identifier)['state']=='ready'
        assert not (manager.root/identifier/'input.audio').exists()
        r=c.get('/v1/jobs/'+identifier+'/stems/vocals',headers=AUTH)
        assert r.status_code==200 and r.content[:4]==b'RIFF'
        assert c.get('/v1/jobs/'+identifier+'/stems/unknown',headers=AUTH).status_code==404
        assert c.get('/v1/jobs',headers=AUTH).json()['jobs'][0]['id']==identifier
        assert c.post('/v1/jobs/'+identifier+'/send',headers=AUTH,json={}).status_code==202
        deadline=time.monotonic()+2
        while time.monotonic()<deadline:
            delivery=c.get('/v1/device',headers=AUTH).json()
            if delivery['state']=='done': break
            time.sleep(.02)
        assert delivery['state']=='done' and len(delivery['files'])==4


@pytest.mark.parametrize('url',['https://example.com/abcdefghijk','http://youtu.be/abcdefghijk','https://youtube.com.evil.test/watch?v=abcdefghijk','https://user:pw@youtube.com/watch?v=abcdefghijk','https://youtube.com/watch?v=bad'])
def test_media_rejects_other_targets(url):
    with pytest.raises(ValueError): video_id(url)


def test_media_ids():
    assert video_id('https://youtu.be/abcdefghijk')=='abcdefghijk'
    assert video_id('https://www.youtube.com/watch?v=abcdefghijk&list=something')=='abcdefghijk'
