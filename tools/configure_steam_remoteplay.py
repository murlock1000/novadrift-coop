#!/usr/bin/env python3
"""Configure Spacewar Remote Play, preserving other Steam settings.

Default is a read-only preview. --apply/--restore stop Steam gracefully only
when no game is running, back up the current config, change one VDF value,
and restart Steam. No invitations or account changes are performed.
"""
from pathlib import Path
import argparse, datetime, hashlib, json, os, re, subprocess, time

ROOT = Path(__file__).resolve().parents[1]
PS = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
OUT = ROOT/'local/remoteplay-re/steam-configuration'
TOKEN = re.compile(r'//[^\r\n]*|"(?:\\.|[^"\\])*"|[{}]')


def shell(script):
    p = subprocess.run([PS, '-NoProfile', '-Command', script],
                       capture_output=True, text=True, check=True, timeout=20)
    return json.loads(p.stdout.lstrip('\ufeff'))


def state():
    return shell(r"""$s=Get-ItemProperty 'HKCU:\Software\Valve\Steam';
$a=Get-ItemProperty 'HKCU:\Software\Valve\Steam\ActiveProcess';
$r=@(Get-ChildItem 'HKCU:\Software\Valve\Steam\Apps' -ErrorAction SilentlyContinue | ForEach-Object {
 $p=Get-ItemProperty $_.PSPath; if($p.Running -eq 1){$_.PSChildName} });
[pscustomobject]@{path=$s.SteamPath;user=$a.ActiveUser;running=[bool](Get-Process steam -ErrorAction SilentlyContinue);games=$r} | ConvertTo-Json -Compress""")


def nodes(text):
    tokens=[m for m in TOKEN.finditer(text) if not m.group().startswith('//')]
    def read(i, nested=False):
        result=[]
        while i<len(tokens):
            key=tokens[i];i+=1
            if key.group()=='}':
                if not nested:raise ValueError('Unexpected closing VDF brace')
                return result,i,key.start()
            if not key.group().startswith('"') or i>=len(tokens):raise ValueError('Invalid VDF key')
            value=tokens[i];i+=1
            if value.group()=='{':
                children,i,end=read(i,True)
                result.append(dict(key=json.loads(key.group()),children=children,end=end))
            elif value.group().startswith('"'):
                result.append(dict(key=json.loads(key.group()),value=json.loads(value.group()),
                                   start=value.start(),end=value.end(),key_start=key.start()))
            else:raise ValueError('Invalid VDF value')
        if nested:raise ValueError('Unclosed VDF object')
        return result,i,len(text)
    return read(0)[0]


def apps_node(text):
    entries=nodes(text)
    for key in ['UserLocalConfigStore','Software','Valve','Steam','apps']:
        match=[n for n in entries if n['key'].lower()==key.lower()]
        if len(match)!=1 or 'children' not in match[0]:
            raise ValueError('Expected one config object: '+key)
        node=match[0];entries=node['children']
    return node


def app_node(text, app_id='480'):
    match=[n for n in apps_node(text)['children'] if n['key']==str(app_id)]
    if len(match)>1:raise ValueError('Duplicate app config: '+str(app_id))
    if not match:return None
    if 'children' not in match[0]:raise ValueError('App config must be an object')
    return match[0]


def option(text, app_id='480'):
    app=app_node(text,app_id)
    entries=[n for n in app['children'] if n['key'].lower()=='launchoptions'] if app else []
    if len(entries)>1:raise ValueError('Duplicate LaunchOptions')
    return entries[0] if entries else None


def replace(text, value, app_id='480'):
    old=option(text,app_id)
    if old:
        if value is None:return text[:old['key_start']]+text[old['end']:]
        return text[:old['start']]+json.dumps(value,ensure_ascii=False)+text[old['end']:]
    if value is None:return text
    app=app_node(text,app_id);newline='\r\n' if '\r\n' in text else '\n'
    if app:
        end=app['end'];insert='\t"LaunchOptions"\t\t'+json.dumps(value,ensure_ascii=False)+newline
    else:
        end=apps_node(text)['end'];insert='\t"'+str(app_id)+'"'+newline+'\t{'+newline+'\t\t"LaunchOptions"\t\t'+json.dumps(value,ensure_ascii=False)+newline+'\t}'+newline
    return text[:end]+insert+text[end:]


def legacy_fling_restore(text, config):
    """Undo only the exact previous mod option, never a later user edit."""
    record=OUT/'original-setting.json'
    if not record.is_file():return text,False
    saved=json.loads(record.read_text())
    old=option(text,'1054430')
    if saved.get('config')==str(config) and old and old['value']==saved.get('approved_launch_option'):
        return replace(text,saved['launch_option'],'1054430'),True
    return text,False


def check_preserved_settings(before, after):
    def semantic(entries):
        result={}
        for n in entries:
            key=n['key'].lower()
            if key in result:raise ValueError('Duplicate VDF setting: '+key)
            result[key]=semantic(n['children']) if 'children' in n else n['value']
        return result
    before_values=semantic(nodes(before));after_values=semantic(nodes(after))
    for values in (before_values,after_values):
        apps=values['userlocalconfigstore']['software']['valve']['steam']['apps']
        for app_id in ('480','1054430'):
            if app_id in apps:
                apps[app_id].pop('launchoptions',None)
                if not apps[app_id]:del apps[app_id]
    if before_values!=after_values:raise RuntimeError('Unrelated Steam settings changed')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    mode=p.add_mutually_exclusive_group();mode.add_argument('--apply',action='store_true');mode.add_argument('--restore',action='store_true')
    a=p.parse_args();s=state();record=OUT/'480/original-setting.json'
    steam=Path(subprocess.check_output(['wslpath','-u',s['path']],text=True).strip())
    if s['user']:
        config=steam/'userdata'/str(s['user'])/'config/localconfig.vdf'
    elif record.is_file() and not a.apply:
        config=Path(json.loads(record.read_text())['config'])
        config.relative_to(steam/'userdata')
    else:
        raise RuntimeError('Steam sign-in required before applying setup; no account changes made')
    runtime=Path(json.loads((ROOT/'local/active-runtime.json').read_text())['runtime'])
    if not (runtime.parent/'.novadriftcoop-owned').is_file():raise RuntimeError('Owned runtime marker missing')
    helper=runtime/'SteamCoop.exe'
    prepared=json.loads((ROOT/'local/remoteplay-re/prepared/manifest.json').read_text())
    if hashlib.sha256(helper.read_bytes()).hexdigest()!=prepared['helper_sha256']:raise RuntimeError('Helper differs from reviewed build')
    desired=prepared['launch_option'];original=config.read_bytes();text=original.decode('utf-8-sig');old=option(text)
    print(json.dumps({'mode':'restore' if a.restore else 'apply' if a.apply else 'preview',
                      'steam_running':s['running'],'steam_signed_in':bool(s['user']),'running_game_count':len(s['games']),
                      'app_id':480,'app_name':'Spacewar','current_launch_option':old['value'] if old else None,'proposed_launch_option':desired,
                      'restore_previous_fling_option':legacy_fling_restore(text,config)[1]},indent=2),flush=True)
    if not (a.apply or a.restore):return
    if s['games']:raise RuntimeError('A Steam game is running; Steam was not stopped')
    OUT.mkdir(parents=True,exist_ok=True);record.parent.mkdir(parents=True,exist_ok=True)
    if a.restore:
        saved=json.loads(record.read_text())
        if saved['config']!=str(config):raise RuntimeError('Active Steam account differs from setup record')
        if not old or old['value']!=desired:raise RuntimeError('Launch option changed since setup; preserve it for manual review')
        value=saved['launch_option']
    else:
        if old and old['value']==desired:
            if not record.is_file():raise RuntimeError('Already configured, but original restore record missing')
            print('Already configured; no changes made');return
        if record.exists():raise RuntimeError('Original setting already recorded; review before replacing it')
        value=desired
    was_running=s['running'];changed=False
    try:
        if was_running:
            subprocess.run([str(steam/'steam.exe'),'-shutdown'],check=True,timeout=20)
            deadline=time.monotonic()+45
            while state()['running']:
                if time.monotonic()>deadline:raise RuntimeError('Steam did not exit gracefully; config untouched')
                time.sleep(.4)
        # Re-read after shutdown, because Steam flushes cached settings on exit.
        data=config.read_bytes();text=data.decode('utf-8-sig');current=option(text)
        if (current['value'] if current else None)!=(old['value'] if old else None):
            raise RuntimeError('Launch option changed during shutdown; config untouched')
        stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
        backup=OUT/(stamp+'-localconfig.vdf');backup.write_bytes(data)
        updated=replace(text,value)
        restored_fling=False
        if a.apply:updated,restored_fling=legacy_fling_restore(updated,config)
        got=option(updated)
        if (got['value'] if got else None)!=value:raise RuntimeError('Edited launch option failed parse check')
        check_preserved_settings(text,updated)
        if not a.restore:
            record.write_text(json.dumps({'config':str(config),'launch_option':current['value'] if current else None,
                                          'backup':str(backup),'approved_launch_option':desired},indent=2)+'\n')
        bom=b'\xef\xbb\xbf' if data.startswith(b'\xef\xbb\xbf') else b''
        temporary=config.with_name(config.name+'.novacoop.pending')
        with temporary.open('wb') as f:f.write(bom+updated.encode('utf-8'));f.flush();os.fsync(f.fileno())
        os.replace(temporary,config);changed=True
        result={'mode':'restored' if a.restore else 'configured','backup':str(backup),
                'launch_option':value,'config_sha256':hashlib.sha256(config.read_bytes()).hexdigest(),
                'game_launched':False,'invite_sent':False,'app_id':480,'previous_fling_option_restored':restored_fling}
        (OUT/('restore-result.json' if a.restore else 'apply-result.json')).write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2),flush=True)
    finally:
        if was_running and not state()['running']:
            subprocess.Popen([str(steam/'steam.exe'),'-silent'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        if not changed:print('Steam config was not changed',flush=True)


if __name__=='__main__':main()
