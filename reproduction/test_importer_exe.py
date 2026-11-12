import subprocess,tempfile,wave,struct,sys
from pathlib import Path
base=Path(__file__).resolve().parent.parent;sdk=base;project=sdk/'plugins'/'Prism Wave';exe=project/'import_wavetable.exe'
with tempfile.TemporaryDirectory(dir=base)as temp:
 folder=Path(temp);(folder/'plugin.json').write_text('{"name":"Prism Wave"}')
 dest=folder/'Content'/'Wavetables'/'user02.wav'
 cmd=[str(exe),str(project/'Content'/'Wavetables'/'user01.wav'),'--slot','2','--frame-size','2048','--plugin-folder',str(folder)]
 p=subprocess.run(cmd,capture_output=True,text=True,check=True);print(p.stdout.strip())
 with wave.open(str(dest),'rb')as w:
  assert (w.getnchannels(),w.getsampwidth(),w.getnframes())==(1,2,16*2048)
  assert len(w.readframes(w.getnframes()))==16*2048*2
 print('PASS standalone importer writes a valid multiframe slot without Python')
 before=dest.read_bytes();p=subprocess.run(cmd,capture_output=True,text=True)
 assert p.returncode==1 and before==dest.read_bytes();print('PASS occupied slot is preserved unless --replace is supplied')
 subprocess.run([*cmd,'--replace'],capture_output=True,check=True);print('PASS explicit replacement succeeds')
 invalid=folder/'nan.wav';pcm=struct.pack('<f',float('nan'))*2048;fmt=struct.pack('<HHIIHH',3,1,44100,176400,4,32)
 invalid.write_bytes(b'RIFF'+struct.pack('<I',36+len(pcm))+b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(pcm))+pcm)
 p=subprocess.run([str(exe),str(invalid),'--slot','3','--plugin-folder',str(folder)],capture_output=True,text=True)
 assert p.returncode==1 and not (dest.parent/'user03.wav').exists();print('PASS nonfinite imported audio is rejected before writing')
