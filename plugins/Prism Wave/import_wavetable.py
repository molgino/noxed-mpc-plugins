#!/usr/bin/env python3
"""Convert a WAV wavetable to a Prism Wave user slot (stdlib only).

python import_wavetable.py input.wav --slot 1 --frame-size 2048 --plugin-folder ".../NOXED - VST - Prism Wave"
Input: PCM 8/16/24/32 or float32 WAV, mono/stereo; frames are individual cycles.
Output: mono 16-bit, 2048 samples/cycle, <=64 frames. No proprietary preset import.
"""
import argparse,math,struct,wave,json
from pathlib import Path

def read_wav(path):
 data=Path(path).read_bytes()
 if data[:4]!=b'RIFF' or data[8:12]!=b'WAVE':raise ValueError('Expected a RIFF/WAVE file')
 pos=12;fmt=None;pcm=None
 while pos+8<=len(data):
  tag=data[pos:pos+4];size=struct.unpack_from('<I',data,pos+4)[0];body=data[pos+8:pos+8+size]
  if len(body)!=size:raise ValueError('Truncated WAV chunk')
  if tag==b'fmt ':fmt=body
  if tag==b'data':pcm=body
  pos+=8+size+(size&1)
 if fmt is None or pcm is None or len(fmt)<16:raise ValueError('Missing WAV format/audio data')
 kind,channels,rate,_,align,bits=struct.unpack_from('<HHIIHH',fmt)
 if kind==65534 and len(fmt)>=40:kind=struct.unpack_from('<H',fmt,24)[0]
 if kind not in (1,3) or channels not in (1,2) or bits not in (8,16,24,32) or (kind==3 and bits!=32):raise ValueError('Use mono/stereo PCM WAV or float32 WAV')
 width=bits//8
 if align!=channels*width or len(pcm)%align:raise ValueError('Invalid WAV block alignment')
 values=[]
 for i in range(0,len(pcm),align):
  total=0
  for ch in range(channels):
   b=pcm[i+ch*width:i+(ch+1)*width]
   x=struct.unpack('<f',b)[0] if kind==3 else ((b[0]-128)/128 if bits==8 else int.from_bytes(b,'little',signed=True)/(2**(bits-1)))
   if not math.isfinite(x):raise ValueError('WAV contains NaN/Infinity')
   total+=x
  values.append(total/channels)
 return values

def convert(samples,frame_size):
 if frame_size<32 or frame_size>16384 or len(samples)%frame_size:raise ValueError('Sample count must be an exact multiple of --frame-size (32..16384)')
 frames=len(samples)//frame_size
 if not 1<=frames<=1024:raise ValueError('Expected 1..1024 input frames')
 count=min(frames,64);out=[]
 for f in range(count):
  source_frame=round(f*(frames-1)/max(1,count-1));cycle=samples[source_frame*frame_size:(source_frame+1)*frame_size]
  mean=sum(cycle)/frame_size;cycle=[x-mean for x in cycle];peak=max(abs(x) for x in cycle)
  if peak<1e-8:raise ValueError(f'Frame {source_frame} is silent after DC removal')
  for i in range(2048):
   p=i*frame_size/2048;j=int(p);t=p-j
   out.append((cycle[j]+t*(cycle[(j+1)%frame_size]-cycle[j]))/peak*.85)
 return out,frames,count

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('input',type=Path);ap.add_argument('--slot',type=int,choices=range(1,9),required=True)
 ap.add_argument('--frame-size',type=int,default=2048);ap.add_argument('--plugin-folder',type=Path,default=Path(__file__).resolve().parent)
 ap.add_argument('--replace',action='store_true',help='Explicitly replace an existing slot WAV')
 args=ap.parse_args()
 try:
  values,original,count=convert(read_wav(args.input),args.frame_size)
  dest=args.plugin_folder/'Content'/'Wavetables'/f'user{args.slot:02d}.wav'
  if dest.exists() and not args.replace:raise ValueError(f'{dest} exists; pass --replace to overwrite')
  dest.parent.mkdir(parents=True,exist_ok=True)
  with wave.open(str(dest),'wb')as w:
   w.setnchannels(1);w.setsampwidth(2);w.setframerate(44100);w.writeframes(b''.join(struct.pack('<h',round(max(-1,min(1,x))*32767)) for x in values))
  dest.with_suffix('.json').write_text(json.dumps({'source':args.input.name,'original_frames':original,'frames':count,'frame_size':2048},indent=2))
  print(f'Imported {original} -> {count} frames into User {args.slot:02d}: {dest}')
  print('If importing into the source project, rebuild/copy Content. Reopen all plugin instances on the MPC after copying.')
 except (ValueError,OSError,struct.error)as e:ap.exit(1,f'Import failed: {e}\n')
if __name__=='__main__':main()
