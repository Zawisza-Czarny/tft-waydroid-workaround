#!/usr/bin/env python3
"""Build a Magisk ZIP from separately obtained, unmodified HPE14 prebuilts."""
from pathlib import Path
import argparse,hashlib,zipfile,subprocess
ap=argparse.ArgumentParser()
ap.add_argument('prebuilts',type=Path)
ap.add_argument('--output',type=Path,default=Path('tft-waydroid-hpe14.zip'))
ap.add_argument('--cc',default='gcc',help='gcc with static glibc; target x86_64 Linux')
a=ap.parse_args(); here=Path(__file__).resolve().parent
core=a.prebuilts/'lib64/libhoudini.so'
assert hashlib.sha256(core.read_bytes()).hexdigest()=='5ef64035ec89bca3d5e33b9f3b6556f98f6c11d11957387d403ae4a4d7cc40ec','Unexpected HPE build'
binary=here/'tft-startup-compat'
subprocess.run([a.cc,'-static','-O2','-Wall','-Wextra','-Werror',str(here/'tft_houdini_startup_compat.c'),'-o',str(binary)],check=True)
with zipfile.ZipFile(a.output,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 def add(name,data,mode=0o644):
  i=zipfile.ZipInfo(name);i.create_system=3;i.external_attr=(0o100000|mode)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,data)
 add('module.prop',b'id=tft_hpe14_test\nname=TFT Waydroid HPE14 compatibility\nversion=1.2\nversionCode=5\nauthor=local-debug\ndescription=HPE14 startup helper and Waydroid mouse/cursor settings\n')
 add('README.md',(here/'README.md').read_bytes())
 add('PUBLISHING.md',(here/'PUBLISHING.md').read_bytes())
 add('system.prop',b'ro.dalvik.vm.native.bridge=libhoudini.so\n')
 add('post-fs-data.sh',(here/'post-fs-data.sh').read_bytes(),0o755)
 add('customize.sh',b'#!/system/bin/sh\n[ "$(getprop ro.product.cpu.abi)" = x86_64 ] || abort "Requires x86_64 Waydroid"\nset_perm_recursive "$MODPATH" 0 0 0755 0644\nset_perm "$MODPATH/launch-tft.sh" 0 0 0755\nset_perm "$MODPATH/bin/tft-startup-compat" 0 0 0755\nset_perm "$MODPATH/system/bin/houdini" 0 0 0755\nset_perm "$MODPATH/system/bin/houdini64" 0 0 0755\n',0o755)
 add('launch-tft.sh',(here/'launch-tft.sh').read_bytes(),0o755)
 add('bin/tft-startup-compat',binary.read_bytes(),0o755)
 add('source/tft_houdini_startup_compat.c',(here/'tft_houdini_startup_compat.c').read_bytes())
 for folder in ['lib/arm','lib64/arm64']:
  add('system/'+folder+'/.replace',b'')
  for f in sorted((a.prebuilts/folder).rglob('*')):
   if f.is_file():add('system/'+f.relative_to(a.prebuilts).as_posix(),f.read_bytes())
 for f in ['lib/libhoudini.so','lib64/libhoudini.so','bin/houdini','bin/houdini64']:
  add('system/'+f,(a.prebuilts/f).read_bytes(),0o755 if f.startswith('bin/') else 0o644)
print(a.output.resolve())
print('SHA256',hashlib.sha256(a.output.read_bytes()).hexdigest())
