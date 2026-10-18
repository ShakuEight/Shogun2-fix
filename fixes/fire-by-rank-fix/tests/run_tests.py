"""Run the fire-by-rank fix (fbrfix) offline harness (fixes/fire-by-rank-fix/tests/fbrfix_test.cpp)
against build/dinput8.dll.

The harness maps the real empire.retail.dll (no game process), loads the proxy and enters the
hooked game code on fake group tasks, units and soldiers (the fix), and on fake machines and
states (the probe). Six runs, each in a fresh folder under build/test_fbrfix/:
  fix        fix on: officer not waited for, reloading men are (under the quorum), grace time,
             order skip, Y, early reload orders, 75 % quorum, no fire order for men
             reloading, a 5-rank unit gets 5 groups, front rank first, 12 ranks              (42)
  off        FBRFIX_OFF=1: the game's own answers until Y                                    (26)
  selfcheck  20 broken group tasks switch the fix off, Y refused                             (5)
  cap3       FBRFIX_ALL_RANKS=0 FBRFIX_QUORUM=0 FBRFIX_FRONT_FIRST=0: 3 groups kept, one
             reloader holds a group, the turn is not moved                                   (6)
  firegrace  FBRFIX_EARLY_RELOAD=0 FBRFIX_FIRE_GRACE=20: a turn ends 2 s after the last shot (5)
  probe      FBRFIX_PROBE=1: transitions, reloads, orders, CSV files                         (28)
The proxy is copied as fbrfix_proxy.dll first (a module named dinput8.dll would clash with the
system dinput8.dll that the proxy itself loads). Exit code 0 = all passed.

python fixes/fire-by-rank-fix/tests/run_tests.py [game_dir]
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
OUT = os.path.join(BUILD, 'test_fbrfix')
EXPECT = {'fix': 42, 'off': 26, 'selfcheck': 5, 'cap3': 6, 'firegrace': 5, 'probe': 28}

game_dll = os.path.join(GAME, 'empire.retail.dll')
for p in (game_dll, os.path.join(BUILD, 'dinput8.dll'), os.path.join(BUILD, 'fbrfix_test.exe')):
    if not os.path.exists(p):
        sys.exit('missing: %s (run build.bat; set GAME_DIR for the game folder)' % p)
os.makedirs(OUT, exist_ok=True)

ok_all = True
for mode in ('fix', 'off', 'selfcheck', 'cap3', 'firegrace', 'probe'):
    folder = os.path.join(OUT, mode)
    if os.path.isdir(folder):
        shutil.rmtree(folder)          # build output of an earlier test run only
    os.makedirs(folder)
    exe = os.path.join(folder, 'fbrfix_test.exe')
    proxy = os.path.join(folder, 'fbrfix_proxy.dll')
    shutil.copy2(os.path.join(BUILD, 'fbrfix_test.exe'), exe)
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
    print('fbrfix %-9s %s  %s' % (mode, 'PASS' if ok else 'FAIL',
                                  m.group(0) if m else 'no RESULT line (exit %d)' % r.returncode))
    ok_all &= ok
print('ALL PASSED' if ok_all else 'SOME TESTS FAILED')
sys.exit(0 if ok_all else 1)
