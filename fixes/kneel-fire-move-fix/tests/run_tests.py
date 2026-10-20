"""Run the kneel fire move fix (kneelfix) offline harness
(fixes/kneel-fire-move-fix/tests/kneelfix_test.cpp) against build/dinput8.dll.

The harness maps the real empire.retail.dll (no game process), loads the proxy and calls the
eight redirected CALLs of the game's can-move test FUN_1015c3f0 on fake units.
Five runs, each in a fresh folder under build/test_kneelfix/:
  fix           defaults: every site redirected; units without kneel fire keep the game's answer;
                a kneeling unit moves unless something else refuses it; I off / on; the self-check;
                original bytes at unload
  off           KNEELFIX_OFF=1: the game's answers until I, then the fix
  probe         KNEELFIX_PROBE=1: kneel_calls.csv rows
  corrupt       one changed byte in a call site: nothing installed
  corrupt_head  one changed byte in FUN_1015c3f0 at the fix's entry point: nothing installed
The proxy is copied as kneelfix_proxy.dll first (a module named dinput8.dll would clash with the
system dinput8.dll that the proxy itself loads). Exit code 0 = all passed.

python fixes/kneel-fire-move-fix/tests/run_tests.py [game_dir]
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
OUT = os.path.join(BUILD, 'test_kneelfix')
EXPECT = {'fix': 20, 'off': 6, 'probe': 5, 'corrupt': 3, 'corrupt_head': 3}

game_dll = os.path.join(GAME, 'empire.retail.dll')
for p in (game_dll, os.path.join(BUILD, 'dinput8.dll'), os.path.join(BUILD, 'kneelfix_test.exe')):
    if not os.path.exists(p):
        sys.exit('missing: %s (run build.bat; set GAME_DIR for the game folder)' % p)
os.makedirs(OUT, exist_ok=True)

ok_all = True
for mode in EXPECT:
    folder = os.path.join(OUT, mode)
    if os.path.isdir(folder):
        shutil.rmtree(folder)          # build output of an earlier test run only
    os.makedirs(folder)
    exe = os.path.join(folder, 'kneelfix_test.exe')
    proxy = os.path.join(folder, 'kneelfix_proxy.dll')
    shutil.copy2(os.path.join(BUILD, 'kneelfix_test.exe'), exe)
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
    print('kneelfix %-12s %s  %s' % (mode, 'PASS' if ok else 'FAIL',
                                   m.group(0) if m else 'no RESULT line (exit %d)' % r.returncode))
    ok_all &= ok
print('ALL PASSED' if ok_all else 'SOME TESTS FAILED')
sys.exit(0 if ok_all else 1)
