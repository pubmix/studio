import json
import sys
from pathlib import Path
from dataclasses import replace
import numpy as np
import pytest
import soundfile as sf
from studio_stem_engine import Engine, EngineError, Settings, SeparationResult
from studio_stem_engine.transfer import export_transfer, verify_transfer
from studio_stem_engine.sources import FAMILIES, select_sources
from test_engine import Fake, fixture


def test_extended_mixdown_transfer_and_cache(tmp_path, fixture):
    backend = Fake()
    engine = Engine(tmp_path/'out', backends={'demucs': backend})
    options = Settings(mode='extended', consistency='none')
    result = engine.separate(fixture[0], options)
    meta = Engine.verify(result)
    assert len(meta['stems']) == 6
    assert [s['channel'] for s in meta['stems']] == list(range(1, 7))
    assert meta['stems'][4]['bank'] == 2 and meta['stems'][4]['lane'] == 1
    assert engine.separate(fixture[0], options) == result and backend.calls == 1
    four = export_transfer(result, tmp_path/'four', set_id=1)
    extended = export_transfer(result, tmp_path/'extended', set_id=1, extended=True)
    assert len(verify_transfer(four)['stems']) == 4
    assert len(verify_transfer(extended, extended=True)['stems']) == 6
    with pytest.raises(EngineError): verify_transfer(extended)
    meta['stems'][0]['file'] = '../bad.wav'
    (result/'metadata.json').write_text(json.dumps(meta))
    with pytest.raises(EngineError): Engine.verify(result)


def test_suppression_preserves_audio_and_unknown_confidence(tmp_path, fixture):
    class Confident(Fake):
        def separate(self, audio, *args):
            return SeparationResult(super().separate(audio, *args), {'guitar': .1, 'piano': .9})
    result = Engine(tmp_path/'out', backends={'demucs': Confident()}).separate(fixture[0], Settings(mode='dynamic'))
    meta = Engine.verify(result)
    assert 'GUITAR' not in [s['name'] for s in meta['stems']]
    assert meta['source_measurements']['guitar']['suppressed_reason'] == 'low_confidence'
    assert meta['source_measurements']['bass']['presence_confidence'] is None
    audio = sum(sf.read(result/s['file'], dtype='float32')[0] for s in meta['stems'])
    assert np.allclose(audio, fixture[1], atol=1e-7)


@pytest.mark.parametrize('depth', [4, 5, 8, 16])
def test_channel_limit_partition_and_reconstruction(depth):
    audio = np.ones((100, 2), dtype='float32')*.1
    names = list(FAMILIES)
    raw = {name: audio/len(names) for name in names}
    result, channels, measurements, _ = select_sources(audio, raw, Settings(mode='dynamic', stem_depth=depth))
    assert len(result) <= depth
    assert len(channels) == len(result)
    assert np.allclose(sum(result.values()), audio)
    assert all(1 <= c['bank'] <= 4 and 1 <= c['lane'] <= 4 for c in channels)


@pytest.mark.parametrize('change', [dict(stem_depth=17), dict(stem_depth=3), dict(stem_depth=True), dict(min_confidence=float('nan')), dict(presence_db=1), dict(mode='other'), dict(mode='extended', preset='best')])
def test_invalid_settings(change):
    with pytest.raises(ValueError): Settings(**change)


def test_two_stem_model_rejected_or_explicit_fallback(tmp_path, fixture):
    class Vocal(Fake):
        def separate(self, a, *args): return {'vocals': a*.3, 'instrumental': a*.7}
    engine = Engine(tmp_path/'out', backends={'roformer': Vocal(), 'demucs': Fake()})
    options = Settings(mode='extended', backend='roformer', model='vocal.ckpt')
    with pytest.raises(EngineError): engine.separate(fixture[0], options)
    out = engine.separate(fixture[0], replace(options, fallback=True))
    meta = Engine.verify(out)
    assert meta['schema'] == 'studio.stems.v1'
    assert meta['fallback']['requested']['backend'] == 'roformer'
    assert meta['settings']['backend'] == 'demucs'


def test_failed_identity_can_fallback(tmp_path, fixture):
    class Missing(Fake):
        def identity(self, settings): raise EngineError('weights missing')
    out = Engine(tmp_path/'out', backends={'roformer': Missing(), 'demucs': Fake()}).separate(
        fixture[0], Settings(backend='roformer', model='x.ckpt', mode='dynamic', fallback=True))
    assert Engine.verify(out)['fallback']['reason'] == 'weights missing'


@pytest.mark.parametrize('confidence', [float('nan'), -.1, 1.1])
def test_bad_confidence_rejected(confidence):
    a = np.ones((8, 2), dtype='float32')
    raw = Fake().separate(a, 1, None, None, None, None)
    with pytest.raises(ValueError): select_sources(a, raw, Settings(mode='dynamic'), {'guitar': confidence})


def test_worker_preserves_youtube_acquisition(tmp_path, fixture, monkeypatch):
    from studio_stem_engine import cloud_worker
    folder = tmp_path/'job';folder.mkdir()
    (folder/'request.json').write_text(json.dumps({'video_id': 'abcdefghijk', 'set_id': 9,
        'settings': {'preset': 'fast', 'mode': 'extended', 'consistency': 'none'}}))
    acquired = []
    def acquire(identifier, destination, emit):
        acquired.append(identifier)
        return fixture[0], 'Authorized fixture'
    monkeypatch.setattr(cloud_worker, 'acquire', acquire)
    monkeypatch.setattr(cloud_worker, 'Engine', lambda output: Engine(output, backends={'demucs': Fake()}))
    # Worker also verifies using the class static API.
    cloud_worker.Engine.verify = Engine.verify
    monkeypatch.setattr(sys, 'argv', ['worker', str(folder)])
    cloud_worker.main()
    assert acquired == ['abcdefghijk']
    assert len(verify_transfer(folder/'prepared')['stems']) == 4
    assert len(verify_transfer(folder/'extended', extended=True)['stems']) == 6


def test_benchmark_compares_groups_and_sources(tmp_path, fixture, monkeypatch):
    from studio_stem_engine import benchmark as module
    source, mixture = fixture
    refs = {}
    for name, scale in [('vocals', .1), ('melody', .5), ('bass', .2), ('rhythm', .2)]:
        path = tmp_path/(name+'.wav');sf.write(path, mixture*scale, 48000, subtype='FLOAT');refs[name] = str(path)
    manifest = tmp_path/'benchmark.json'
    manifest.write_text(json.dumps({'purpose':'fixture', 'tracks':[{'id':'a', 'mixture':str(source), 'references':refs}],
        'configurations':[{'name':mode, 'settings':{'mode':mode}} for mode in ['four','extended']]}))
    class TestEngine(Engine):
        def __init__(self, output): super().__init__(output, backends={'demucs': Fake()})
    monkeypatch.setattr(module, 'Engine', TestEngine)
    report = module.benchmark(manifest, tmp_path/'report')
    assert all(row['status'] == 'ok' for row in report['runs'])
    assert report['runs'][0]['metrics']['bass']['snr_db'] > 100


def test_training_split_leaks_and_deterministic_mixtures(tmp_path, fixture):
    from studio_stem_engine.training import validate, prepare
    source, mixture = fixture
    sources = {}
    for i, name in enumerate(['vocals', 'other', 'bass', 'drums']):
        path = tmp_path/(name+'.wav');sf.write(path, mixture*(i+1)/10, 48000, subtype='FLOAT');sources[name] = str(path)
    track = {'id':'song', 'recording_group':'session1', 'split':'train', 'rights':'Own recording', 'sources':sources}
    manifest = tmp_path/'training.json';manifest.write_text(json.dumps({'tracks':[track]}))
    assert len(validate(manifest)) == 1
    a = prepare(manifest, tmp_path/'a', seed=42, seconds=.05)
    b = prepare(manifest, tmp_path/'b', seed=42, seconds=.05)
    assert a == b
    entry = a['tracks'][0]
    summed = sum(sf.read(tmp_path/'a'/v)[0] for v in entry['sources'].values())
    mix = sf.read(tmp_path/'a'/entry['mixture'])[0]
    assert np.allclose(summed, mix, atol=1e-7)
    manifest.write_text(json.dumps({'tracks':[track, {**track, 'id':'copy', 'split':'test'}]}))
    with pytest.raises(ValueError, match='leaks'): validate(manifest)


def test_separator_adapter_full_source_labels_and_gain(tmp_path, monkeypatch):
    """Pinned public API shape, simulated runtime; does not claim real model quality."""
    import types
    from studio_stem_engine.backends import SeparatorBackend
    observed = {}
    class Separator:
        def __init__(self, **kwargs):
            self.kwargs = kwargs
            self.model_instance = types.SimpleNamespace(normalization_threshold=1)
        def load_model(self, model_filename): observed['model'] = model_filename
        def separate(self, source, custom_output_names):
            assert self.model_instance.normalization_threshold == float('inf')
            audio, rate = sf.read(source, always_2d=True)
            names = ['lead vocal', 'backing vocals', 'guitar', 'piano', 'bass', 'kick', 'snare', 'hats']
            outputs = []
            for name in names:
                filename = custom_output_names[name]+'.wav'
                sf.write(Path(self.kwargs['output_dir'])/filename, audio/len(names), rate, subtype='FLOAT')
                outputs.append(filename)
            return outputs
    module = types.ModuleType('audio_separator.separator'); module.Separator = Separator
    monkeypatch.setitem(sys.modules, 'audio_separator.separator', module)
    torch = types.ModuleType('torch'); torch.manual_seed = lambda seed: None; torch.device = lambda name: name
    monkeypatch.setitem(sys.modules, 'torch', torch)
    monkeypatch.setenv('STUDIO_MODEL_DIR', str(tmp_path/'models'))
    audio = np.ones((100, 2), dtype='float32')*1.5
    backend = SeparatorBackend('audio-separator')
    result = backend.separate(audio, 44100, 'full.ckpt', Settings(), tmp_path, lambda e: None)
    assert set(result) == {'lead_vocal', 'backing_vocals', 'guitar', 'piano', 'bass', 'kick', 'snare', 'hats'}
    assert np.allclose(sum(result.values()), audio)
    assert observed['model'] == 'full.ckpt'
