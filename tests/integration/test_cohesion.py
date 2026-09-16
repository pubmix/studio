"""Cross-workstream integration tests; no model downloads or real music."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

import numpy as np
import soundfile as sf
from studio_stem_engine import Engine, STEMS
from prepare import prepare, validate

ROOT=Path(__file__).resolve().parents[2]
BASE=json.loads((ROOT/"packages/contracts/system-v1/baseline.json").read_text())
SCHEMA=json.loads((ROOT/"packages/contracts/stem-set/v1/manifest.schema.json").read_text())
spec=importlib.util.spec_from_file_location("firmware_validator",ROOT/"firmware/teensy/tools/validate_prepared.py")
fw=importlib.util.module_from_spec(spec);spec.loader.exec_module(fw)


class SyntheticBackend:
    def identity(self, settings):
        return {"fixture":"cohesion-v1"}
    def separate(self, audio, rate, model, settings, scratch, progress):
        return {"vocals":audio*.1,"other":audio*.4,"bass":audio*.2,"drums":audio*.3}


class CohesionTests(unittest.TestCase):
    def test_real_publisher_to_cpp_playback(self):
        for channels in (1,2):
            with self.subTest(channels=channels), tempfile.TemporaryDirectory() as t:
                root=Path(t)
                signal=.8*np.sin(2*np.pi*330*np.arange(4801)/48000)
                x=signal[:,None] if channels==1 else np.column_stack((signal,signal*.6))
                sf.write(root/"song.wav",x,48000,subtype="FLOAT")
                native=Engine(root/"native",backends={"demucs":SyntheticBackend()}).separate(root/"song.wav")
                metadata=Engine.verify(native)
                self.assertEqual(metadata["schema"],BASE["native"]["schema"])
                self.assertEqual([s["name"] for s in metadata["stems"]],BASE["roles"])
                before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in native.iterdir()}
                ready=prepare(native,root/"prepared")
                m=validate(ready)
                self.assertEqual(m["sample_rate"],BASE["prepared"]["sample_rate"])
                self.assertEqual(m["frames"],4411)
                self.assertEqual([s["role"] for s in m["stems"]],BASE["roles"])
                self.assertEqual(fw.validate(ready)["stage"],"ready")
                self.assertEqual(before,{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in native.iterdir()})
                # Host demo validates WAVs but uses demo IDs. Catalog preservation is an explicit open gate.
                run=subprocess.run([str(ROOT/"firmware/teensy/build/studio_sim"),str(ready)],
                                   input="play\nrender 0.05\nquit\n",text=True,capture_output=True,cwd=root,check=True)
                energy=re.search(r"energy=([0-9.eE+-]+)",run.stdout)
                self.assertIsNotNone(energy,run.stdout)
                self.assertGreater(float(energy[1]),0)
                self.assertIn("frames=2205",run.stdout)
                with self.assertRaises(OSError):
                    fw.validate(native) # Metadata/schema is deliberately not firmware media.
                with (ready/"bass.wav").open("ab") as f:f.write(b"corrupt")
                with self.assertRaises(ValueError):fw.validate(ready)

    def test_roles_and_media_across_contracts(self):
        self.assertEqual([s.upper() for s in STEMS],BASE["roles"])
        self.assertEqual(list(fw.ROLES),BASE["roles"])
        items=SCHEMA["properties"]["stems"]["prefixItems"]
        self.assertEqual([s["properties"]["role"]["const"] for s in items],BASE["roles"])
        self.assertEqual(SCHEMA["properties"]["set_id"]["maximum"],BASE["prepared"]["set_id_max"])
        model=(ROOT/"firmware/teensy/firmware/STUDIO/src/studio/Model.h").read_text()
        for key,value in [("Lanes",4),("SampleRate",44100),("BlockSize",128)]:
            self.assertRegex(model,rf"\b{key}={value}\b")
        names=re.search(r"StemNames\[\]=\{([^}]+)",model)[1]
        self.assertEqual(re.findall(r'"([^"]+)"',names),BASE["roles"])
        p=BASE["prepared"]
        self.assertEqual(4*p["channels"]*p["sample_rate"]*(p["bits"]//8),p["stem_bytes_per_second"])

    def test_hardware_pin_inventory(self):
        pins=list(BASE["pins"].values());reserved=BASE["reserved_edge_pins"]
        self.assertEqual(len(pins),35)
        self.assertEqual(len(set(pins)),len(pins))
        self.assertFalse(set(pins)&set(reserved))
        self.assertEqual(set(pins+reserved),set(range(42)))
        self.assertEqual([BASE["pins"][x] for x in ("main_dac","hp_dac","adc")],[7,32,8])
        self.assertEqual(BASE["lane_order"],["TOP_BUTTON","PUSH_ENCODER","FADER","BOTTOM_BUTTON"])
        doc=(ROOT/"hardware/architecture-v1/03-PINS-AND-RESOURCES.md").read_text()
        found=set()
        for cell in re.findall(r"^\| ([0-9][^|]*) \|",doc,re.M):
            for token in cell.strip().split(" / "):
                if "–" in token:
                    a,b=map(int,token.split("–"));found.update(x for x in range(a,b+1) if x<42)
                elif token.isdigit() and int(token)<42:found.add(int(token))
        self.assertEqual(found,set(range(42)))
        board=(ROOT/"firmware/teensy/firmware/STUDIO/BoardConfig.h").read_text()
        self.assertIn("#define STUDIO_ENABLE_I2S 0",board)
        self.assertIn("#define STUDIO_ENABLE_SD 0",board)
        self.assertEqual(BASE["hardware_status"],"recommended_unbuilt")

    def test_baseline_links_exist(self):
        for path in ("docs/SYSTEM_ARCHITECTURE.md","services/preparation-adapter/README.md",
                     "packages/contracts/system-v1/README.md"):
            p=ROOT/path
            for target in re.findall(r"\]\(([^)]+)\)",p.read_text()):
                if "://" not in target:
                    self.assertTrue((p.parent/target.split("#")[0]).exists(),(path,target))


if __name__=="__main__":
    unittest.main()
