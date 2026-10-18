"""Run the wall manning fix (wallfix) offline harness
(fixes/wall-manning-fix/tests/wallfix_test.cpp) against build/dinput8.dll.

The harness maps the real empire.retail.dll (no game process), loads the proxy and enters the
hooked game code on a fake unit, move parameters, a form-up action, an A* request and the wall
task's staging test (entered through the game's own call). Five runs, each in a fresh folder under
build/test_wallfix/:
  fix      defaults: the staging march skipped for the wall task only, the game's 'close'
           kept, I off / on; original bytes back at unload                                    (19)
  off      WALLFIX_OFF=1: the game's answer until I                                           (16)
  maxdist  WALLFIX_MAX_DIST=50: skipped at 30 m, kept at 300 m                                 (4)
  probe    WALLFIX_PROBE=1: every site patched, events, slots, stacks, an A* search and a
           staging decision logged, its men sampled, the A* wrapper returns the game's result (38)
  corrupt  one changed byte in a hooked site: nothing installed                                (3)
The proxy is copied as wallfix_proxy.dll first (a module named dinput8.dll would clash with the
system dinput8.dll that the proxy itself loads). Exit code 0 = all passed.

python fixes/wall-manning-fix/tests/run_tests.py [game_dir]
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
OUT = os.path.join(BUILD, 'test_wallfix')
EXPECT = {'fix': 19, 'off': 16, 'maxdist': 4, 'probe': 38, 'corrupt': 3}

game_dll = os.path.join(GAME, 'empire.retail.dll')
for p in (game_dll, os.path.join(BUILD, 'dinput8.dll'), os.path.join(BUILD, 'wallfix_test.exe')):
    if not os.path.exists(p):
        sys.exit('missing: %s (run build.bat; set GAME_DIR for the game folder)' % p)
os.makedirs(OUT, exist_ok=True)

ok_all = True
for mode in EXPECT:
    folder = os.path.join(OUT, mode)
    if os.path.isdir(folder):
        shutil.rmtree(folder)          # build output of an earlier test run only
    os.makedirs(folder)
    exe = os.path.join(folder, 'wallfix_test.exe')
    proxy = os.path.join(folder, 'wallfix_proxy.dll')
    shutil.copy2(os.path.join(BUILD, 'wallfix_test.exe'), exe)
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
    print('wallfix %-8s %s  %s' % (mode, 'PASS' if ok else 'FAIL',
                                    m.group(0) if m else 'no RESULT line (exit %d)' % r.returncode))
    ok_all &= ok
print('ALL PASSED' if ok_all else 'SOME TESTS FAILED')
sys.exit(0 if ok_all else 1)
