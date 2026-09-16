import importlib.util,json,pathlib,struct,tempfile,unittest,wave
p=pathlib.Path(__file__).parents[1]/'tools'/'validate_prepared.py'
spec=importlib.util.spec_from_file_location('validator',p);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class ImportTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.temp.name)
  self.manifest={'api_version':1,'set_id':1,'sample_rate':44100,'frames':32,'alignment_offset_frames':0,'stems':[]}
  for role in module.ROLES:
   name=role.lower()+'.wav';self.manifest['stems'].append({'role':role,'file':name})
   with wave.open(str(self.root/name),'wb') as w:w.setparams((2,2,44100,0,'NONE',''));w.writeframes(struct.pack('<hh',100,-100)*32)
  self.save()
 def tearDown(self):self.temp.cleanup()
 def save(self):(self.root/'manifest.json').write_text(json.dumps(self.manifest))
 def test_valid(self):self.assertEqual(module.validate(self.root)['stage'],'ready')
 def test_order(self):self.manifest['stems'].reverse();self.save();self.assertRaises(ValueError,module.validate,self.root)
 def test_missing(self):(self.root/'bass.wav').unlink();self.assertRaises(OSError,module.validate,self.root)
 def test_alignment(self):self.manifest['alignment_offset_frames']=1;self.save();self.assertRaises(ValueError,module.validate,self.root)
 def test_rate(self):self.manifest['sample_rate']=48000;self.save();self.assertRaises(ValueError,module.validate,self.root)
 def test_count(self):self.manifest['frames']=33;self.save();self.assertRaises(ValueError,module.validate,self.root)
 def test_hash(self):self.manifest['stems'][0]['sha256']='wrong';self.save();self.assertRaises(ValueError,module.validate,self.root)
 def test_uint32_id(self):
  for value in (True,0,-1,4294967296):
   self.manifest['set_id']=value;self.save();self.assertRaises(ValueError,module.validate,self.root)
  self.manifest['set_id']=4294967295;self.save();self.assertEqual(module.validate(self.root)['set_id'],4294967295)
 def test_json_boolean_constants(self):
  for field in ('api_version','frames','alignment_offset_frames'):
   old=self.manifest[field];self.manifest[field]=bool(old);self.save();self.assertRaises(ValueError,module.validate,self.root);self.manifest[field]=old
 def test_present_invalid_hash(self):
  for value in ('',None,False):
   self.manifest['stems'][0]['sha256']=value;self.save();self.assertRaises(ValueError,module.validate,self.root)
 def test_truncated(self):p=self.root/'bass.wav';p.write_bytes(p.read_bytes()[:-8]);self.assertRaises(ValueError,module.validate,self.root)
if __name__=='__main__':unittest.main()
