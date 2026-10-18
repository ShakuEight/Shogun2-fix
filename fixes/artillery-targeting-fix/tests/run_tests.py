"""Run the artillery targeting fix (artfix) offline harness (fixes/artillery-targeting-fix/tests/artfix_test.cpp) against build/dinput8.dll.

The harness maps the real empire.retail.dll (no game process), loads the proxy and enters the
game's FUN_1047fd90 on fake fire actions, guns and units, then checks the target record the hook
left behind. Three runs, each in a fresh folder under build/test_artfix/:
  full       fix on (the default at launch)      every check passes (18)
  off        ARTFIX_OFF=1                         every check passes (18)
  selfcheck  20 broken unit links switch it off   every check passes (7)
The proxy is copied as artfix_proxy.dll first (a module named dinput8.dll would clash with the
system dinput8.dll that the proxy itself loads). Exit code 0 = all passed.

python fixes/artillery-targeting-fix/tests/run_tests.py [game_dir]
"""
import os
import re
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
GAME = ARGS[0] if ARGS else r'E:\SteamLibrary\steamapps\common\Total War SHOGUN 2'
BUILD = os.path.join(REPO, 'build')
OUT = os.path.join(BUILD, 'test_artfix')
EXPECT = {'full': 18, 'off': 18, 'selfcheck': 7}

game_dll = os.path.join(GAME, 'empire.retail.dll')
for p in (game_dll, os.path.join(BUILD, 'dinput8.dll'), os.path.join(BUILD, 'artfix_test.exe')):
    if not os.path.exists(p):
        sys.exit('missing: %s (run build.bat; set GAME_DIR for the game folder)' % p)
os.makedirs(OUT, exist_ok=True)

ok_all = True
for mode in ('full', 'off', 'selfcheck'):
    folder = os.path.join(OUT, mode)
    if os.path.isdir(folder):
        shutil.rmtree(folder)          # build output of an earlier test run only
    os.makedirs(folder)
    exe = os.path.join(folder, 'artfix_test.exe')
    proxy = os.path.join(folder, 'artfix_proxy.dll')
    shutil.copy2(os.path.join(BUILD, 'artfix_test.exe'), exe)
    shutil.copy2(os.path.join(BUILD, 'dinput8.dll'), proxy)
    r = subprocess.run([exe, game_dll, proxy, mode, mode], cwd=folder, capture_output=True, text=True, timeout=300)
    text = r.stdout + r.stderr
    with open(os.path.join(OUT, mode + '.txt'), 'w', encoding='utf-8') as f:
        f.write(text)
    m = re.search(r'RESULT (\d+) passed, (\d+) failed', text)
    ok = bool(m) and int(m.group(2)) == 0 and int(m.group(1)) == EXPECT[mode]
    for line in text.splitlines():
        if line.lstrip().startswith('FAIL'):
            print('   ' + line.strip())
    print('artfix %-9s %s  %s' % (mode, 'PASS' if ok else 'FAIL',
                                  m.group(0) if m else 'no RESULT line (exit %d)' % r.returncode))
    ok_all &= ok
print('ALL PASSED' if ok_all else 'SOME TESTS FAILED')
sys.exit(0 if ok_all else 1)
