"""Package the public release DLL: release/<name>-<version>/ with dinput8.dll, README.txt and
SHA256.txt, LICENSE and NOTICE, plus release/<name>-<version>.zip. An existing package of the same name is moved to
release/_previous/<name>_<stamp>/ first, never overwritten. Called by release.bat.

python tools/release/package.py <version> <name> <readme.txt>
"""
import datetime
import hashlib
import os
import shutil
import sys
import zipfile

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
VERSION = sys.argv[1] if len(sys.argv) > 1 else '1.0'
NAME = '%s-%s' % (sys.argv[2] if len(sys.argv) > 2 else 'Shogun2-PathFix', VERSION)
DLL = os.path.join(REPO, 'build_release', 'dinput8.dll')
README = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else os.path.join(REPO, 'fixes', 'pathfix', 'release', 'README.txt')
OUTROOT = os.path.join(REPO, 'release')
OUT = os.path.join(OUTROOT, NAME)
ZIP = OUT + '.zip'

if not os.path.exists(DLL):
    sys.exit('missing %s: run release.bat' % DLL)
os.makedirs(OUTROOT, exist_ok=True)
stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
for old in (OUT, ZIP):
    if os.path.exists(old):
        prev = os.path.join(OUTROOT, '_previous')
        os.makedirs(prev, exist_ok=True)
        base = os.path.basename(old)
        dst = os.path.join(prev, base.replace('.zip', '') + '_' + stamp + ('.zip' if old.endswith('.zip') else ''))
        os.rename(old, dst)
        print('previous package moved to', dst)
os.makedirs(OUT)
shutil.copy2(DLL, os.path.join(OUT, 'dinput8.dll'))
text = open(README, encoding='utf-8').read().replace('\r\n', '\n').replace('\n', '\r\n')
with open(os.path.join(OUT, 'README.txt'), 'w', encoding='utf-8', newline='') as f:
    f.write(text)
for legal in ('LICENSE', 'NOTICE'):        # Apache-2.0: every distribution carries the licence and the notice
    shutil.copy2(os.path.join(REPO, legal), os.path.join(OUT, legal))
digest = hashlib.sha256(open(DLL, 'rb').read()).hexdigest().upper()
with open(os.path.join(OUT, 'SHA256.txt'), 'w', encoding='ascii', newline='') as f:
    f.write('%s  dinput8.dll\r\n' % digest)
with zipfile.ZipFile(ZIP, 'w', zipfile.ZIP_DEFLATED) as z:
    for fn in ('dinput8.dll', 'README.txt', 'SHA256.txt', 'LICENSE', 'NOTICE'):
        z.write(os.path.join(OUT, fn), arcname=fn)
print('package  %s' % OUT)
print('zip      %s (%d bytes)' % (ZIP, os.path.getsize(ZIP)))
print('sha256   %s  dinput8.dll (%d bytes)' % (digest, os.path.getsize(DLL)))
