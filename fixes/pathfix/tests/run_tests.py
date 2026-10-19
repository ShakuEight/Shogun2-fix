"""Run the pathfix offline harness (fixes/pathfix/tests/hooktest.cpp) against build/dinput8.dll.

The harness maps the real empire.retail.dll (no game process), loads the proxy, enters the game's
own helper functions on fake game objects and checks what the hooks did. Four runs, same criteria
as every version since v20:
  full (twice)  every check passes (124 for v41, 122 for v40)
  geo           geometry-disable path: every check passes (32)
  crash         a deliberate access violation at FUN_1036e710 + 4: the process dies and the run
                folder it wrote holds crash.dmp (the crash filter worked)
  play  (v41)   no PATHFIX_CAPTURE: G acts on I, nothing is recorded without P (own fresh folder)
  pkey  (v41)   capture on: P ends the capture, then I still switches G on and off
  rec   (v42)   no PATHFIX_CAPTURE: P starts / stops recordings, each in its own run folder, one still
                on when the game closes (own fresh folder)
  yari  (v43)   capture on, PATHFIX_TOGGLE_TICKS: the scheduled flips, and Y (the yari wall laid along the
                path) entered through the real slot call site (own fresh folder)
full / geo / crash / pkey run with PATHFIX_CAPTURE=1 (the harness sets it before the proxy loads).
With --with-release (release.bat), two more runs of mode "release" in fresh folders:
  release_dev   the dev DLL switched to variant G through its test export (the reference)
  release_pub   build_release/dinput8.dll, the public release: the same scenarios must give the
                same results, plus: no test exports, only the five G hooks patched, nothing written
The proxy is copied to build/test_pathfix/pathfix_proxy.dll first (a module named dinput8.dll
would clash with the system dinput8.dll that the proxy itself loads). Outputs stay in
build/test_pathfix/. Exit code 0 = all passed.

python fixes/pathfix/tests/run_tests.py [game_dir] [--with-release]
"""
import os
import re
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
WITH_RELEASE = '--with-release' in sys.argv
GAME = ARGS[0] if ARGS else r'E:\SteamLibrary\steamapps\common\Total War SHOGUN 2'
BUILD = os.path.join(REPO, 'build')
OUT = os.path.join(BUILD, 'test_pathfix')
EXPECT = {'full': 136, 'geo': 32, 'play': 18, 'pkey': 9, 'rec': 18, 'yari': 27}   # v43 (v42: full 134; v41: 124; v40: 122)
EXPECT_RELEASE = {'release_dev': 79, 'release_pub': 86}   # release 1.0 (v42): 79 shared + RS1-3 + RF1-4 (v41 package: 71 / 78)
RELEASE_DLL = os.path.join(REPO, 'build_release', 'dinput8.dll')

game_dll = os.path.join(GAME, 'empire.retail.dll')
for p in (game_dll, os.path.join(BUILD, 'dinput8.dll'), os.path.join(BUILD, 'pathfix_hooktest.exe')):
    if not os.path.exists(p):
        sys.exit('missing: %s (run build.bat; set GAME_DIR for the game folder)' % p)
os.makedirs(OUT, exist_ok=True)
exe = os.path.join(OUT, 'hooktest.exe')
proxy = os.path.join(OUT, 'pathfix_proxy.dll')
shutil.copy2(os.path.join(BUILD, 'pathfix_hooktest.exe'), exe)
shutil.copy2(os.path.join(BUILD, 'dinput8.dll'), proxy)


def run(tag, mode, folder=OUT, proxy_src=None):
    e, p = exe, proxy
    if folder != OUT:                      # a fresh folder of its own (play: nothing may be written there)
        if os.path.isdir(folder):
            shutil.rmtree(folder)          # build output of an earlier test run only
        os.makedirs(folder)
        e, p = os.path.join(folder, 'hooktest.exe'), os.path.join(folder, 'pathfix_proxy.dll')
        shutil.copy2(exe, e)
        shutil.copy2(proxy_src or proxy, p)
    r = subprocess.run([e, game_dll, p, tag, mode], cwd=folder, capture_output=True, text=True, timeout=600)
    text = r.stdout + r.stderr
    with open(os.path.join(OUT, tag + '.txt'), 'w', encoding='utf-8') as f:
        f.write(text)
    return r.returncode, text


ok_all = True
for tag, mode in (('full1', 'full'), ('full2', 'full'), ('geo', 'geo'), ('play', 'play'), ('pkey', 'pkey'), ('rec', 'rec'),
                  ('yari', 'yari')):
    code, text = run(tag, mode, os.path.join(OUT, mode) if mode in ('play', 'rec', 'yari') else OUT)
    m = re.search(r'RESULT (\d+) passed, (\d+) failed', text)
    ok = bool(m) and int(m.group(2)) == 0 and (EXPECT[mode] is None or int(m.group(1)) == EXPECT[mode])
    for line in text.splitlines():
        if line.lstrip().startswith('FAIL'):
            print('   ' + line.strip())
    print('%-6s %s  %s' % (tag, 'PASS' if ok else 'FAIL', m.group(0) if m else 'no RESULT line (exit %d)' % code))
    ok_all &= ok

code, text = run('crash', 'crash')
latest = os.path.join(OUT, 'pathfix_runs', 'latest.txt')
run_dir = None
if os.path.exists(latest):
    run_dir = os.path.join(OUT, 'pathfix_runs', open(latest).read().strip())
dump = run_dir and os.path.exists(os.path.join(run_dir, 'crash.dmp'))
crashed = 'K3 crashing now' in text and code != 0
ok = bool(dump) and crashed
print('%-6s %s  process exit 0x%08X, crash.dmp %s' % ('crash', 'PASS' if ok else 'FAIL', code & 0xFFFFFFFF,
                                                     'written in ' + run_dir if dump else 'MISSING'))
ok_all &= ok
if WITH_RELEASE:
    if not os.path.exists(RELEASE_DLL):
        sys.exit('missing: %s (release.bat builds it)' % RELEASE_DLL)
    counts = {}
    for tag, src in (('release_dev', None), ('release_pub', RELEASE_DLL)):
        code, text = run(tag, 'release', os.path.join(OUT, tag), src)
        m = re.search(r'RESULT (\d+) passed, (\d+) failed', text)
        ok = bool(m) and int(m.group(2)) == 0 and (EXPECT_RELEASE[tag] is None or int(m.group(1)) == EXPECT_RELEASE[tag])
        for line in text.splitlines():
            if line.lstrip().startswith('FAIL'):
                print('   ' + line.strip())
        print('%-11s %s  %s' % (tag, 'PASS' if ok else 'FAIL', m.group(0) if m else 'no RESULT line (exit %d)' % code))
        ok_all &= ok
        # equivalence: every behaviour check line of the reference must appear identically in the release run
        counts[tag] = [l.strip() for l in text.splitlines() if l.strip().startswith('PASS') or l.strip().startswith('FAIL')]
    ref = [l for l in counts['release_dev']]
    pub = set(counts['release_pub'])
    missing = [l for l in ref if l not in pub]
    print('%-11s %s  %d of %d reference check lines identical in the release run' % (
        'equivalence', 'PASS' if not missing else 'FAIL', len(ref) - len(missing), len(ref)))
    for l in missing[:10]:
        print('   differs: ' + l)
    ok_all &= not missing
print('ALL PASSED' if ok_all else 'SOME TESTS FAILED')
sys.exit(0 if ok_all else 1)
