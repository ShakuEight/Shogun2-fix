"""Run the single unit group fix (groupfix) offline harness
(fixes/single-unit-group-fix/tests/groupfix_test.cpp) against build/dinput8.dll.

The harness maps the real empire.retail.dll (no game process), loads the proxy and runs the game's
own group-of-the-selection function (FUN_10f89a40) on a fake selection manager with 0, 1 and 2
selected units; the probe-only sites are stubbed after their stolen bytes and entered directly.
Four runs, each in a fresh folder under build/test_groupfix/:
  fix      defaults: the count immediate 0, the dev log hook patched, no probe site; one unit is
           grouped, two are, none is not; I off: one unit refused again; I on again; original
           bytes back at unload                                                                 (47)
  off      GROUPFIX_OFF=1: one unit refused until I, then grouped                               (38)
  probe    GROUPFIX_PROBE=1: every site patched; groupings and each probe site counted and
           logged (CSV checked)                                                                 (56)
  corrupt  one changed byte in the fix site: nothing installed                                  (3)
The proxy is copied as groupfix_proxy.dll first (a module named dinput8.dll would clash with the
system dinput8.dll that the proxy itself loads). Exit code 0 = all passed.

python fixes/single-unit-group-fix/tests/run_tests.py [game_dir]
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
OUT = os.path.join(BUILD, 'test_groupfix')
EXPECT = {'fix': 47, 'off': 38, 'probe': 56, 'corrupt': 3}

game_dll = os.path.join(GAME, 'empire.retail.dll')
for p in (game_dll, os.path.join(BUILD, 'dinput8.dll'), os.path.join(BUILD, 'groupfix_test.exe')):
    if not os.path.exists(p):
        sys.exit('missing: %s (run build.bat; set GAME_DIR for the game folder)' % p)
os.makedirs(OUT, exist_ok=True)

ok_all = True
for mode in EXPECT:
    folder = os.path.join(OUT, mode)
    if os.path.isdir(folder):
        shutil.rmtree(folder)          # build output of an earlier test run only
    os.makedirs(folder)
    exe = os.path.join(folder, 'groupfix_test.exe')
    proxy = os.path.join(folder, 'groupfix_proxy.dll')
    shutil.copy2(os.path.join(BUILD, 'groupfix_test.exe'), exe)
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
    print('groupfix %-8s %s  %s' % (mode, 'PASS' if ok else 'FAIL',
                                    m.group(0) if m else 'no RESULT line (exit %d)' % r.returncode))
    ok_all &= ok
print('ALL PASSED' if ok_all else 'SOME TESTS FAILED')
sys.exit(0 if ok_all else 1)
