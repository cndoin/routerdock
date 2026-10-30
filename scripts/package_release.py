"""Package generic firmware and the source-only router agent. Never bundle NVS."""
from pathlib import Path
import zipfile,tarfile,hashlib,json
root=Path(__file__).resolve().parents[1];out=root/'artifacts';out.mkdir(exist_ok=True)
build=root/'firmware/.pio/build/esp32-crowpanel-28'
files={'bootloader.bin':build/'bootloader.bin','partitions.bin':build/'partitions.bin','firmware.bin':build/'firmware.bin'}
# Framework path resolved by PlatformIO's package directory, overridable for CI.
import os
packages=Path(os.environ.get('PLATFORMIO_CORE_DIR',str(Path.home()/'.platformio')))/'packages'
files['boot_app0.bin']=packages/'framework-arduinoespressif32/tools/partitions/boot_app0.bin'
for p in files.values():assert p.is_file(),p
flash='''"""Flash the verified ESP32 target; preserves the NVS region."""
import argparse,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--port',required=True);a=p.parse_args()
root=Path(__file__).resolve().parent
pairs=[('0x1000','bootloader.bin'),('0x8000','partitions.bin'),('0xe000','boot_app0.bin'),('0x10000','firmware.bin')]
for _,name in pairs:
 if not (root/name).is_file():raise SystemExit('Missing '+name)
cmd=[sys.executable,'-m','esptool','--chip','esp32','--port',a.port,'--baud','460800','write-flash']
for address,name in pairs:cmd.extend([address,str(root/name)])
subprocess.run(cmd,check=True)
'''
with zipfile.ZipFile(out/'routerdock-esp32-crowpanel-28.zip','w',zipfile.ZIP_DEFLATED) as z:
 for name,p in files.items():z.write(p,name)
 z.writestr('flash.py',flash);z.write(root/'docs/HARDWARE.md','HARDWARE.md');z.write(root/'LICENSE','LICENSE')
with tarfile.open(out/'routerdock-agent.tar.gz','w:gz') as t:
 for p in sorted((root/'openwrt').iterdir()):
  if not p.is_file():continue
  info=t.gettarinfo(str(p),p.name);info.mode=0o755 if p.suffix=='.sh' else 0o644;info.uid=info.gid=0;info.uname=info.gname='root'
  with p.open('rb') as f:t.addfile(info,f)
items=[]
for p in sorted(out.iterdir()):
 if p.name=='SHA256SUMS':continue
 if p.suffix in ('.zip','.gz'):items.append(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name)
(out/'SHA256SUMS').write_text('\n'.join(items)+'\n',encoding='ascii')
print('Packaged firmware/app boot files (no NVS), source-only agent, SHA256SUMS.')
