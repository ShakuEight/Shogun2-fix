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

## Session 1, part 2, 2026-10-18 (early reload)

User question: with fire by rank, does rank 1 reload while ranks 2 and 3 fire, so that it can
fire again at once when the rotation comes back? Checked offline on the part-1 probe runs
(fbr_rowdump.csv): the reload time is set at the shot and frozen until the fire-completed exit
`FUN_101be8f0` gives the group its reload orders; that wait (`FUN_1017b9d0`) holds for men who
never get a shot (Hattori: 24 of 40 shot at once, 9 s idle). With 3 groups of ~6 s group 0 still
had 7 to 15 s of reload left at the wrap.

- Fix lever E (default on): hook `FUN_1017b9d0` (`53 56 57 8B F1`, docs/re/prologues_fbrfix_done.txt),
  give each fired shooter whose fire order is done the game's own reload order at once (calls
  `FUN_10188290` with flag 1), answer the test for him; skip the exit's second order to him
  (turn key table). `FBRFIX_EARLY_RELOAD=0` off. Lever F `FBRFIX_FIRE_GRACE=<ticks>` (default
  0): end the turn N ticks after the group's last shot.
- Probe: `counting` event in fbr_reload.csv (reload started); the slot-based reload sampling gave
  false `fired` rows after casualties, now tracked per soldier (`who[]`); compare_runs.py drops
  shots under 15 s apart for older runs and has idle / idle90 / cycle / wrapleft / noshot /
  early / cut columns.
- Runs (game folder `fbrfix_runs\`): 143100 Hattori A (E off; identical to 112837), 143341 /
  143623 / 143853 E, 144128 / 144410 / 144640 E+F, 144916 / 145159 / 145429 E+F+R, 145724 /
  150006 / 150237 E+R (R = ALL_RANKS). Idle90 7..9 -> 2.5 s (FAW 2.2), Hattori shot-to-shot
  29..32 -> 23..24 s (FAW 25..26); shots within replay drift (FAW totals 114..153 in Hattori);
  E+R Hattori +30 % shots; F no gain. Table in README.md.
- Harness: fix 28, off 22, selfcheck 5, allranks 4, firegrace 5, probe 27.
- Game folder restored to the user's active build (part 1, was `dinput8.dll.fbrfix1` during the
  runs); the part-2 build sits as `dinput8.dll.fbrfix2`, the run build as `dinput8.dll.bak`.

Pitfalls met:
- A fire-by-rank task restart (START -> REFORM, the unit re-forms) begins at group 0 even if it
  just fired: the long end-of-battle waits in these runs, the game's own behaviour.
- The probe's per-slot arrays shift with casualties (see above).

## Session 1, part 3, 2026-10-18 (every rank, quorum)

User decisions: every rank fires (no cap of 3); a group fires 1 s after 75 % of its men are
ready, instead of waiting for the last reloader.

- `FBRFIX_ALL_RANKS` now defaults on (`=0` keeps 3 groups).
- Quorum in `HandleGroupReady`: shooters loaded and ready (loaded counts as ready after the
  grace) >= `FBRFIX_QUORUM` % (75) with men still reloading -> per-wait timer (`WaitRec.qfirst`),
  fire after `FBRFIX_QUORUM_WAIT` ticks (10). Fourth hook `FUN_101883f0` (fire order per man,
  task slot +0x80, only ref the vtable; 8 stolen bytes `83 EC 50 56 8B 74 24 58`,
  docs/re/prologues_fbrfix_fire.txt): no fire order for a man with reload time > 0, marked
  sat out (`EarlyRec.satTask/satTick`); the fire-completed hook and the exit's order skip him.
- Runs 152601 / 152843 / 153113 (hattori / ikkoikki / generic, defaults, probe): Hattori one
  quorum volley (3 of 39 reloading after a task restart; hang 17.8 s -> none, volleys 4 -> 5);
  Ikko Ikki and generic identical to E+R (no straggler case: whole groups reload together).
- Harness: fix 35, off 26, selfcheck 5, cap3 5 (replaces allranks), firegrace 5, probe 28.
- Game folder: the user had swapped in the part-2 build (`dinput8.dll.fbrfix2`) and removed the
  `.bak` before part 3; for the runs it was set aside as `.fbrfix2` again.

## Session 1, part 4, 2026-10-18 (front rank first, one final fix)

User question: what happens on a re-form into a different depth; which rank fires when it is
rank 5's turn but rank 1 has reloaded. Static: the group table is filled from the formation by
`FUN_101540f0` (task slot +0x78 per array position) on the task's first tick (slot +4
`FUN_101b6d80`) and on REFORM enter; START byte +0x3c; ADVANCE finishes the task when slot +0x74
`FUN_10173550` finds groups 0..2 thinned out. User decisions: another ready rank fires when the
current one cannot; the front-most rank has priority; every rank (cap 8 -> 64); "group all the
parts into one final fix" (chosen: one coherent fix in code and docs, no squash).

- Front rank first in `HandleGroupReady` (per-group pass; eligible = 75 % loaded; writes +0x40;
  `FBRFIX_FRONT_FIRST=0` off). Runs 160744 / 161038 / 161331 (reform_deep: front / strict /
  off), 161624.. 163711 (the three replays x front / strict / off).
- Found in those runs: (1) loaded rear ranks without a line of fire took the turn again and
  again (Ikko: group 2 x10, no shot) -> per-group line-of-fire memory (`LosRec`, a group may
  jump only if >= 25 % of its men fired on its last turn; table signature resets it); runs
  165131.. 165936. (2) a front rank taking the turn from its kneeling reload pose: one man aimed
  22 s, the turn never ended (Hattori unit 2) -> the switch gives the group the wait entry's
  ready orders (slot +0x84 flag 0) and answers "not yet" for that tick; `FBRFIX_FIRE_GRACE`
  default 50 (5 s after the last shot) as a safety net. Final runs 170430 / 170711 / 171004 /
  171235: shots off -> on Hattori 153 -> 194, Ikko 161 -> 206, generic 230 -> 244, reform
  104 -> 129 (strict order 121); front vs strict in Hattori 194 / 240 (FAW 114 in both: maybe
  real, maybe drift; open).
- Harness bug found: fake soldiers were 0x400 bytes but the class sits at +0x48C (heap overflow
  since part 1, random crashes once more scenes ran); now 0x600. 50 repeated runs clean.
- Harness: fix 42, off 26, selfcheck 5, cap3 6, firegrace 5, probe 28.
- Docs rewritten as one fix (README, ARCHITECTURE, source header); this file keeps the parts.
- Game folder: the user had installed the part-3 build as `dinput8.dll`; kept as
  `dinput8.dll.fbrfix3` during the runs.

Next (not started, waiting for the user):
- Hattori front-first vs strict (194 / 240) with more runs or a longer battle; Ikko Ikki rear
  ranks without a line of fire; the depleted test runs less often with front-first; public
  release source.
- Decide the 3-group cap option and the grace time; then a public release source and the
  combined package (fixes/README.md).
- Matchlock samurai / warrior monks, longer battles (the replays end 30..50 s after contact).
