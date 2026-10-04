#!/usr/bin/env python3
"""Desktop launcher: open Waydroid, then invoke the installed startup helper."""
from pathlib import Path
import argparse,os,shlex,shutil,subprocess,time,re,sys
ap=argparse.ArgumentParser()
ap.add_argument('--adb-command',default='adb' if shutil.which('adb') else 'toolbox run adb')
ap.add_argument('--ip',help='Override the IP reported by waydroid status')
ap.add_argument('--check-only',action='store_true',help='Check bridge/module without starting or stopping TFT')
a=ap.parse_args()
state=Path(os.environ.get('XDG_STATE_HOME',str(Path.home()/'.local/state')))/'tft-waydroid'
state.mkdir(parents=True,exist_ok=True)
log=open(state/'launcher.log','a',buffering=1)
adb=shlex.split(a.adb_command)
def command(argv,timeout=15):
 r=subprocess.run(argv,capture_output=True,text=True,errors='replace',timeout=timeout)
 log.write(r.stdout+r.stderr)
 return r
def notify(msg):
 if shutil.which('notify-send'):
  subprocess.run(['notify-send','TFT · Waydroid',msg],capture_output=True)
try:
 if not a.check_only:
  subprocess.Popen(['waydroid','show-full-ui'],stdout=log,stderr=log,start_new_session=True)
 target=None
 deadline=time.monotonic()+120
 while time.monotonic()<deadline:
  status=command(['waydroid','status']).stdout
  match=re.search(r'^IP address:\s*(\S+)',status,re.M)
  ip=a.ip or (match[1] if match else None)
  if ip:
   target=adb+['-s',ip+':5555']
   try:
    command(adb+['connect',ip+':5555'],timeout=4)
    ready=command(target+['shell','getprop sys.boot_completed'],timeout=4)
    if ready.returncode==0 and ready.stdout.strip()=='1':break
   except subprocess.TimeoutExpired:pass
  if a.check_only:raise RuntimeError('Waydroid is not ready or ADB is unavailable.')
  time.sleep(1)
 else:raise RuntimeError('Waydroid did not become ready within 120 seconds.')
 script='/data/adb/modules/tft_hpe14_test/launch-tft.sh'
 if a.check_only:
  android='set -e; /system/bin/sh '+script+' --check-only; if [ -f /data/adb/modules/tft_hpe14_test/disable ]; then echo MODULE_DISABLED; exit 1; fi'
 else:android='/system/bin/sh '+script
 result=command(target+['shell','su -c '+shlex.quote(android)],timeout=90)
 print(result.stdout,end='');print(result.stderr,end='',file=sys.stderr)
 if result.returncode:raise RuntimeError(result.stdout.strip() or result.stderr.strip() or 'TFT startup failed')
 if not a.check_only:notify('Launch completed. Details: '+str(state/'launcher.log'))
except Exception as e:
 log.write(str(e)+'\n');print(str(e),file=sys.stderr)
 if not a.check_only:notify(str(e)+'\nLog: '+str(state/'launcher.log'))
 sys.exit(1)
