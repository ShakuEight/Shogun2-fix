# fire-by-rank-fix (fbrfix): history and working notes

Matchlock fire by rank: slow start, and the Hattori / Ikko Ikki gun units stop after three
volleys. Shared rules, paths and engine knowledge are in the root CLAUDE.md; the engine decode is
`docs/re/missile_fire_x86.md`; the route and the design are in `ARCHITECTURE.md`. Local only
(git-ignored): `analysis/` (x86q.py capstone helper, states.py, extract_db.py, diff_units.py,
metrics.py, group_wait.py; Ghidra decompiles in `analysis/decompiles/`), `docs/research/`
(research log with every run), `data/db/` (RPFM table extracts).

## Session 1, 2026-10-18 (dev build, not released)

User report: matchlock target acquisition slow, worse with fire by rank (FBR); Hattori and
Ikko Ikki FBR fires 3 ranks then never reloads; theory: their special officer model. Bug is vanilla
(user); the fix must work with vanilla and Workshop mods; a data fault is still fixed DLL-side.
User decisions: full move of shared code into `proxy/` (done first, see the artillery fix's
CLAUDE.md, Session 2), fix folder name `fire-by-rank-fix`.

- RPFM 4.7.100 restored from the Recycle Bin to `E:\_SHOGUN\_rpfm\` (rpfm_cli.exe + schema_sho2).
  Data: only the Hattori ashigaru / samurai and Ikko Ikki ashigaru matchlock units have
  `officer_katana` (battle_personalities: katana animations, no gun); the rest `officer_teppo`.
- Static: state machine framework, runner `FUN_11374cf0`, row fire task (`0x115C0564`); probe
  hooks on the runner entry, transition (+0x1B), order functions A/B.
- Probe runs (game folder `fbrfix_runs\`): 105521 / 105756 / 110027 (generic / ikko / hattori:
  FBR units run the GROUP fire task `0x115C1048`, not the row task; first group wait ~10 s;
  Hattori hangs in the group-0 wait), 110631 / 110913 (group dumps: officer soldier 0, class
  `0x115DBFB4`, never completes the kneeling reload order after group 0's volley; groups capped at
  3 while 4-5 ranks deep), 111456 / 111741 / 112016 (raw soldier dumps: +0x48C 8 gun men and gun
  officer, 0x10 sword officer).
- Fix: hooks `FUN_1017b980` (group test: shooters loaded and ready, or loaded + 1.5 s grace;
  officer class excluded) and `FUN_10188290` (no ready / reload order for men outside the unit's
  most common +0x48C). New shared macro `HOOK_DETOUR_RET` (proxy/hook.h) for answering a function.
  Fix-on runs 112837 / 113119 / 113350, repeat 114409 (identical): FBR delay 9.5..16.4 -> 2.1..8.1
  s, first wait ~10 -> 1.6 s, Hattori hang 20 s -> none, volleys 3 -> 6.
- Option `FBRFIX_ALL_RANKS=1` (raise the 3-group cap to the ranks present): runs 113857 (ikko),
  114128 (hattori): more volleys, same shots within noise; off by default, user to decide.
- Harness `tests/fbrfix_test.cpp`: fix 20, off 17, selfcheck 5, allranks 4, probe 26.
- Installed in the game folder during the session as `dinput8.dll`; at the end renamed to
  `dinput8.dll.fbrfix` (the folder had no active `dinput8.dll` before the session).

Pitfalls met:
- `owner_vt` in the probe CSVs is a 7-digit RVA (`15C1048`), not 6.
- A harness scene must not hand the game's own function a layout it cannot survive (a null
  group table crashed `FUN_1017b980` when the fix declined): use a group count of 0.
- `python -I` ignores the script folder: run the local analysis scripts that import x86q.py
  without `-I` from `analysis/`.

Next (not started, waiting for the user):
- Decide the 3-group cap option and the grace time; then a public release source and the
  combined package (fixes/README.md).
- Matchlock samurai / warrior monks, longer battles (the replays end 30..50 s after contact).
