"""Draw explicit choice pickers: MPC's native MenuOverlay is blank for VST2.
Hardware evidence: open-mpc docs/SDK-PORTING.md, Key 37 OS 3.10.1.12, 2026-11-15.
No firmware assets used; captions are baked into original PNG buttons.
"""
import math
import gen_ui as ui

def install(pw, pages, params, ui_open=56):
    definitions=[]
    info={p['index']:p for p in params['params']}
    open_count=ui_open+1
    def get(obj,k): return dict(obj)[k]
    def put(obj,k,v):
        for i,(key,_) in enumerate(obj):
            if key==k:obj[i]=(k,v);return
        obj.append((k,v))
    def art(filename,w,h,caption='',lit=False,transparent=False):
        im,d=pw.canvas(w,h,(0,0,0,0))
        if not transparent:
            pw.rrect(d,(1,1,w-1,h-1),'#143746' if lit else '#212632',pw.ACCENT if lit else '#424956',1,4)
            pw.text(d,(16,h/2),caption,18,'semibold',pw.TEXT,'lm')
        pw.finish(im,w,h).save(pw.SKINS/filename)
    for pg in pages:
        popups=[]
        for e in pg.children:
            comp=get(e,'componentData');typ=get(comp,'type')
            if not typ.startswith('pwField'):continue
            mapping=get(get(e,'handle remapping'),'map')
            target=int(next(get(x,'value') for x in mapping if get(x,'key')=='Data').split()[-1])
            choices=info[target].get('choices')
            if not choices:continue
            x,y,w,h=map(int,get(get(e,'bounds'),'bounds').split())
            name='pickField%d_%dx%d'%(target,w,h)
            art('hit-%dx%d.png'%(w,h),w,h,transparent=True)
            kids=[pw.entry('box','Image',ui.obj(('version',2),('imageType','Regular'),('colour','0'),('image','field-%dx%d.png'%(w,h))),'0 0 %d %d'%(w,h)),
                  pw.focus(w,h),pw.value_label('14 0 %d %d'%(w-40,h),20,pw.FIELD_TEXT,'left verticallyCentred'),
                  pw.entry('open','Button',ui.button_data('hit-%dx%d.png'%(w,h),'hit-%dx%d.png'%(w,h),target+1,open_count),'0 0 %d %d'%(w,h))]
            # Override the inherited handle rather than duplicate handleName keys.
            put(get(get(kids[-1],'componentData'),'data'),'handleName','Open')
            definitions.append((name,ui.component_definition([ui.action('Mouse Down','Q-Link','Data','')],kids,'0')))
            put(comp,'type',name);mapping.append(ui.obj(('key','Open'),('value','Parameter %d'%ui_open)))
            rows=min(10,len(choices));cols=math.ceil(len(choices)/rows)
            mw=620;mh=92+rows*44
            panel,d=pw.canvas(mw,mh,pw.hex_rgb('#12161e'))
            pw.rrect(d,(1,1,mw-1,mh-1),'#12161e','#657586',1.5,8)
            pw.text(d,(20,30),info[target]['name'].upper(),21,'bold',pw.TEXT,'lm')
            pw.text(d,(20,mh-16),'CHOOSE A VALUE · X CLOSES',12,'semibold',pw.MUTED,'lm')
            pw.finish(panel,mw,mh).save(pw.SKINS/('picker-%d.png'%target))
            close='picker-close.png';art(close,48,40,'X')
            kids=[pw.entry('panel','Image',ui.obj(('version',2),('imageType','Regular'),('colour','0'),('image','picker-%d.png'%target)),'0 0 %d %d'%(mw,mh)),
                  pw.entry('close','Button',ui.button_data(close,close,0,open_count),'560 10 48 40',[('Data','Parameter %d'%ui_open)])]
            bw=(mw-32)//cols-8
            for i,label in enumerate(choices):
                on,off='choice-%d-%d-on.png'%(target,i),'choice-%d-%d-off.png'%(target,i)
                caption=label.replace(' (Basic fallback)','')
                art(on,bw,38,caption,True);art(off,bw,38,caption)
                r,c=i%rows,i//rows
                kids.append(pw.entry('choice %d'%i,'Button',ui.button_data(on,off,i,len(choices)),'%d %d %d 38'%(16+c*(bw+8),62+r*44,bw)))
            popup='picker%d'%target;definitions.append((popup,ui.component_definition([],kids,'0')))
            popups.append(pw.entry(popup,popup,ui.obj(('version',1),('handleName','Data')),'16 48 %d %d'%(mw,mh),[('Data','Parameter %d'%target)],'No',['IndexedEnabling/%d/%d/Parameter %d'%(target+1,open_count,ui_open)],'Hide'))
        pg.children.extend(popups)
    return definitions
