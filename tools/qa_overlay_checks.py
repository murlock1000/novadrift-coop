"""Native combat-menu routing and actual client-pixel beacon checks."""
import json
import math
from pathlib import Path
import shutil
import subprocess
import time
from PIL import Image


def run(ns):
    control,wait,check,rows,ui=(ns[k] for k in ('control','wait','check','rows','ui'))
    runtime,out,root,pid,sha,ps=(ns[k] for k in ('runtime','out','ROOT','pid','sha','POWERSHELL'))
    read_jsonl,windows=ns['read_jsonl'],ns['windows']
    def record(kind,owner):
        return next((r for r in reversed(read_jsonl(runtime/'ship-balance.jsonl'))
                     if r.get('kind')==kind and r.get('owner')==owner),None)
    def tray(owner):return record('tray',owner)
    def rescue(owner):return record('rescue',owner)
    def no_modal():
        m=rows(True)[-1]
        return m['ui']==m['menu_session']==m['paused']==0
    def screenshot(name):
        identity=ui('focus')
        handle=next(w['handle'] for w in identity['windows'] if w['title'].startswith('Nova Drift'))
        path=runtime.parent/'test-profile/Temp'/name
        script=(root/'tools/capture_game.ps1').read_text()
        command=('& {\n'+script+'\n} -TargetPid '+str(pid)+' -ExpectedStartTicks '+str(identity['start_ticks'])+' -GameWindowHandle '+str(handle)+
                 " -ExpectedSha256 '"+sha+"' -OutputPath '"+windows(path)+"'")
        result=subprocess.run([ps,'-NoProfile','-Command',command],capture_output=True,text=True,timeout=20)
        if result.returncode:raise RuntimeError(result.stderr)
        with Image.open(path) as capture:
            if capture.width < 640 or capture.height < 360:
                raise RuntimeError('Game client is minimized or too small for visual review')
            if capture.mode == 'RGBA' and capture.getextrema()[3][1] == 0:
                raise RuntimeError('Game client capture is fully transparent')
        shutil.copy2(path,out/name)
        return out/name
    initial=rows()[-1];builds=(initial['b1'],initial['b2']);generation=initial['generation']
    control(engine_input=1)
    ui('press','SPACE');time.sleep(.5)
    check('native upgrade key cannot open a combat modal',no_modal(),rows(True)[-1])
    for owner in (1,2):
        control(upgrade=owner);wait(lambda:tray(owner)['open']==1)
        control(deferred_upgrade=owner);time.sleep(.4)
        check('direct deferred combat request remains a personal tray',no_modal() and tray(owner)['open']==1,{'owner':owner,'metrics':rows(True)[-1]})
    control(engine_input=1,engine_key=ord('O'));time.sleep(.4)
    screenshot('both-trays-inspect.png')
    control(engine_input=1);time.sleep(3)
    screenshot('both-trays-settled.png')
    ui('press','ESCAPE');wait(lambda:rows(True)[-1]['paused']==1)
    screenshot('shared-pause.png')
    ui('press','ESCAPE');wait(lambda:rows(True)[-1]['paused']==0)
    check('shared pause still resumes both ships',no_modal(),rows(True)[-1])
    # Capture ordinary gameplay with both trays collapsed.
    control(tray=1);wait(lambda:tray(1)['open']==0)
    control(tray=2);wait(lambda:tray(2)['open']==0)
    screenshot('co-op-gameplay.png')
    # Viper's persistent native barrier used to outlive its dead ship and
    # read the removed player while a teammate kept the world running.
    for owner in (1,2):
        control(augment=36,owner=owner);wait(lambda:tray(owner)['caustic_helpers']>0)
    check('native Viper helpers have independent owners',tray(1)['caustic_helpers']>0 and tray(2)['caustic_helpers']>0,{'first':tray(1),'second':tray(2)})
    for owner in (2,1):
        c=control(kill=owner,beacon_edge=owner,rescue_mode=1)
        wait(lambda:rows()[-1]['p'+str(owner)]==-1 and rescue(owner)['down']==1)
        wait(lambda:record('beacon_visual',owner)['frames']>2)
        check('death removes only the fallen Viper helper',tray(owner)['caustic_helpers']==0 and tray(3-owner)['caustic_helpers']>0,{'first':tray(1),'second':tray(2)})
        control(upgrade=owner,rescue_mode=1);time.sleep(.2)
        control(deferred_upgrade=owner,rescue_mode=1);time.sleep(.2)
        check('downed owner cannot open or queue a fullscreen shop',no_modal() and tray(owner)['open']==0,{'owner':owner,'metrics':rows(True)[-1]})
        time.sleep(1)
        state=next(r for r in reversed(read_jsonl(runtime/'ship-balance.jsonl')) if r.get('kind')=='state')
        check('downed teammate retains expanded arena',abs(state['zoom_scale']-1.2)<.00001 and abs(state['zoom_target']-1.2)<.00001,state)
        if '--mutation-beacon' in ns['sys'].argv:
            control(rescue_mode=1,mutate_beacon=1);time.sleep(.2)
        visual=record('beacon_visual',owner)
        image=Image.open(screenshot('beacon-owner-'+str(owner)+'.png')).convert('RGB')
        check('beacon stays in its expanded outer margin',10 < visual['x'] < image.width*.08 if owner==2 else image.width*.92 < visual['x'] < image.width-10,visual)
        # Actual presented pixels, not merely a 'draw was called' assertion.
        # Look around the entire range ring; a ship or projectile alone cannot
        # satisfy the required distribution across many angular sectors.
        hit=visible=0
        for i in range(64):
            a=i*math.tau/64
            x=visual['x']+visual['radius']*math.cos(a);y=visual['y']+visual['radius']*math.sin(a)
            if x<5 or y<5 or x>=image.width-5 or y>=image.height-5:continue
            visible+=1;found=False
            # The native bevelled hex is wider at its vertices than its edges.
            for radial in range(65,113,2):
                px=visual['x']+visual['radius']*radial/100*math.cos(a)
                py=visual['y']+visual['radius']*radial/100*math.sin(a)
                if px<3 or py<3 or px>=image.width-3 or py>=image.height-3:continue
                for dx in range(-2,3):
                    for dy in range(-2,3):
                        r,g,b=image.getpixel((round(px)+dx,round(py)+dy))
                        if owner==2:found|=r>70 and g>50 and b<155 and r>g*1.10
                        else:found|=r>65 and r>g*1.3 and r>b*1.10
            hit+=int(found)
        check('native beacon artwork has visible owner-colored client pixels',visible>=20 and hit>=visible*.55,{'owner':owner,'sectors':visible,'colored_sectors':hit,'visual':visual})
        control(rescue_mode=1,rescue_hold=owner)
        wait(lambda:rescue(owner)['progress']>.25,12)
        screenshot('revive-progress-owner-'+str(owner)+'.png')
        wait(lambda:rows()[-1]['p'+str(owner)]>=100000,12)
        wait(lambda:tray(owner)['caustic_helpers']>0)
        revived=rows()[-1]
        check('visible beacon revives correct ship with same build',rescue(owner)['down']==0 and revived['generation']==generation and (revived['b1'],revived['b2'])==builds,revived)
    ui('close');ns['process'].wait(timeout=20)
    check('clean owned overlay shutdown',ns['process'].returncode==0,ns['process'].returncode)
