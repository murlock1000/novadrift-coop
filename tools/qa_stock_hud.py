"""Native owner-specific weapon reserves and construct HUD client-pixel checks."""
import math
import shutil
import subprocess
import time
from PIL import Image


def run(ns):
    control,wait,check,rows,ui=(ns[k] for k in ('control','wait','check','rows','ui'))
    runtime,out=ns['runtime'],ns['out']
    def stock(owner):
        return next((r for r in reversed(ns['read_jsonl'](runtime/'ship-balance.jsonl')) if r.get('kind')=='stock' and r['owner']==owner),None)
    def snapshot(name,hold=True,**changes):
        identity=ui('focus')
        native_dir=runtime.parent/'test-profile/Local/NovaDrift'
        stamp=(native_dir/'native-hud-2.png').stat().st_mtime_ns if (native_dir/'native-hud-2.png').exists() else 0
        control(screen=1,freeze=int(hold),**changes)
        wait(lambda:(native_dir/'native-hud-2.png').exists() and (native_dir/'native-hud-2.png').stat().st_mtime_ns!=stamp)
        time.sleep(.15)
        handle=next(w['handle'] for w in identity['windows'] if w['title'].startswith('Nova Drift'))
        path=runtime.parent/'test-profile/Temp'/(name+'.png')
        command=('& {\n'+(ns['ROOT']/'tools/capture_game.ps1').read_text()+'\n} -TargetPid '+str(ns['pid'])+' -ExpectedStartTicks '+str(identity['start_ticks'])+' -GameWindowHandle '+str(handle)+
                 " -ExpectedSha256 '"+ns['sha']+"' -OutputPath '"+ns['windows'](path)+"'")
        result=subprocess.run([ns['POWERSHELL'],'-NoProfile','-Command',command],capture_output=True,text=True,timeout=20)
        if result.returncode:raise RuntimeError(result.stderr)
        with Image.open(path) as capture:
            if capture.width<640 or capture.height<360 or (capture.mode=='RGBA' and capture.getextrema()[3][1]==0):raise RuntimeError('Invalid game-client capture')
        shutil.copy2(path,out/(name+'.png'))
        for owner in (1,2):shutil.copy2(native_dir/f'native-hud-{owner}.png',out/f'{name}-native-{owner}.png')
        control(**changes)
        return out/(name+'.png')
    def pixels(image,name):
        output=Image.open(image).convert('RGB')
        for owner in (1,2):
            g=stock(owner);native=Image.open(out/f'{name}-native-{owner}.png').convert('RGBA');nw,nh=native.size
            samples=matches=0
            for y in range(math.ceil(g['sy'])+2,math.floor(g['sy']+g['sh'])-2,2):
                for x in range(math.ceil(g['sx'])+2,math.floor(g['sx']+g['sw'])-2,2):
                    px=native.getpixel((x,y))
                    if px[3]<250 or max(px[:3])<80:continue
                    neighbors=[native.getpixel((x+dx,y+dy)) for dx,dy in ((-1,0),(1,0),(0,-1),(0,1))]
                    # Compare opaque, flat interiors; the GPU resamples glyph
                    # edges at the fractional destination Y coordinate.
                    if any(q[3]<250 or max(abs(a-b) for a,b in zip(px[:3],q[:3]))>12 for q in neighbors):continue
                    ox=round((g['dx']+(x-g['sx'])*g['dw']/g['sw'])*output.width/nw)
                    oy=round((g['dy']+(y-g['sy'])*g['dh']/g['sh'])*output.height/nh)
                    samples+=1
                    matches+=any(sum(abs(a-b) for a,b in zip(px[:3],output.getpixel((ox+dx,oy+dy))))<50 for dx,dy in ((0,0),(-1,0),(1,0),(0,-1),(0,1)))
            check(f'P{owner} presents its own native ammo and construct pixels',samples>50 and matches/samples>.87,{'samples':samples,'matches':matches,'geometry':g,'image':str(image)})
    control(augment=7,owner=1)
    time.sleep(.7)
    control(augment=32,owner=2)
    time.sleep(.7)
    for owner,assets in ((1,(46,)),(2,(89,142,217,46))):
        for asset in assets:
            control(native_augment=asset,owner=owner)
            time.sleep(.6)
    wait(lambda:stock(1) and stock(2))
    time.sleep(3)
    mutation='--mutation-stock-hud' in ns['sys'].argv
    image=snapshot('independent-native-stock',mutate_stock=int(mutation))
    check('native reserve state belongs to each build',stock(1)['blade']>0 and stock(2)['blade']==0 and stock(1)['mines']==0 and stock(2)['mines']>0,[stock(1),stock(2)])
    pixels(image,'independent-native-stock')
    if mutation:raise RuntimeError('Stock-owner mutation unexpectedly passed')
    # The native direct slot key depletes P2 mines while P1's build is untouched.
    before=stock(2)['mines'];partner=stock(1)['allies']
    control(engine_input=1,engine_key=ord('7'))
    wait(lambda:stock(2)['mines']<before,5)
    control(engine_input=1)
    check('P2 deployment consumes only its own reserve',stock(1)['mines']==0 and stock(1)['allies']==partner,[stock(1),stock(2)])
    image=snapshot('after-native-deploy')
    pixels(image,'after-native-deploy')
    control(level=3);wait(lambda:ns['tray'](1)['budget']>0 and ns['tray'](2)['budget']>0)
    control(tray=1);wait(lambda:ns['tray'](1)['open']==1)
    control(tray=2);wait(lambda:ns['tray'](2)['open']==1)
    time.sleep(3)
    image=snapshot('stock-with-both-upgrade-trays')
    pixels(image,'stock-with-both-upgrade-trays')
    for owner in (1,2):
        scale=min(Image.open(image).width/2400,Image.open(image).height/1350)
        check('native stock stays below upgrade controls and above health',stock(owner)['dy']>(Image.open(image).height-285*scale+108*scale) and stock(owner)['dy']+stock(owner)['dh']<Image.open(image).height-38*scale,stock(owner))
    control(engine_input=1,engine_key=ord('O'));time.sleep(.3)
    snapshot('compact-native-inspect',engine_input=1,engine_key=ord('O'))
    control(engine_input=1)
    control(tray=1);wait(lambda:ns['tray'](1)['open']==0)
    control(tray=2);wait(lambda:ns['tray'](2)['open']==0)
    # Keep the documentation beacon away from the arena edge and show progress.
    control(beacon_center=2,kill=2,rescue_mode=1)
    def rescue():return next((r for r in reversed(ns['read_jsonl'](runtime/'ship-balance.jsonl')) if r.get('kind')=='rescue' and r['owner']==2),None)
    wait(lambda:rows()[-1]['p2']==-1 and rescue() and rescue()['down']==1)
    time.sleep(1)
    for attempt in range(3):
        beacon=Image.open(snapshot('beacon-visible',hold=False,rescue_mode=1)).convert('RGB')
        visual=next(r for r in reversed(ns['read_jsonl'](runtime/'ship-balance.jsonl')) if r.get('kind')=='beacon_visual' and r['owner']==2)
        if attempt==0:check('documentation capture contains the whole beacon',visual['radius']<visual['x']<beacon.width-visual['radius'] and visual['radius']<visual['y']<beacon.height-visual['radius'],visual)
        sectors=0
        for i in range(64):
            a=i*math.tau/64;cx,cy=math.cos(a),math.sin(a);found=False
            for radial in range(55,113,2):
                x=round(visual['x']+visual['radius']*radial/100*cx)
                y=round(visual['y']+visual['radius']*radial/100*cy)
                rr,gg,bb=beacon.getpixel((round(x+15*cx),round(y+15*cy)))
                for dx in range(-2,3):
                    for dy in range(-2,3):
                        r,g,b=beacon.getpixel((x+dx,y+dy));dr,dg,db=r-rr,g-gg,b-bb
                        # Translucent gold over a blue nebula can have R ~= G.
                        # Require a local red/green rise beyond the blue rise.
                        found|=dr>20 and dg>15 and dr+dg>2*db+40 and r>g*.8
            sectors+=int(found)
        if sectors>=36:break
        time.sleep(1)  # allow a native screen flash to fade before documentation capture
    check('documentation screenshot visibly contains the native gold beacon',sectors>=36,{'sectors':sectors,'visual':visual})
    control(rescue_mode=1,rescue_hold=2)
    wait(lambda:rescue()['progress']>.3,12)
    snapshot('beacon-revival',hold=False,rescue_mode=1,rescue_hold=2)
    wait(lambda:rows()[-1]['p2']>=100000,12)
    image=snapshot('stock-after-revival')
    pixels(image,'stock-after-revival')
    check('revival retains personal ammo and construct HUDs',rows()[-1]['p1']>=100000 and stock(2)['sw']>0,[stock(1),stock(2)])
    control(kill=1)
    wait(lambda:rows()[-1]['p1']==-1 and rows()[-1]['p2']>=100000)
    image=snapshot('stock-with-p2-surviving')
    pixels(image,'stock-with-p2-surviving')
    check('surviving P2 keeps its own native reserve HUD',stock(2)['blade']==0 and stock(2)['mines']>0,[stock(1),stock(2)])
    ui('close');ns['process'].wait(timeout=20)
    check('clean owned stock-HUD shutdown',ns['process'].returncode==0,ns['process'].returncode)
