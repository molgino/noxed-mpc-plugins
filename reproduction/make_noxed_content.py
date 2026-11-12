from pathlib import Path
import math,struct,wave,importlib.util
root=Path(__file__).resolve().parent.parent/'plugins'/'Prism Wave'
spec=importlib.util.spec_from_file_location('importer',root/'import_wavetable.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
samples=[]
for f in range(16):
 t=f/15
 for i in range(2048):
  p=i/2048
  samples.append(sum(math.sin(2*math.pi*p*h)*math.exp(-abs(h-(1+12*t))*.25)/(h**.6) for h in range(1,25)))
values,_,_=m.convert(samples,2048)
with wave.open(str(root/'Content'/'Wavetables'/'user01.wav'),'wb')as w:
 w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(b''.join(struct.pack('<h',round(x*32767))for x in values))
(root/'Content'/'Wavetables'/'README.txt').write_text('NOXED User 01: original additive 16-frame spectral sweep, MIT licensed.\nUser 02..08 are empty and play Basic Morph until imported.\nUse import_wavetable.py to write user01.wav .. user08.wav here, then reload every plugin instance.\nDo not replace slot content needed by existing projects: projects store slot indices, not audio data.\n')
