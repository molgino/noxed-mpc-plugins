"""Original silver-panel art and TB-style pitch/time input controls; no Roland artwork."""
import gen_ui as ui
from PIL import Image,ImageDraw

def build(a,params):
    E=dict(mode=84,cursor=85,octave=86,style=87,count=88,first=105,back=118,tap=119,note=120,tie=121,rest=122,clear=123,accent=124,slide=125,display=126)
    defs=[];kids=[];repl=[]
    im=Image.new('RGB',(a.W*a.S,a.H*a.S),'#cacbc3');d=ImageDraw.Draw(im)
    a.box(d,(8,8,1271,619),'#b3b5ac','#62665b',2,14)
    a.box(d,(16,16,1263,610),'#d5d5cc','#f1efe3',1,10)
    a.text(d,(54,46),'NOXED',30,True,anchor='lm');a.text(d,(250,48),'Bass Line',29,anchor='lm')
    a.text(d,(1214,47),'ACIDLINE',30,anchor='rm')
    a.box(d,(54,78,1218,83),a.ORANGE)
    a.text(d,(54,102),'PATTERN WRITE',13,True,anchor='lm')
    a.text(d,(1218,102),'PITCH → TIME → NORMAL',13,True,anchor='rm')
    def button(key,caption,param,x,y,w=140,h=48,idx=1,n=1,dark=False):
        for on in (False,True):
            asset=Image.new('RGBA',(w*a.S,h*a.S));dr=ImageDraw.Draw(asset)
            face=('#33372f' if dark else '#e8e1cb')
            a.box(dr,(2,4,w-2,h-1),'#73766a',radius=4)
            a.box(dr,(2,2,w-2,h-5),face,'#676c60',1,4)
            a.line(dr,[(6,5),(w-7,5)],'#656a5d' if dark else '#faf7e9',1)
            if n>1 or key=='enable':a.box(dr,(10,10,23,15),a.ORANGE if on else '#8e8876',radius=2)
            a.text(dr,(w/2,h/2+3),caption,17 if w>=90 else 13,True,'#efeddf' if dark else a.INK)
            asset.resize((w,h),Image.Resampling.LANCZOS).save(a.SKINS/f'{key}-{on}.png')
        name='303-'+key
        defs.append((name,ui.component_definition([ui.action('Mouse Down','Q-Link','Data','')],[
          a.entry('button','Button',ui.button_data(f'{key}-True.png',f'{key}-False.png',idx,n),f'0 0 {w} {h}')],'0')))
        kids.append(a.entry(name,name,ui.obj(('version',1),('handleName','Data')),f'{x} {y} {w} {h}',[('Data',f'Parameter {param}')],focus='Yes'))
    for i,(label,x) in enumerate([('NORMAL',54),('PITCH MODE',230),('TIME MODE',406)]):button('mode%d'%i,label,84,x,120,162,50,i,3)
    button('enable','SEQ ON / OFF',16,1014,120,204,50)
    # Small input-position lamps; they indicate the write cursor, not a fake playhead.
    a.text(d,(54,196),'WRITE POSITION',11,True,anchor='lm')
    for i in range(16):button('pos%d'%i,str(i+1),85,54+i*45,212,38,34,i,16)
    a.box(d,(54,262,770,304),'#bdc0b2','#92998b',1,3)
    kids.append(a.entry('readout','Label',ui.label_data('Value','ff343b2d',23,'left verticallyCentred'),'70 264 684 36',[('Data','Parameter 126')]))
    a.text(d,(820,196),'TRANSPOSE',12,True,anchor='lm')
    for i,(label,x) in enumerate([('DOWN',808),('NORMAL',946),('UP',1084)]):button('oct%d'%i,label,86,x,212,124,48,i,3)
    button('accent','ACCENT',124,808,274,194,48)
    button('slide','SLIDE',125,1022,274,194,48)
    # Thirteen chromatic keys, in the 303's familiar eight-white/five-black arrangement.
    white=[0,2,4,5,7,9,11,12];names=['C','D','E','F','G','A','B','C']
    key_x,key_y,key_w=54,358,145
    a.text(d,(54,340),'PITCH MODE',12,True,anchor='lm')
    for i,(note,label) in enumerate(zip(white,names)):
        button('key%d'%note,label,105+note,key_x+i*key_w,key_y,140,150)
    for note,left in [(1,0),(3,1),(6,3),(8,4),(10,5)]:
        button('key%d'%note,['C#','D#','F#','G#','A#'][(1,3,6,8,10).index(note)],105+note,key_x+(left+1)*key_w-43,key_y,80,88,dark=True)
    a.text(d,(54,538),'TIME MODE',12,True,anchor='lm')
    for key,label,param,x in [('note','NOTE',120,54),('tie','TIE',121,222),('rest','REST',122,390),('back','BACK',118,666),('tap','TAP / NEXT',119,834),('clear','CLEAR',123,1054)]:
        button(key,label,param,x,552,150 if key!='clear' else 164,44)
    filename='panel-sequencer.png';im.resize((a.W,a.H),Image.Resampling.LANCZOS).save(a.SKINS/filename)
    bg=a.entry('panel','Image',ui.obj(('version',2),('imageType','Regular'),('colour','0'),('image',filename)),'0 0 1280 628')
    tab=ui.obj(('version',1),('tabName','SEQUENCE'),('fnKeyIndex',2),('fnKeySubIndex',0),('componentDefinition',ui.component_definition([],[bg]+kids,'ffcecec6')),('qlinkBoundsData',['48 116 532 58','802 204 416 122','48 546 500 56','660 546 560 56']))
    mapping={13:84,14:16,15:85,16:86,9:17,10:18,11:83,12:88}
    qmap=ui.obj(('Tab',3),('SubTab',1),('Bank Direction','RowTopDown'),('Q-Links',ui.Obj((f'Q-Link {q}',mapping.get(q,-1)) for q in range(1,17))))
    repl=[ui.obj(('key','Parameter 84'),('value',['Normal','Pitch Mode','Time Mode'])),ui.obj(('key','Parameter 85'),('value',[str(i+1)for i in range(16)])),ui.obj(('key','Parameter 86'),('value',['Down','Normal','Up']))]
    repl += [ui.obj(('key',f'Parameter {i}'),('value',['Off','On'])) for i in range(105,126)]
    return tab,defs,qmap,repl
