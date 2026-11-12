import importlib.util,math,struct,tempfile,unittest,wave
from pathlib import Path
root=Path(__file__).resolve().parent.parent/'plugins'/'Prism Wave'
spec=importlib.util.spec_from_file_location('imp',root/'import_wavetable.py');imp=importlib.util.module_from_spec(spec);spec.loader.exec_module(imp)
class ImportTests(unittest.TestCase):
 def test_cycle_resampling(self):
  values,n,f=imp.convert([math.sin(i*2*math.pi/512)for i in range(512*3)],512)
  self.assertEqual((len(values),n,f),(6144,3,3));self.assertAlmostEqual(max(values),.85,places=5)
 def test_frame_reduction(self):
  values,n,f=imp.convert([math.sin(i*2*math.pi/32)for i in range(32*256)],32)
  self.assertEqual((n,f,len(values)),(256,64,2048*64))
 def test_silent_invalid_and_non_wave(self):
  for samples,size in [([0]*2048,2048),([1]*100,2048),([0]*32,0)]:
   with self.assertRaises(ValueError):imp.convert(samples,size)
  with tempfile.TemporaryDirectory(dir=Path(__file__).parent)as t:
   p=Path(t)/'bad.wav';p.write_bytes(b'bad');
   with self.assertRaises(ValueError):imp.read_wav(p)
 def test_stereo_pcm_and_float(self):
  with tempfile.TemporaryDirectory(dir=Path(__file__).parent)as t:
   p=Path(t)/'stereo.wav'
   with wave.open(str(p),'wb')as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(44100);w.writeframes(struct.pack('<hhhh',16384,0,-16384,0))
   self.assertEqual(imp.read_wav(p),[.25,-.25])
   fmt=struct.pack('<HHIIHH',3,1,44100,176400,4,32);pcm=struct.pack('<ff',.25,-.5)
   p.write_bytes(b'RIFF'+struct.pack('<I',36+len(pcm))+b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(pcm))+pcm)
   self.assertEqual(imp.read_wav(p),[.25,-.5])
if __name__=='__main__':unittest.main()
