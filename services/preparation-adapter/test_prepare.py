import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

from prepare import prepare, validate, ROLES, CEILING

FIRMWARE = Path(os.environ["STUDIO_FIRMWARE_VALIDATOR"]) if "STUDIO_FIRMWARE_VALIDATOR" in os.environ else None


class PreparationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.source = self.root/"native"
        self.dest = self.root/"prepared"

    def tearDown(self):
        self.tmp.cleanup()

    def fixture(self, rate=48000, channels=2, frames=4801, amplitude=1.4, data=None):
        self.source.mkdir(exist_ok=True)
        if data is None:
            t = np.arange(frames)/rate
            base = amplitude*np.sin(2*np.pi*440*t)
            data = np.stack([base, -base*.7], axis=1) if channels==2 else base[:,None]
        roles=[]
        arrays=[]
        for i, role in enumerate(ROLES):
            x=(data*(i+1)/4).astype("float32")
            arrays.append(x)
            file=role.lower()+".wav"
            sf.write(self.source/file,x,rate,subtype="FLOAT")
            roles.append({"name":role,"file":file,"sha256":hashlib.sha256((self.source/file).read_bytes()).hexdigest()})
        m={"schema":"studio.stems.v1","key":"a"*64,"format":"WAV FLOAT32",
           "frames":len(data),"sample_rate":rate,"channels":channels,"stems":roles}
        self.metadata(m)
        return arrays

    def metadata(self, m):
        (self.source/"metadata.json").write_text(json.dumps(m))
    
    def mutate(self, fn):
        m=json.loads((self.source/"metadata.json").read_text());fn(m);self.metadata(m)

    def no_publication(self):
        self.assertFalse([p for p in self.dest.glob("set-*") if p.is_dir()])
        self.assertFalse(list(self.dest.glob(".staging-*")))

    def test_resample_gain_and_firmware_validator(self):
        arrays=self.fixture()
        before={p.name:p.read_bytes() for p in self.source.iterdir()}
        target=prepare(self.source,self.dest)
        m=validate(target);p=json.loads((target/"preparation.json").read_text())
        self.assertEqual(m["frames"],4411)
        self.assertLess(p["common_gain"],1)
        converted=[resample_poly(x.astype("float64"),147,160,axis=0) for x in arrays]
        for role,x in zip(ROLES,converted):
            y,rate=sf.read(target/(role.lower()+".wav"),always_2d=True)
            self.assertEqual(rate,44100)
            self.assertLessEqual(np.max(np.abs(y-x*p["common_gain"])),1.51/32768)
        summed=sum(sf.read(target/(r.lower()+".wav"),always_2d=True)[0] for r in ROLES)
        self.assertLessEqual(np.max(np.abs(summed)),CEILING+6/32768)
        self.assertEqual(before,{p.name:p.read_bytes() for p in self.source.iterdir()})
        if FIRMWARE is None:
            self.fail("Set STUDIO_FIRMWARE_VALIDATOR for the mandatory cross-project check")
        spec=importlib.util.spec_from_file_location("firmware_validator",FIRMWARE)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        result=module.validate(target)
        self.assertEqual(result["stage"],"ready")

    def test_mono_duplication(self):
        self.fixture(channels=1,rate=32000)
        target=prepare(self.source,self.dest)
        for role in ROLES:
            x,_=sf.read(target/(role.lower()+".wav"),dtype="int16",always_2d=True)
            np.testing.assert_array_equal(x[:,0],x[:,1])

    def test_identity_rate_and_no_boost(self):
        arrays=self.fixture(rate=44100,amplitude=.1)
        target=prepare(self.source,self.dest)
        p=json.loads((target/"preparation.json").read_text())
        self.assertEqual(p["common_gain"],1)
        self.assertEqual(validate(target)["frames"],len(arrays[0]))

    def test_impulse_alignment(self):
        x=np.zeros((4800,1));x[1600]=.2
        self.fixture(channels=1,data=x)
        target=prepare(self.source,self.dest)
        maxima=[int(np.argmax(np.abs(sf.read(target/(r.lower()+".wav"))[0][:,0]))) for r in ROLES]
        self.assertEqual(maxima,[1470]*4)

    def test_registry_reuse_and_no_overwrite(self):
        self.fixture()
        target=prepare(self.source,self.dest)
        before={p.name:p.read_bytes() for p in target.iterdir()}
        self.assertEqual(prepare(self.source,self.dest),target)
        self.assertEqual(before,{p.name:p.read_bytes() for p in target.iterdir()})
        self.mutate(lambda m:m.update(key="b"*64))
        next_target=prepare(self.source,self.dest)
        self.assertNotEqual(validate(next_target)["set_id"],validate(target)["set_id"])

    def test_deterministic_across_registries(self):
        self.fixture()
        a=prepare(self.source,self.dest)
        b=prepare(self.source,self.root/"second")
        for role in ROLES:
            file=role.lower()+".wav"
            self.assertEqual((a/file).read_bytes(),(b/file).read_bytes())

    def test_missing_native(self):
        self.fixture();(self.source/"bass.wav").unlink()
        with self.assertRaises(ValueError):prepare(self.source,self.dest)
        self.no_publication()

    def test_corrupt_native_hash(self):
        self.fixture()
        with (self.source/"bass.wav").open("ab") as f:f.write(b"bad")
        with self.assertRaises(ValueError):prepare(self.source,self.dest)
        self.no_publication()

    def test_missing_hash_and_wrong_roles(self):
        self.fixture()
        self.mutate(lambda m:m["stems"][0].pop("sha256"))
        with self.assertRaises(ValueError):prepare(self.source,self.dest)
        self.fixture()
        self.mutate(lambda m:m["stems"].reverse())
        with self.assertRaises(ValueError):prepare(self.source,self.dest)
        self.no_publication()

    def test_nonfinite_audio(self):
        x=np.zeros((100,1));x[3]=np.nan
        self.fixture(channels=1,data=x)
        with self.assertRaises(ValueError):prepare(self.source,self.dest)
        self.no_publication()

    def test_false_int_and_wrong_channels(self):
        self.fixture()
        self.mutate(lambda m:m.update(frames=True))
        with self.assertRaises(ValueError):prepare(self.source,self.dest)
        self.fixture()
        self.mutate(lambda m:m.update(channels=3))
        with self.assertRaises(ValueError):prepare(self.source,self.dest)

    def test_symlink_and_traversal(self):
        self.fixture()
        original=self.source/"bass.wav";outside=self.root/"outside.wav"
        original.rename(outside);original.symlink_to(outside)
        with self.assertRaises(ValueError):prepare(self.source,self.dest)
        original.unlink();outside.rename(original)
        self.mutate(lambda m:m["stems"][0].update(file="../outside.wav"))
        with self.assertRaises(ValueError):prepare(self.source,self.dest)

    def test_truncated_matching_hash(self):
        self.fixture()
        f=self.source/"bass.wav";f.write_bytes(f.read_bytes()[:-12])
        self.mutate(lambda m:m["stems"][2].update(sha256=hashlib.sha256(f.read_bytes()).hexdigest()))
        with self.assertRaises(ValueError):prepare(self.source,self.dest)
        self.no_publication()

    def test_pcm_input_rejected(self):
        self.fixture()
        f=self.source/"bass.wav";x,_=sf.read(f);sf.write(f,x,48000,subtype="PCM_16")
        self.mutate(lambda m:m["stems"][2].update(sha256=hashlib.sha256(f.read_bytes()).hexdigest()))
        with self.assertRaises(ValueError):prepare(self.source,self.dest)

    def test_corrupt_existing_prepared_rejected(self):
        self.fixture();target=prepare(self.source,self.dest)
        f=target/"vocals.wav";f.write_bytes(f.read_bytes()[:-10])
        with self.assertRaises(ValueError):prepare(self.source,self.dest)

    def test_failure_cleanup(self):
        self.fixture()
        with patch("prepare.os.rename",side_effect=OSError("simulated publish failure")):
            with self.assertRaises(OSError):prepare(self.source,self.dest)
        self.no_publication()
        self.assertEqual(validate(prepare(self.source,self.dest))["set_id"],1)

    def test_output_cannot_modify_native(self):
        self.fixture()
        with self.assertRaises(ValueError):prepare(self.source,self.source/"prepared")

    def test_concurrent_registry(self):
        import subprocess, sys
        self.fixture()
        command=[sys.executable,"-B",str(Path(__file__).with_name("prepare.py")),str(self.source),str(self.dest)]
        processes=[subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) for _ in range(2)]
        outputs=[p.communicate(timeout=30) for p in processes]
        self.assertEqual([p.returncode for p in processes],[0,0],outputs)
        self.assertEqual(outputs[0][0],outputs[1][0])
        self.assertEqual(len([p for p in self.dest.glob("set-*") if p.is_dir()]),1)

    def test_zero_stems(self):
        self.fixture(channels=1,data=np.zeros((100,1)))
        target=prepare(self.source,self.dest)
        self.assertEqual(json.loads((target/"preparation.json").read_text())["common_gain"],1)
        self.assertEqual(validate(target)["frames"],92)


if __name__=="__main__":
    unittest.main()
