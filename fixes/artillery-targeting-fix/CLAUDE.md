# artillery-targeting-fix (artfix): history and working notes

Named `aimfix` until 2026-10-18 (Session 2 below). The Session 1 notes keep the names of the
time: `fixes/aimfix/`, `src/aimfix.cpp`, `tests/aimtest.cpp`, `AIMFIX_*`, `aimfix_runs\`,
`AimfixTest*`. Today: `fixes/artillery-targeting-fix/`, `src/artfix.cpp`,
`tests/artfix_test.cpp`, `ARTFIX_*`, `artfix_runs\`, `ArtFixTest*`, log prefix `artfix:`,
CMake option `FIX_ARTILLERY_TARGETING`, entry points `ArtFix_Install` / `ArtFix_Remove`.

Artillery aims at the middle of the target unit. Shared rules, paths and engine knowledge are in
the root CLAUDE.md; the engine decode is `docs/re/artillery_aim_x86.md`; the route and the design
are in `ARCHITECTURE.md`. Local only (git-ignored): `analysis/` (x86q.py capstone helper over the
on-disk DLL, scan scripts, Ghidra decompiles in `analysis/decompiles/`), `docs/research/`.

## Session 1, 2026-10-18 (dev build, not released)

User report: artillery targets the edge of an enemy unit instead of its centre of mass; spread
sometimes lands a shot in the middle anyway. Replay `Artilery_shooting` (3 batteries of 4
Armstrong guns vs 3 x 200 line infantry, 138 s battle). Baseline autorun run 20261017_235443:
loads in 30 s, ends phase 10.

- Static: strings -> engine action state table -> shoot-action members `FUN_103d3xxx`; target
  record getter `FUN_103a6850` (offset, entity, object, transform).
- Probe runs (game folder `aimfix_runs\`): 20261018_000323 (getter calls: only from
  `FUN_10420d00`, once per shot, 60 shots), 20261018_000908 (watchpoint: writer `FUN_1045a8a0` from
  `FUN_1047fd90`), 20261018_001429 (set-target hook: shooter, action vtable `0x115E7D94`, call
  chains), 20261018_002020 / 002354 (shooter class, unit link `[entity+0x3B4]`, unit dumps).
- Fix: entry hook `FUN_1047fd90`, replace the record's soldier by the unit member nearest the
  centroid, artillery only (vtable slot `+0x1B0` = `FUN_1045a2e0`). A/B: 20261018_002946 (off) vs
  003239 (on): median aim 22.0 -> 0.4 m from the centroid, at an end 47 -> 0 %, shots 60 / 60,
  enemy losses 247 -> 320. Final build re-run 20261018_004118 (probe) and a play-mode run: same
  numbers, 676 changes, 0 unit-check failures.
- Harness `tests/aimtest.cpp` (full 15, off 15, selfcheck 6) in `test.bat`; pathfix tests still
  pass with aimfix compiled in.
- Tooling: `tools/autorun/autorun.py --env NAME=VALUE` (repeatable) added for env-switched runs.
- Installed: dev build (pathfix v42 + aimfix) in the game folder; the previous v42 DLL kept as
  `dinput8.dll.v42.bak`.

- U key (user request, same session): toggles the fix flag, focus only, system sounds; refused
  after the self-check switch-off. Harness export `AimfixTestKeys`; checks K1..K3 (full, off),
  S4 (selfcheck): 18 + 18 + 7. Not tried in the live game. The game folder had no `dinput8.dll`
  at that point (the user had renamed the builds to `dinput8.dll.artifix` and
  `dinput8.dll.v42pathfix`), so the new build was not installed.
- First commit of aimfix (dev build only, public release skipped by the user).

Pitfalls met:
- The Bash tool's heredocs turn `\\` into `\` (a C string `"\n"` became a line break, `\a` a bell
  character): edit C sources with the Edit tool, not with Python in a heredoc.
- A local helper script named `dis.py` shadows the stdlib `dis` module and breaks capstone.
- `FUN_10420d00`'s ECX is a launch struct; the record is its stack argument.
- The probe hook on `FUN_103a6850` (30 callers) and on `FUN_1045a8a0` are installed only with
  `AIMFIX_PROBE=1`; play mode has the fix hook alone.

Next (not started, waiting for the user):
- Public release: `release/` source (fix only, always on), release equivalence mode in the
  harness, and packaging of a DLL with both fixes (fixes/README.md: combined name and readme).
- Other engine classes (mortars, rockets, other guns), fire at will, columns / squares, wall
  targets: same code path, not measured.

## Session 2, 2026-10-18 (rename and shared engine; behaviour unchanged)

- User: rename aimfix to "artilery-targeting-fix" (folder spelled `artillery-targeting-fix`).
  `git mv` of the folder and files; identifiers, env variables, run folder and log prefix renamed
  to `artfix` (list at the top of this file). `stack_offsets_art.h` regenerated with
  `tools/re/stack_calc.py --prefix ART` (same offsets as the old AIM header).
- Shared infrastructure moved into `proxy/` (user decision "full move now", made when the
  fire-by-rank fix was started): `hook.h/.cpp` (HookDef / HookSlot, byte check, trampoline pool,
  patch, restore, HOOK_DETOUR), `gamemem.h/.cpp` (guarded reads and writes, GameTickAt),
  `keys.h/.cpp` (focus test, shared U poll thread through Keys_Watch, tones). artfix has no
  private hook engine, reads or key thread any more.
- Harness unchanged apart from names: full 18, off 18, selfcheck 7. The old probe folders in the
  game directory stay `aimfix_runs\`; new probe runs write `artfix_runs\`. CSV names stay `aim_*`.
