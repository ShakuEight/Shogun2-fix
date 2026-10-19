"""Run the wall collapse fix (collapsefix) offline harness
(fixes/wall-collapse-fix/tests/collapsefix_test.cpp) against build/dinput8.dll.

The harness maps the real empire.retail.dll (no game process), loads the proxy and enters the
hooked game code on fake buildings and pieces (a gate structure and its door, a wall piece), the
gates-open flag and the battle clock and phase. The battle start's gate loop and per-piece gate
damage are the game's own code; the damage function they end in is a stub.
Four runs, each in a fresh folder under build/test_collapsefix/:
  fix      defaults: only the gate loop patched; gates open to all: the intact door is kept
           (counted), the gate structure untouched, nothing when the gates are not open,
           projectile damage still destroys a wall piece and a door; I off: the game's code
           destroys the door; I on again; original bytes back at unload                        (30)
  off      COLLAPSEFIX_OFF=1: the door is destroyed until I, then kept                         (23)
  probe    COLLAPSEFIX_PROBE=1: every site patched; level apply, damage set, damage added and
           fort damage wrapped (the game function ran, its result came back), collapse effects,
           phase change (Start Battle tick), hit point sets in the window, the gate loop and
           the sampler logged (CSV files checked)                                              (30)
  corrupt  one changed byte in a hooked site: nothing installed                                (3)
The proxy is copied as collapsefix_proxy.dll first (a module named dinput8.dll would clash with the
system dinput8.dll that the proxy itself loads). Exit code 0 = all passed.

python fixes/wall-collapse-fix/tests/run_tests.py [game_dir]
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
OUT = os.path.join(BUILD, 'test_collapsefix')
EXPECT = {'fix': 30, 'off': 23, 'probe': 30, 'corrupt': 3}

game_dll = os.path.join(GAME, 'empire.retail.dll')
for p in (game_dll, os.path.join(BUILD, 'dinput8.dll'), os.path.join(BUILD, 'collapsefix_test.exe')):
    if not os.path.exists(p):
        sys.exit('missing: %s (run build.bat; set GAME_DIR for the game folder)' % p)
os.makedirs(OUT, exist_ok=True)

ok_all = True
for mode in EXPECT:
    folder = os.path.join(OUT, mode)
    if os.path.isdir(folder):
        shutil.rmtree(folder)          # build output of an earlier test run only
    os.makedirs(folder)
    exe = os.path.join(folder, 'collapsefix_test.exe')
    proxy = os.path.join(folder, 'collapsefix_proxy.dll')
    shutil.copy2(os.path.join(BUILD, 'collapsefix_test.exe'), exe)
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
    print('collapsefix %-8s %s  %s' % (mode, 'PASS' if ok else 'FAIL',
                                    m.group(0) if m else 'no RESULT line (exit %d)' % r.returncode))
    ok_all &= ok
print('ALL PASSED' if ok_all else 'SOME TESTS FAILED')
sys.exit(0 if ok_all else 1)
