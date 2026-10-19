"""Run the zombie yari wall fix (zombiefix) offline harness
(fixes/zombie-yari-wall-fix/tests/zombiefix_test.cpp) against build/dinput8.dll.

The harness maps the real empire.retail.dll (no game process), loads the proxy and enters the
hooked game code on two fake battle units (unit, group, four men, a squad task in a named state).
Four runs, each in a fresh folder under build/test_zombiefix/:
  fix      defaults: only the rout tick patched; a routing unit's group marked set up but
           without its rout task is re-armed, a group with it, an unmarked one and an empty one
           are left alone; I off (counted only) / on; original bytes back at unload           (29)
  off      ZOMBIEFIX_OFF=1: counted only until I, then re-armed                                (24)
  probe    ZOMBIEFIX_PROBE=1: every site patched, task and soldier machine transitions, pike
           and knockdown outcomes (the wrappers return the game's result), kills and a routing
           unit's contacts, tasks built for a routing unit's group, the fix's re-arm, morale
           changes, stacks, positions, men and raw dumps logged                                (57)
  corrupt  one changed byte in a hooked site: nothing installed                                (3)
The proxy is copied as zombiefix_proxy.dll first (a module named dinput8.dll would clash with the
system dinput8.dll that the proxy itself loads). Exit code 0 = all passed.

python fixes/zombie-yari-wall-fix/tests/run_tests.py [game_dir]
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
OUT = os.path.join(BUILD, 'test_zombiefix')
EXPECT = {'fix': 29, 'off': 24, 'probe': 57, 'corrupt': 3}

game_dll = os.path.join(GAME, 'empire.retail.dll')
for p in (game_dll, os.path.join(BUILD, 'dinput8.dll'), os.path.join(BUILD, 'zombiefix_test.exe')):
    if not os.path.exists(p):
        sys.exit('missing: %s (run build.bat; set GAME_DIR for the game folder)' % p)
os.makedirs(OUT, exist_ok=True)

ok_all = True
for mode in EXPECT:
    folder = os.path.join(OUT, mode)
    if os.path.isdir(folder):
        shutil.rmtree(folder)          # build output of an earlier test run only
    os.makedirs(folder)
    exe = os.path.join(folder, 'zombiefix_test.exe')
    proxy = os.path.join(folder, 'zombiefix_proxy.dll')
    shutil.copy2(os.path.join(BUILD, 'zombiefix_test.exe'), exe)
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
    print('zombiefix %-8s %s  %s' % (mode, 'PASS' if ok else 'FAIL',
                                    m.group(0) if m else 'no RESULT line (exit %d)' % r.returncode))
    ok_all &= ok
print('ALL PASSED' if ok_all else 'SOME TESTS FAILED')
sys.exit(0 if ok_all else 1)
