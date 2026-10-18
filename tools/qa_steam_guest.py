#!/usr/bin/env python3
"""Prepare or run one owned Steam Link guest without changing Steam configuration.

Default prepares a private plan only. Explicit --run requires a fresh Invite Anyone
HTTPS URL in a file beneath local/. No invitation URL is printed or saved in reports.
The installed guest necessarily receives it in its own argv and may log it privately.
Preferences are isolated by a reversible rename, not the incomplete --settings flag.
This is a client-process smoke test; it never claims a real connection/input pass.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
import uuid
from urllib.parse import urlsplit

ROOT=Path(__file__).resolve().parents[1]
PRIVATE=ROOT/'local/steam-guest'
CLIENT=Path('/mnt/c/Program Files (x86)/Steam/streaming_client.exe')
EXPECTED_SHA='ced85fa9dd490716e2b8a8a88046021ced7a0f4522d92e8cd7db01ffc9b0600d'
PS='/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'
BROKER=ROOT/'tools/steam_guest_process.ps1'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def windows(path):
    return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()

def linux(path):
    return Path(subprocess.check_output(['wslpath','-u',path],text=True).strip())

def write_json(path,value):
    temporary=path.with_name(path.name+'.tmp')
    with temporary.open('w') as f:
        os.chmod(temporary,0o600)
        json.dump(value,f,indent=2);f.write('\n')
    temporary.replace(path)

def broker_args(action,run=None,identity=None,relay_only=False):
    args=[PS,'-NoProfile','-ExecutionPolicy','Bypass','-File',windows(BROKER),'-Action',action,
          '-ExpectedExe',windows(CLIENT),'-ExpectedSha256',EXPECTED_SHA]
    if run:args+=['-RunDirectory',windows(run/'working')]
    if action=='start' and run and (run/'working/public-sdr-config.json').is_file():
        config=run/'working/public-sdr-config.json'
        args+=['-SdrConfig',windows(config),'-SdrSha256',digest(config)]
    if action=='start' and relay_only:args+=['-RelayOnly']
    if identity:args+=['-TargetPid',str(identity['pid']),'-StartTicks',str(identity['start_ticks'])]
    return args

def broker(action,run=None,identity=None):
    result=subprocess.run(broker_args(action,run,identity),capture_output=True,text=True,timeout=20)
    if result.returncode:raise RuntimeError('Owned Windows guest '+action+' operation failed')
    try:return json.loads(result.stdout.lstrip('\ufeff'))
    except ValueError:raise RuntimeError('Guest broker returned invalid status')

def roots():
    known=broker('paths')
    # Static SDL_GetPrefPath("Valve Corporation", "SteamLink") observed in this exact build.
    # Preserve Local as well in case platform codec/client helpers use it.
    return [linux(known[key])/'Valve Corporation/SteamLink' for key in ('roaming','local')]

def fingerprint(path):
    if path.is_symlink() or path.parent.is_symlink():raise RuntimeError('Preference root or parent is a link; manual review required')
    if not path.exists():return {}
    if not path.is_dir():raise RuntimeError('Preference root is not an ordinary directory')
    result={}
    for p in sorted(path.rglob('*')):
        if p.is_symlink():raise RuntimeError('Preference tree contains a link; manual review required')
        if p.is_file():result[str(p.relative_to(path))]=digest(p)
    return result

def validate_invite(path):
    path=path.resolve()
    try:path.relative_to(ROOT/'local')
    except ValueError:raise ValueError('Keep the private invitation file beneath project local/')
    if not path.is_file() or path.stat().st_size>8192:raise ValueError('Private invitation file missing or too large')
    if path.stat().st_mode & 0o077:raise ValueError('Private invitation file must have mode 600 (chmod 600)')
    value=path.read_text(encoding='utf-8-sig').strip()
    if not value or re.search(r'[\s"<>\\]',value):raise ValueError('Invalid private invitation text')
    parsed=urlsplit(value)
    try:port=parsed.port
    except ValueError:raise ValueError('Invalid invitation port')
    if (parsed.scheme!='https' or parsed.hostname not in ('s.team','steamcommunity.com') or
        parsed.username or parsed.password or parsed.fragment or port not in (None,443) or
        not re.match(r'^/remoteplay/connect/[^/]+',parsed.path)):
        raise ValueError('Use a fresh Steam Invite Anyone HTTPS URL')
    return value

def reviewed_public_config(path,expected):
    path=path.resolve()
    try:path.relative_to(ROOT/'local')
    except ValueError:raise ValueError('Reviewed public SDR configuration must be beneath project local/')
    if not re.fullmatch(r'[a-fA-F0-9]{64}',expected or ''):raise ValueError('Explicit reviewed SDR SHA-256 is required')
    if not path.is_file() or path.stat().st_size>1048576 or digest(path)!=expected.lower():raise ValueError('Reviewed public SDR configuration identity mismatch')
    value=json.loads(path.read_text())
    if value.get('success') is not True or not isinstance(value.get('revision'),int) or not isinstance(value.get('certs'),list) or not value['certs'] or not value.get('pops'):
        raise ValueError('Reviewed public SDR configuration has an unexpected schema')
    return path

def ensure_no_clients():
    if broker('list')['count']:raise RuntimeError('Close existing Steam Link/streaming clients before this isolated test')

def isolate(run,state):
    ensure_no_clients()
    entries=[]
    for i,path in enumerate(roots()):
        # Preserve the original directory by same-volume rename: file contents, ACLs,
        # timestamps and alternate streams are not copied/recreated.
        backup=path.with_name(path.name+'.novacoop-guest-backup-'+state['tag'])
        if backup.exists():raise RuntimeError('Preference backup already exists')
        entries.append({'path':str(path),'backup':str(backup),'present':path.exists(),
                        'parent_present':path.parent.exists(),'before':fingerprint(path),'index':i,
                        'original_inode':path.stat().st_ino if path.exists() else None})
    state['preferences']=entries;state['phase']='isolating';write_json(run/'journal.json',state)
    for entry in entries:
        path=Path(entry['path']);backup=Path(entry['backup'])
        if entry['present']:path.rename(backup)
    state['phase']='isolated';write_json(run/'journal.json',state)

def load_identity(run):
    path=run/'working/client-identity.json'
    if not path.exists():return None
    value=json.loads(path.read_text(encoding='utf-8-sig'))
    if (not isinstance(value.get('exe'),str) or value['exe'].lower()!=windows(CLIENT).lower() or value.get('sha256')!=EXPECTED_SHA or
        not isinstance(value.get('pid'),int) or not isinstance(value.get('start_ticks'),int)):
        raise RuntimeError('Guest process identity file invalid')
    return value

def restore(run,state):
    if not re.fullmatch(r'[A-Za-z0-9_-]+',state.get('tag','')):raise RuntimeError('Invalid recovery ownership tag')
    expected={str(p) for p in roots()}
    entries=state.get('preferences',[])
    if {e['path'] for e in entries}!=expected:raise RuntimeError('Recovery preference paths do not match current user known folders')
    identity=load_identity(run)
    if identity:
        # Start-time mismatch refuses cleanup, avoiding a reused PID.
        stopped=broker('stop',run,identity);state['client_cleanup']=stopped
    ensure_no_clients()
    for entry in entries:
        path=Path(entry['path']);backup=Path(entry['backup'])
        if backup!=path.with_name(path.name+'.novacoop-guest-backup-'+state['tag']):raise RuntimeError('Invalid recovery backup path')
        original_in_place=entry['present'] and not backup.exists()
        if original_in_place and (not path.is_dir() or fingerprint(path)!=entry['before'] or path.stat().st_ino!=entry['original_inode']):raise RuntimeError('Ambiguous original preference recovery state')
        if entry['present'] and not original_in_place and fingerprint(backup)!=entry['before']:raise RuntimeError('Preserved original preferences changed')
        if path.exists() and not original_in_place:
            fingerprint(path)  # Refuse links or unexpected root types before moving/deleting.
            output=path.with_name(path.name+'.novacoop-guest-output-'+state['tag'])
            if output.exists():raise RuntimeError('Generated preference archive already exists')
            path.rename(output)
            entry['generated_output']=str(output);write_json(run/'journal.json',state)
        if entry['present'] and not original_in_place:backup.rename(path)
        output=Path(entry['generated_output']) if entry.get('generated_output') else None
        if output and output!=path.with_name(path.name+'.novacoop-guest-output-'+state['tag']):raise RuntimeError('Generated preference ownership path mismatch')
        if output and output.exists():
            archive=run/'generated-preferences'/str(entry['index'])
            archive.parent.mkdir(parents=True,exist_ok=True)
            if archive.exists():
                if fingerprint(archive)!=fingerprint(output):raise RuntimeError('Generated private preference archive incomplete; manual review required')
            else:shutil.copytree(output,archive,symlinks=True)
            shutil.rmtree(output)
        if not entry['parent_present'] and path.parent.exists():
            try:path.parent.rmdir()
            except OSError:pass  # Never remove another application's files.
        if fingerprint(path)!=entry['before'] or path.exists()!=entry['present'] or (entry['present'] and path.stat().st_ino!=entry['original_inode']):
            raise RuntimeError('Original preferences were not restored exactly')
        entry['restored']=True;write_json(run/'journal.json',state)
    state['phase']='restored';write_json(run/'journal.json',state)

def recover(path):
    run=path.resolve()
    try:run.relative_to(PRIVATE.resolve())
    except ValueError:raise RuntimeError('Recovery folder must be an owned local/steam-guest run')
    marker=run/'.novacoop-steam-guest'
    state=json.loads((run/'journal.json').read_text())
    global CLIENT,EXPECTED_SHA
    if state.get('client_binary'):
        CLIENT=Path(state['client_binary']['path']);EXPECTED_SHA=state['client_binary']['sha256']
    if not marker.is_file() or marker.read_text().strip()!=state.get('tag'):raise RuntimeError('Owned recovery marker mismatch')
    if state.get('phase')=='restored':
        expected={str(p) for p in roots()}
        entries=state.get('preferences',[])
        if {e['path'] for e in entries}!=expected or any(fingerprint(Path(e['path']))!=e['before'] or Path(e['path']).exists()!=e['present'] for e in entries):
            raise RuntimeError('Completed recovery no longer matches original preferences')
    else:restore(run,state)
    print(json.dumps({'status':'RESTORED','run':str(run),'original_preferences_restored':True}))

def main():
    global CLIENT,EXPECTED_SHA
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',action='store_true',help='Explicitly launch one owned client; never starts Steam or sends an invite')
    parser.add_argument('--url-file',type=Path,help='Private UTF-8 Invite Anyone URL file beneath local/')
    parser.add_argument('--seconds',type=int,default=30,help='Bounded client lifetime, 5..600 seconds')
    parser.add_argument('--client',type=Path,help='Explicit reviewed extracted standalone SteamLink.exe or streaming_client.exe')
    parser.add_argument('--client-sha256',help='Required SHA-256 when selecting a reviewed alternate client')
    parser.add_argument('--sdr-config',type=Path,help='Reviewed unmodified Valve public GetSDRConfig JSON under local/')
    parser.add_argument('--sdr-config-sha256',help='Explicit reviewed public SDR file SHA-256')
    parser.add_argument('--relay-only',action='store_true',help='Request disabled peer-to-peer ICE with native --settingsdata 980101; relay traffic still needs observation')
    parser.add_argument('--restore',type=Path,help='Recover the exact preference directories of an interrupted owned run')
    args=parser.parse_args()
    if args.restore:
        if args.run:parser.error('--restore and --run are mutually exclusive')
        recover(args.restore);return 0
    if not 5<=args.seconds<=600:parser.error('--seconds must be 5..600')
    if args.client:
        if not re.fullmatch(r'[a-fA-F0-9]{64}',args.client_sha256 or ''):parser.error('--client requires an explicit reviewed --client-sha256')
        CLIENT=args.client.resolve();EXPECTED_SHA=args.client_sha256.lower()
        try:CLIENT.relative_to(ROOT/'local')
        except ValueError:parser.error('Alternate client must be an owned extracted copy beneath project local/')
        if CLIENT.name.lower() not in ('steamlink.exe','streaming_client.exe'):parser.error('Expected a reviewed Steam Link client executable')
    elif args.client_sha256:parser.error('--client-sha256 requires --client')
    if bool(args.sdr_config)!=bool(args.sdr_config_sha256):parser.error('--sdr-config and --sdr-config-sha256 must be supplied together')
    public_config=reviewed_public_config(args.sdr_config,args.sdr_config_sha256) if args.sdr_config else None
    if digest(CLIENT)!=EXPECTED_SHA:raise RuntimeError('Reviewed client build identity changed')
    invite=validate_invite(args.url_file) if args.url_file else None
    if args.run and not invite:parser.error('--run requires --url-file')
    PRIVATE.mkdir(parents=True,exist_ok=True);os.chmod(PRIVATE,0o700)
    tag=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')+'-'+uuid.uuid4().hex[:8]
    run=PRIVATE/tag;run.mkdir(mode=0o700);(run/'working').mkdir(mode=0o700)
    (run/'.novacoop-steam-guest').write_text(tag+'\n')
    if public_config:shutil.copy2(public_config,run/'working/public-sdr-config.json')
    report={'status':'PREPARED','client_launched':False,'connection_tested':False,'remote_input_tested':False,
            'same_pc_supported':'unverified','run':str(run),'client_sha256':EXPECTED_SHA,
            'public_sdr_config_sha256':digest(public_config) if public_config else None,
            'relay_only_requested':args.relay_only,'relay_path_verified':False,
            'command':'streaming_client.exe --windowed --size 960x540 '+('--sdr_config <reviewed public config> ' if public_config else '')+('--settingsdata 980101 ' if args.relay_only else '')+'<private Invite Anyone HTTPS URL>',
            'invitation_saved_in_report':False,'steam_configuration_modified':False,
            'local_preferences_temporarily_isolated':False}
    write_json(run/'report.json',report)
    if not args.run:
        print(json.dumps(report,indent=2));return 0
    state={'tag':tag,'phase':'prepared','preferences':[],'client_binary':{'path':str(CLIENT),'sha256':EXPECTED_SHA}};write_json(run/'journal.json',state)
    process=None;failure=None;requested_stop=False
    print(json.dumps({'status':'STARTING_GUEST','run':str(run),'stop_request':str(run/'stop.request')}),flush=True)
    try:
        isolate(run,state);report['local_preferences_temporarily_isolated']=True
        with (run/'private-broker.log').open('w') as output:
            process=subprocess.Popen(broker_args('start',run,relay_only=args.relay_only),stdin=subprocess.PIPE,stdout=output,stderr=output,text=True)
            process.stdin.write(invite+'\n');process.stdin.close();invite=None
            deadline=time.monotonic()+args.seconds
            while time.monotonic()<deadline:
                if (run/'stop.request').exists():requested_stop=True;break
                identity=load_identity(run)
                if identity:
                    report['client_launched']=True
                    if 'client' not in state:state['client']=identity;write_json(run/'journal.json',state)
                if process.poll() is not None:break
                time.sleep(.1)
            if not report['client_launched']:raise RuntimeError('Guest did not publish an owned process identity')
            if process.poll() is not None and not requested_stop:raise RuntimeError('Guest client exited before the bounded observation ended')
            report['client_alive_at_deadline']=process.poll() is None
            report['ended_by_owned_stop_request']=requested_stop
    except (Exception,KeyboardInterrupt) as e:
        failure=type(e).__name__
        report['error']='Guest preparation/process observation failed; inspect private artifacts locally'
    finally:
        try:
            if state.get('preferences'):restore(run,state)
            report['original_preferences_restored_or_untouched']=not state.get('preferences') or state.get('phase')=='restored'
            if process:
                try:process.wait(timeout=15)
                except subprocess.TimeoutExpired:raise RuntimeError('Owned client broker did not exit after cleanup')
        except Exception:
            report['recovery_required']=True
            report['recovery_command']='python3 tools/qa_steam_guest.py --restore '+str(run)
            failure='RecoveryError'
        report['status']='CLIENT_PROCESS_SMOKE_ONLY' if not failure else 'FAIL'
        report['failure_type']=failure
        write_json(run/'report.json',report);print(json.dumps(report,indent=2))
    return 0 if not failure else 1

if __name__=='__main__':
    try:raise SystemExit(main())
    except Exception:
        # Do not print exception/argv text that could include private invitation data.
        print(json.dumps({'status':'FAIL','error':'Guest setup failed before launch; no invitation is printed'}))
        raise SystemExit(1)
