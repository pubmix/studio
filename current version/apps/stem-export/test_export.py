import importlib.util, json, tempfile, unittest, wave
from pathlib import Path
import numpy as np
from export_stems import export

class ExportTests(unittest.TestCase):
    def fixture(self, path, count=4800):
        signal = np.rint(np.sin(np.arange(count)*2*np.pi*440/48000)*20000).astype('<i2')
        with wave.open(str(path),'wb') as w:
            w.setparams((1,2,48000,count,'NONE','not compressed')); w.writeframes(signal.tobytes())
    def test_format_gain_and_refusal(self):
        with tempfile.TemporaryDirectory() as t:
            t=Path(t); src=t/'mono.wav';self.fixture(src)
            out=t/'export'; m=export([src]*4,out,1,'Test','test')
            validator_path = Path(__file__).resolve().parents[2] / 'firmware/teensy/tools/validate_prepared.py'
            spec=importlib.util.spec_from_file_location('validator',validator_path)
            validator=importlib.util.module_from_spec(spec);spec.loader.exec_module(validator)
            self.assertEqual(validator.validate(out)['stage'], 'ready')
            samples=[]
            for s in m['stems']:
                with wave.open(str(out/s['file'])) as w:
                    self.assertEqual((w.getnchannels(),w.getsampwidth(),w.getframerate()),(2,2,44100))
                    self.assertEqual(w.getnframes(),m['frames'])
                    samples.append(np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(np.int32))
            self.assertLessEqual(np.abs(sum(samples)).max(),29208)
            self.assertEqual(m['frames'],4410)
            self.assertLess(json.loads((out/'export.json').read_text())['shared_gain'],1)
            self.assertEqual(len(list((out/'sd-root-files').glob('*.wav'))),4)
            with self.assertRaises(ValueError): export([src]*4,out,1,'Test','test')
    def test_mismatched_length_not_published(self):
        with tempfile.TemporaryDirectory() as t:
            t=Path(t); a=t/'a.wav';b=t/'b.wav';self.fixture(a);self.fixture(b,2400)
            with self.assertRaises(ValueError): export([a,a,a,b],t/'out',1,'Test','test')
            self.assertFalse((t/'out').exists())
if __name__=='__main__':unittest.main()
