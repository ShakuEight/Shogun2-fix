"""Run the guard mode ranged fix (guardfix) offline harness
(fixes/guard-mode-ranged-fix/tests/guardfix_test.cpp) against build/dinput8.dll.

The harness maps the real empire.retail.dll (no game process), loads the proxy and calls the
seven redirected missile attack task tests on fake battle units and fake missile attack tasks.
Four runs, each in a fresh folder under build/test_guardfix/:
  fix      defaults: the seven calls redirected and fire at will's choice hooked (a guard missile
           unit leaves shattered enemies out); a guard unit's intercept left to the game for a
           target never in range, refused once it was in range; shattered targets invalid for a
           guard unit, broken ones not; an intercept under way stopped; units not in guard mode
           and the naval task untouched; I off / on; the self-check; original bytes at unload
  off      GUARDFIX_OFF=1: the game's answers until I, then the fix
  probe    GUARDFIX_PROBE=1: the transition site too; transition, call, position and order rows
  corrupt  one changed byte in a call site: nothing installed
The proxy is copied as guardfix_proxy.dll first (a module named dinput8.dll would clash with the
system dinput8.dll that the proxy itself loads). Exit code 0 = all passed.

python fixes/guard-mode-ranged-fix/tests/run_tests.py [game_dir]
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
OUT = os.path.join(BUILD, 'test_guardfix')
EXPECT = {'fix': 83, 'off': 57, 'probe': 32, 'corrupt': 3}

game_dll = os.path.join(GAME, 'empire.retail.dll')
for p in (game_dll, os.path.join(BUILD, 'dinput8.dll'), os.path.join(BUILD, 'guardfix_test.exe')):
    if not os.path.exists(p):
        sys.exit('missing: %s (run build.bat; set GAME_DIR for the game folder)' % p)
os.makedirs(OUT, exist_ok=True)

ok_all = True
for mode in EXPECT:
    folder = os.path.join(OUT, mode)
    if os.path.isdir(folder):
        shutil.rmtree(folder)          # build output of an earlier test run only
    os.makedirs(folder)
    exe = os.path.join(folder, 'guardfix_test.exe')
    proxy = os.path.join(folder, 'guardfix_proxy.dll')
    shutil.copy2(os.path.join(BUILD, 'guardfix_test.exe'), exe)
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
    print('guardfix %-8s %s  %s' % (mode, 'PASS' if ok else 'FAIL',
                                   m.group(0) if m else 'no RESULT line (exit %d)' % r.returncode))
    ok_all &= ok
print('ALL PASSED' if ok_all else 'SOME TESTS FAILED')
sys.exit(0 if ok_all else 1)
