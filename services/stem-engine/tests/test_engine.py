import json
from pathlib import Path
import numpy as np
import pytest
import soundfile as sf
from studio_stem_engine import Engine, EngineError, Settings, STEMS
from studio_stem_engine.engine import canonical, consistent
from studio_stem_engine.audio import align, resample

class Fake:
    def __init__(self): self.calls = 0
    def identity(self, settings): return {"test": 1}
    def separate(self, audio, rate, model, settings, scratch, progress):
        self.calls += 1
        return {"vocals": audio*.1, "other": audio*.2, "guitar": audio*.1,
                "piano": audio*.1, "bass": audio*.2, "drums": audio*.2}

@pytest.fixture
def fixture(tmp_path):
    path = tmp_path / 'song.wav'
    data = np.random.default_rng(7).normal(0, .1, (4801, 2)).astype('float32')
    sf.write(path, data, 48000, subtype='FLOAT')
    return path, data

def test_mapping():
    result = canonical(Fake().separate(np.ones((10,2)), 44100, None, None, None, None))
    assert tuple(result) == STEMS
    assert np.allclose(result['melody'], .4)
    with pytest.raises(EngineError): canonical({'instrumental': np.zeros((10,2))})

def test_contract_cache_corruption(tmp_path, fixture):
    source, mixture = fixture
    backend = Fake()
    engine = Engine(tmp_path/'out', backends={'demucs': backend})
    events=[]
    out = engine.separate(source, progress=events.append)
    assert backend.calls == 1
    assert len(list(out.glob('*.wav'))) == 4
    values = [sf.read(out/f'{s}.wav',dtype='float32',always_2d=True)[0] for s in STEMS]
    assert np.max(np.abs(sum(values)-mixture)) < 1e-7
    assert engine.separate(source) == out
    assert backend.calls == 1
    assert events[-1]['stage'] == 'complete'
    with open(out/'vocals.wav', 'ab') as f: f.write(b'corrupt')
    with pytest.raises(EngineError, match='checksum'): engine.separate(source)

def test_failure_atomic(tmp_path, fixture):
    class Broken(Fake):
        def separate(self,*a): raise RuntimeError('model failed')
    root = tmp_path/'out'
    with pytest.raises(EngineError, match='model failed'):
        Engine(root,backends={'demucs':Broken()}).separate(fixture[0])
    assert not list(root.glob('*/metadata.json'))
    assert not list(root.glob('.pending-*'))

def test_settings_invalidate(tmp_path, fixture):
    engine = Engine(tmp_path/'out',backends={'demucs':Fake()})
    assert engine.separate(fixture[0],Settings(seed=0)) != engine.separate(fixture[0],Settings(seed=1))

def test_staged_preserves_missed_vocals(tmp_path, fixture):
    class Vocal(Fake):
        def separate(self,a,*rest): return {'vocals':a*.3,'instrumental':a*.7}
    engine=Engine(tmp_path/'out',backends={'demucs':Fake(),'mlx':Vocal()})
    out=engine.separate(fixture[0],Settings(preset='best'))
    vocal=sf.read(out/'vocals.wav',dtype='float32')[0]
    assert np.allclose(vocal,fixture[1]*.37)

@pytest.mark.parametrize('policy',['melody','energy'])
def test_projection(policy):
    mix=np.ones((17,2),dtype='float32')*2
    result,before=consistent(mix,{s:mix*.1 for s in STEMS},policy)
    assert before > 0
    assert np.allclose(sum(result.values()),mix)

def test_alignment_rejects_truncation():
    with pytest.raises(ValueError): align(np.ones((10,2)),100,2)
    with pytest.raises(ValueError): align(np.full((10,2),np.nan),10,2)
    assert align(np.ones((9,2)),10,2).shape == (10,2)

def test_resample_grid():
    wave=np.zeros((48001,2),dtype='float32');wave[1000]=1
    back=align(resample(resample(wave,48000,44100),44100,48000),len(wave),2)
    assert np.argmax(back[:,0]) == 1000

@pytest.mark.parametrize('shape',[(1234,1),(1234,2)])
def test_silence(tmp_path, shape):
    source=tmp_path/'silent.wav';sf.write(source,np.zeros(shape),44100,subtype='FLOAT')
    out=Engine(tmp_path/'out',backends={'demucs':Fake()}).separate(source)
    assert Engine.verify(out)['channels']==shape[1]

def test_invalid_input(tmp_path):
    source=tmp_path/'empty.wav';sf.write(source,np.zeros((0,2)),44100)
    with pytest.raises(EngineError): Engine(tmp_path/'out',backends={'demucs':Fake()}).separate(source)

def test_mp3(tmp_path, fixture):
    import subprocess, imageio_ffmpeg
    from studio_stem_engine.audio import read
    source=tmp_path/'encoded.mp3'
    subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),'-v','error','-i',str(fixture[0]),str(source)],check=True)
    decoded,rate=read(source)
    out=Engine(tmp_path/'out',backends={'demucs':Fake()}).separate(source)
    assert Engine.verify(out)['frames']==len(decoded)

def test_metric_gain_and_silence():
    from studio_stem_engine.benchmark import metrics
    x=np.random.default_rng(2).normal(size=(1000,2))
    result=metrics(x,x*.5)
    assert result['si_sdr_db'] > 100
    assert 5.9 < result['snr_db'] < 6.1
    assert metrics(x*0,x)['si_sdr_db'] is None

def test_ensemble_weighting():
    from studio_stem_engine.backends import EnsembleBackend
    ensemble=EnsembleBackend([(Fake(),'a',1),(Fake(),'b',3)])
    result=ensemble.separate(np.ones((100,2)),44100,None,Settings(),None,lambda e:None)
    assert np.allclose(result['melody'],.4)
    with pytest.raises(ValueError): EnsembleBackend([(Fake(),'a',float('nan'))])

def test_cache_serializes_processes(tmp_path, fixture):
    import subprocess, sys
    script=tmp_path/'worker.py'
    script.write_text('''
import sys,time
from pathlib import Path
from studio_stem_engine import Engine
class Backend:
 def identity(self,settings): return {"version":1}
 def separate(self,a,*args):
  with open(sys.argv[3],"a") as f: f.write("inference\\n")
  time.sleep(.2)
  return {"vocals":a*.1,"other":a*.3,"bass":a*.2,"drums":a*.4}
Engine(sys.argv[2],backends={"demucs":Backend()}).separate(sys.argv[1])
''')
    calls=tmp_path/'calls.txt'
    command=[sys.executable,str(script),str(fixture[0]),str(tmp_path/'out'),str(calls)]
    first=subprocess.Popen(command);second=subprocess.Popen(command)
    assert first.wait(timeout=20)==second.wait(timeout=20)==0
    assert calls.read_text().splitlines()==['inference']

@pytest.mark.parametrize("payload", ["{}", "[]", "null"])
def test_malformed_metadata_error(tmp_path, payload):
    (tmp_path/'metadata.json').write_text(payload)
    with pytest.raises(EngineError): Engine.verify(tmp_path)

def test_noncanonical_metadata_names(tmp_path, fixture):
    out=Engine(tmp_path/'out',backends={'demucs':Fake()}).separate(fixture[0])
    meta=json.loads((out/'metadata.json').read_text())
    meta['stems'][1]['name']='OTHER'
    (out/'metadata.json').write_text(json.dumps(meta))
    with pytest.raises(EngineError): Engine.verify(out)
