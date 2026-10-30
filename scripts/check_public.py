"""Check publication inputs and embedded assets. No private values are printed."""
from pathlib import Path
import re,sys,subprocess
root=Path(__file__).resolve().parents[1]
forbidden=[r'gh[pousr]_[A-Za-z0-9_]{20,}',r'github_pat_[A-Za-z0-9_]+',r'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY',r'192\.168\.100\.',r'(?m)^#define\s+MONITOR_(?:SSID|WIFI_PASS|TOKEN)\s+"[^"\n]+"']
errors=[]
try:
 names=subprocess.check_output(['git','ls-files','--cached'],cwd=root,text=True,stderr=subprocess.DEVNULL).splitlines()
except subprocess.CalledProcessError:
 names=[str(p.relative_to(root)) for p in root.rglob('*') if p.is_file() and not set(p.parts)&{'.pio','.git','node_modules','.local','artifacts','__pycache__'}]
for name in names:
 p=root/name
 if any(part in name.lower() for part in ('device_private','secrets.h','docs/recon/','.local/')):errors.append(name+': private path')
 if p.suffix.lower() in ('.png','.jpg','.ico'):continue
 data=p.read_text(encoding='utf-8-sig',errors='replace')
 for pattern in forbidden:
  if re.search(pattern,data):errors.append(name+': credential or lab-specific value')
 if '\ufffd' in data:errors.append(name+': invalid UTF-8')
html=(root/'firmware/web/index.html').read_text(encoding='utf-8-sig')
embedded=(root/'firmware/include/PortalPage.h').read_text(encoding='utf-8-sig')
if embedded.split('R"RDHTML(',1)[1].rsplit(')RDHTML";',1)[0]!=html:errors.append('PortalPage.h is stale: run scripts/embed_portal.py')
for name in ['README.md','README.zh-CN.md']:
 text=(root/name).read_text(encoding='utf-8-sig')
 for link in re.findall(r'(?:src="|\]\()((?:docs/)[^"\)]+)',text):
  if not (root/link.split('#')[0]).exists():errors.append(name+': missing asset '+link)
if errors:
 print('\n'.join(errors));sys.exit(1)
print(f'PASS: {len(names)} publication files checked; no known credential patterns, lab addresses or private paths; embedded portal and README assets match.')
