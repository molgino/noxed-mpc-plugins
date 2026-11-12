#!/usr/bin/env python3
"""Generate Acidline's original silver hardware skin, plus previews from its TUI.

Requires Pillow only to regenerate artwork. The normal SDK build copies skins/
verbatim and does not require Pillow. No manufacturer artwork is used.
python tools/make_acidline_skin.py --params plugins/Acidline/build/params.json
"""
import argparse, json, math, random
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import gen_ui as ui

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'plugins' / 'Acidline'
SKINS = PROJECT / 'skins'
S = 2
INK = '#282827'
ORANGE = '#cc542e'
W, H = 1280, 628
MAIN = [(9,'TUNING',130,218),(1,'CUT OFF FREQ',334,218),
        (2,'RESONANCE',538,218),(3,'ENV MOD',742,218),
        (4,'DECAY',946,218),(5,'ACCENT',1150,218),
        (0,'VOLUME',130,461),(7,'DRIVE',334,461),
        (6,'SLIDE TIME',538,461),(13,'RELEASE',742,461)]
SETUP = [(10,'PULSE WIDTH',244,302),(11,'ACCENT VELOCITY',508,302),
         (14,'KEY TRACK',772,302),(15,'BEND RANGE',1036,302)]

def font(size, bold=False):
    candidates = [Path('C:/Windows/Fonts') / ('arialbd.ttf' if bold else 'arial.ttf'),
                  Path('/usr/share/fonts/truetype/liberation2') / ('LiberationSans-Bold.ttf' if bold else 'LiberationSans-Regular.ttf')]
    for p in candidates:
        if p.exists(): return ImageFont.truetype(str(p), round(size*S))
    return ImageFont.truetype('DejaVuSans.ttf', round(size*S))

def box(d, bounds, fill, outline=None, width=1, radius=0):
    xy = tuple(round(v*S) for v in bounds)
    if radius: d.rounded_rectangle(xy, radius=round(radius*S), fill=fill, outline=outline, width=round(width*S))
    else: d.rectangle(xy, fill=fill, outline=outline, width=round(width*S))

def text(d, xy, message, size=18, bold=False, colour=INK, anchor='mm'):
    d.text(tuple(round(v*S) for v in xy), message, font=font(size,bold), fill=colour, anchor=anchor)

def line(d, points, colour, width=1):
    d.line([(round(x*S),round(y*S)) for x,y in points], fill=colour, width=round(width*S))

def panel(page):
    # Subtle horizontal machining marks, deterministic and entirely original.
    rng = random.Random(303)
    image = Image.new('RGB',(W*S,H*S))
    d = ImageDraw.Draw(image)
    for y in range(H*S):
        light = int(196 + 15*math.cos((y/(H*S)-.28)*math.pi) + rng.uniform(-1.4,1.4))
        d.line((0,y,W*S,y),fill=(light+3,light+3,light))
    box(d,(8,8,1271,619),'#b0b1ad', '#545752',2,16)
    box(d,(15,14,1264,609),'#d4d4cd','#eeeee7',1,12)
    # The inner face is a softly brushed alloy, not a flat software-card layout.
    for y in range(20,604):
        c=int(202+12*math.cos((y/628-.22)*math.pi)+rng.uniform(-1.2,1.2))
        line(d,[(23,y),(1256,y)],(c+3,c+3,c),1)
    for x,y in [(36,33),(1243,33),(36,590),(1243,590)]:
        d.ellipse(tuple(round(v*S) for v in (x-7,y-7,x+7,y+7)),fill='#898b86',outline='#eff0e9',width=S)
        line(d,[(x-4,y+2),(x+4,y-2)],'#41443f',1)
    text(d,(62,48),'NOXED',34,True,anchor='lm')
    text(d,(256,48),'Bass Line',31,False,anchor='lm')
    text(d,(1218,49),'ACIDLINE',40,True,anchor='rm')
    box(d,(61,85,1218,91),ORANGE)
    if page==0:
        text(d,(61,111),'TONE CONTROL',12,True,anchor='lm')
        for p,label,x,y in MAIN:
            text(d,(x,y-82),label,17,True)
            ticks(d,x,y,66)
            # A recessed readout provides the MPC parameter's current value.
            box(d,(x-65,y+76,x+65,y+104),'#c3c3b8','#a5a69c',1,3)
        box(d,(61,344,1218,346),'#8f9188')
        line(d,[(61,347),(1218,347)],'#efefe6')
        text(d,(61,361),'PERFORMANCE CONTROL',12,True,anchor='lm')
        text(d,(1046,374),'WAVEFORM',17,True)
        text(d,(1046,490),'LEGATO / SLIDE',17,True)
    else:
        text(d,(62,120),'EXTENDED CONTROL',16,True,anchor='lm')
        for p,label,x,y in SETUP:
            text(d,(x,y-89),label,19,True)
            ticks(d,x,y,74)
            box(d,(x-65,y+76,x+65,y+104),'#c3c3b8','#a5a69c',1,3)
        box(d,(102,478,1178,545),'#bebfb6','#999d91',1,5)
        text(d,(640,501),'ACCENT VELOCITY sets the minimum MIDI velocity for accented notes.',18)
        text(d,(640,526),'Return to BASSLINE for the six classic tone controls.',16)
    box(d,(61,601,1218,602),'#92958b')
    text(d,(62,582 if page==0 else 578),'COMPUTER CONTROLLED',13,True,anchor='lm')
    text(d,(1218,582 if page==0 else 578),'MONOPHONIC BASS SYNTHESIZER',13,anchor='rm')
    image=image.resize((W,H),Image.Resampling.LANCZOS)
    image.save(SKINS/('panel-main.png' if page==0 else 'panel-setup.png'))

def ticks(d,x,y,r):
    for t in range(11):
        a=math.radians(-225+t*27)
        r2=r+(8 if t in (0,5,10) else 5)
        line(d,[(x+math.cos(a)*r,y+math.sin(a)*r),(x+math.cos(a)*r2,y+math.sin(a)*r2)],'#44463f',1.4)

def knob_frame(value,size):
    im=Image.new('RGBA',(size*S,size*S))
    d=ImageDraw.Draw(im);c=size/2;r=size*.395
    # Offset shadow, steel rim, ribbed black grip and a flat charcoal cap.
    d.ellipse(tuple(round(v*S)for v in (c-r-3,c-r+3,c+r+4,c+r+10)),fill=(0,0,0,62))
    for inset,colour in [(0,'#676962'),(2,'#121310'),(5,'#393b36'),(9,'#1b1d19')]:
        d.ellipse(tuple(round(v*S)for v in (c-r+inset,c-r+inset,c+r-inset,c+r-inset)),fill=colour)
    for t in range(44):
        a=2*math.pi*t/44
        line(d,[(c+math.cos(a)*(r-4),c+math.sin(a)*(r-4)),(c+math.cos(a)*(r-8),c+math.sin(a)*(r-8))],'#575951'if t%2==0 else'#171915',1.2)
    d.arc(tuple(round(v*S)for v in (c-r+9,c-r+9,c+r-9,c+r-9)),190,295,fill='#606259',width=S)
    a=math.radians(-225+270*value)
    line(d,[(c+math.cos(a)*r*.22,c+math.sin(a)*r*.22),(c+math.cos(a)*r*.76,c+math.sin(a)*r*.76)],'#eeeee2',3.5)
    return im.resize((size,size),Image.Resampling.LANCZOS)

def make_art():
    SKINS.mkdir(parents=True,exist_ok=True)
    for page in (0,1):panel(page)
    strip=Image.new('RGBA',(112,112*127))
    for f in range(127):strip.paste(knob_frame(f/126,112),(0,f*112))
    strip.save(SKINS/'303-knob.png')
    for mode in ('wave','slide'):
        width,height=(164,68) if mode=='wave' else (344,52)
        for state in (False,True):
            im=Image.new('RGBA',(width*S,height*S));d=ImageDraw.Draw(im)
            box(d,(2,4,width-2,height-1),'#72756b',radius=6)
            box(d,(3,2,width-3,height-6),'#d8d6c5'if not state else'#ded6bd','#73776a',1,5)
            line(d,[(7,5),(width-8,5)],'#fcf9e6',2)
            box(d,(13,12,24,19),ORANGE if state else '#8a7761',radius=2)
            im.resize((width,height),Image.Resampling.LANCZOS).save(SKINS/f'{mode}-{"on"if state else"off"}.png')

def entry(name,typ,data,bounds,remap=(),focus='No'):
    return ui.entry(name,typ,data,bounds,focus,'Show','Always',remap)

def control_definition(param,size=112):
    # Children inherit the parameter through Data. Only the outer control focuses.
    children=[entry('knob','Knob',ui.knob_filmstrip_data('303-knob.png',127),'14 8 112 112'),
              entry('value','Label',ui.label_data('Value','ff373a31',21,'horizontallyCentred verticallyCentred'),'0 140 140 28')]
    focus=ui.entry('focus','Focus',ui.focus_data('ffbd512d'),'0 0 140 174','No','Show','WhenFocussed')
    return ui.component_definition(ui.control_actions('knob overlay bipolar'if param==9 else'knob overlay'),[focus,*children],'0')

def make_tui(params):
    defs=[];tabs=[];maps=[]
    for p in range(16):defs.append((f'control{p}',control_definition(p)))
    for name,data,typ in [('knob overlay',ui.knob_overlay_data(False),'KnobOverlay'),
                          ('knob overlay bipolar',ui.knob_overlay_data(True),'KnobOverlay'),
                          ('menu overlay',ui.menu_overlay_data(),'MenuOverlay')]:
        defs.append((name,ui.overlay_component(typ,data,'ff20221e')))
    for page,positions in enumerate((MAIN,SETUP)):
        children=[entry('panel','Image',ui.obj(('version',2),('imageType','Regular'),('colour','0'),('image','panel-main.png'if page==0 else'panel-setup.png')),'0 0 1280 628')]
        for p,_,x,y in positions:
            children.append(entry(f'parameter{p}',f'control{p}',ui.obj(('version',1),('handleName','Data')),f'{x-70} {y-64} 140 174',[('Data',f'Parameter {p}')],focus='Yes'))
        if page==0:
            # Genuine waveform selectors; no decorative sequencer keys with no function.
            for choice,label,x in [(0,'SAW',874),(1,'SQUARE',1054)]:
                button=ui.component_definition(ui.control_actions('menu overlay',False),[
                    entry('button','Button',ui.button_data('wave-on.png','wave-off.png',choice,2),'0 0 164 68'),
                    entry('caption','Label',ui.label_data('Text','ff30342b',19,'horizontallyCentred verticallyCentred',label),'20 21 132 32')],'0')
                name=f'wave{choice}';defs.append((name,button));children.append(entry(name,name,ui.obj(('version',1),('handleName','Data')),f'{x} 396 164 68',[('Data','Parameter 8')],focus='Yes'))
            toggle=ui.component_definition(ui.control_actions('menu overlay',False),[
                entry('button','Button',ui.button_data('slide-on.png','slide-off.png',1,1),'0 0 344 52'),
                entry('value','Label',ui.label_data('Value','ff30342b',20,'horizontallyCentred verticallyCentred'),'0 0 344 52')],'0')
            defs.append(('slide switch',toggle));children.append(entry('slide','slide switch',ui.obj(('version',1),('handleName','Data')),'874 510 344 52',[('Data','Parameter 12')],focus='Yes'))
        bounds=['48 116 766 220','856 116 370 455','48 375 766 195','0 0 0 0']if page==0 else['130 194 1020 250','0 0 0 0','0 0 0 0','0 0 0 0']
        tabs.append(ui.obj(('version',1),('tabName','BASSLINE'if page==0 else'SETUP'),('fnKeyIndex',page),('fnKeySubIndex',0),('componentDefinition',ui.component_definition([],children,'ffcecec6')),('qlinkBoundsData',bounds)))
        groups=[[9,1,2,3],[4,5,8,12],[0,7,6,13],[-1]*4]if page==0 else[[10,11,14,15],[-1]*4,[-1]*4,[-1]*4]
        assigned={13+col-4*row:groups[col][row]for col in range(4)for row in range(4)}
        maps.append(ui.obj(('Tab',page+1),('SubTab',1),('Bank Direction','Column'),('Q-Links',ui.Obj((f'Q-Link {q}',assigned[q])for q in range(1,17)))))
    data=ui.obj(('pageData',ui.obj(('version',1),('componentDefinitions',ui.obj(('version',2),('importFiles',[]),('localComponentDefinitions',[ui.obj(('key',k),('value',v))for k,v in defs]))),('replacementStrings',[ui.obj(('key','Parameter 8'),('value',['Saw','Square'])),ui.obj(('key','Parameter 12'),('value',['Off','On']))]),('info',ui.obj(('version',1),('type','CompleteDescription'))),('tabs',tabs))))
    qlinks=ui.obj(('version',4),('info',ui.obj(('version',1),('type','CompleteDescription'))),('Screen Mode Q-Links',ui.obj(('version',4),('map',maps))),('Program Mode Q-Links',ui.Obj((f'Q-Link {q+1}',p)for q,p in enumerate([1,2,3,4,5,6,7,0,9,8,12,13,10,11,14,15]))))
    (SKINS/'TUI.json').write_text(ui.dump_json(data)+'\n')
    (SKINS/'Q-Links.json').write_text(ui.dump_json(qlinks)+'\n')

def render_preview(params,output):
    # Render the emitted widget tree, including the actual sprite frames and bindings.
    tui=json.loads((SKINS/'TUI.json').read_text())['pageData']
    definitions={x['key']:x['value']for x in tui['componentDefinitions']['localComponentDefinitions']}
    defaults={p['index']:p for p in params['params']}
    for ti,tab in enumerate(tui['tabs']):
        im=Image.new('RGBA',(W,H),'#cecec6')
        def walk(definition,origin=(0,0),bindings=None):
            bindings=bindings or {}
            for e in definition.get('componentsData',[]):
                if e['bounds']['whenVisible']=='WhenFocussed':continue
                x,y,w,h=map(int,e['bounds']['bounds'].split());x+=origin[0];y+=origin[1]
                binds=dict(bindings)
                for pair in e['handle remapping']['map']:binds[pair['key']]=pair['value']
                comp=e['componentData'];typ=comp['type'];data=comp['data']
                if typ in definitions:walk(definitions[typ],(x,y),binds);continue
                param=binds.get(data.get('handleName','Data'),'');p=defaults.get(int(param.split()[-1]))if param.startswith('Parameter ')else None
                asset=None
                if typ=='Image':asset=Image.open(SKINS/data['image']).convert('RGBA').resize((w,h))
                elif typ=='Knob':
                    strip=Image.open(SKINS/data['filmStrip']);fh=strip.height//data['numFrames'];f=round(p['default']*(data['numFrames']-1));asset=strip.crop((0,f*fh,strip.width,(f+1)*fh))
                elif typ=='Button':
                    value=round(p['default']*(data['numButtonsInGroup']-1))if data['numButtonsInGroup']>1 else round(p['default'])
                    asset=Image.open(SKINS/data['onImage']if value==data['buttonId']else SKINS/data['offImage']).convert('RGBA')
                elif typ=='Label':
                    draw=ImageDraw.Draw(im);value=data.get('text')if data['type']=='Text'else p['display'];f=font(data['textStyle']['font']['height']/S)
                    draw.text((x+w/2,y+h/2),value,font=f,fill='#373a31',anchor='mm')
                if asset is not None:im.alpha_composite(asset,(x,y))
        walk(tab['componentDefinition'])
        im.convert('RGB').save(output/f'Acidline-{tab["tabName"]}-Vorschau.png')

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--params',type=Path,required=True);ap.add_argument('--preview-dir',type=Path,default=PROJECT/'preview');args=ap.parse_args()
    params=json.loads(args.params.read_text());assert len(params['params'])==16
    make_art();make_tui(params);args.preview_dir.mkdir(parents=True,exist_ok=True);render_preview(params,args.preview_dir)
    print('Generated original silver-panel skin: 2 pages, 16 parameters, 127-frame knobs, working waveform/slide switches.')
    print('Artwork and previews:',SKINS,args.preview_dir)
if __name__=='__main__':main()
