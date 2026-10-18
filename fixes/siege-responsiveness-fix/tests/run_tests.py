"""Run the siege responsiveness fix (siegefix) offline harness
(fixes/siege-responsiveness-fix/tests/siegefix_test.cpp) against build/dinput8.dll.

The harness maps the real empire.retail.dll (no game process), loads the proxy and enters the
hooked game code on a fake pathfinder, entities and path requests. Seven runs, each in a fresh
folder under build/test_siegefix/:
  fix      defaults: budget 2000, the queue's own budget, incremental cap untouched, the local
           player's queued requests made immediate (nothing else), J                       (39)
  off      SIEGEFIX_OFF=1: the game's 500, no request or counter changed until J           (24)
  nosync   SIEGEFIX_PLAYER_SYNC=0 SIEGEFIX_QUEUE_OWN=0: budget raised only                 (15)
  budget   SIEGEFIX_BUDGET=1234 SIEGEFIX_INCR=300, J, restored at unload                   (7)
  range    out-of-range values: the defaults                                               (5)
  corrupt  one changed byte in the budget compare: nothing installed                       (6)
  probe    SIEGEFIX_PROBE=1 SIEGEFIX_OFF=1: one pathfinder tick, CSV files                 (27)
The proxy is copied as siegefix_proxy.dll first (a module named dinput8.dll would clash with the
system dinput8.dll that the proxy itself loads). Exit code 0 = all passed.

python fixes/siege-responsiveness-fix/tests/run_tests.py [game_dir]
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
OUT = os.path.join(BUILD, 'test_siegefix')
EXPECT = {'fix': 39, 'off': 24, 'nosync': 15, 'budget': 7, 'range': 5, 'corrupt': 6, 'probe': 27}

game_dll = os.path.join(GAME, 'empire.retail.dll')
for p in (game_dll, os.path.join(BUILD, 'dinput8.dll'), os.path.join(BUILD, 'siegefix_test.exe')):
    if not os.path.exists(p):
        sys.exit('missing: %s (run build.bat; set GAME_DIR for the game folder)' % p)
os.makedirs(OUT, exist_ok=True)

ok_all = True
for mode in EXPECT:
    folder = os.path.join(OUT, mode)
    if os.path.isdir(folder):
        shutil.rmtree(folder)          # build output of an earlier test run only
    os.makedirs(folder)
    exe = os.path.join(folder, 'siegefix_test.exe')
    proxy = os.path.join(folder, 'siegefix_proxy.dll')
    shutil.copy2(os.path.join(BUILD, 'siegefix_test.exe'), exe)
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
    print('siegefix %-8s %s  %s' % (mode, 'PASS' if ok else 'FAIL',
                                    m.group(0) if m else 'no RESULT line (exit %d)' % r.returncode))
    ok_all &= ok
print('ALL PASSED' if ok_all else 'SOME TESTS FAILED')
sys.exit(0 if ok_all else 1)
