# siege-responsiveness-fix (siegefix): history and working notes

Defending units unresponsive during wall assaults. Shared rules, paths and engine knowledge are in
the root CLAUDE.md; the engine decode is `docs/re/path_queue_x86.md`; the design is in
`ARCHITECTURE.md`; numbers in `README.md`. Local only (git-ignored): `analysis/` (queue_wait.py,
by_army.py, side_fields.py, Ghidra decompiles in `analysis/decompiles/`), `docs/research/`
(research log with every run).

## Session 1, 2026-10-18 (dev build, not released)

User report and theory: see ARCHITECTURE.md. Replay `wall siege bug.replay` (vanilla recording,
user defended with 5 units vs 30; moved units on and off the wall late in the battle). User goal:
one DLL with all fixes compatible.

- Probe on the pathfinder update FUN_103aac30 (entry, +0x1E8 queue reached, +0x28B after each
  served request, +0x2D3 queue done), the query FUN_10385fd0, the queueing FUN_1035fb40 and the
  served search FUN_1039a500. Baseline: the queue starves (7 % served, waits to 120 ticks).
- Side: entity +0x2C is an id; FUN_10140150's global is a debug selection (-1). Unit dumps (12
  KB) have no side field. Working chain: world +0x281E4 local (army, alliance) pairs; battle
  alliances / armies / unit lists; soldier group +0x510 = army unit.
- Fix: A = queue's own budget (counter zeroed at +0x1E8) + immediate 500 -> 2000; B = local
  player's queued requests made immediate in the query hook. Shared engine got `Hook_WriteCode`;
  `check_prologues.py` got the missing pathfix sites 0x3DF4D1 / 0x3670A0 and an overlap check.
- Harness `tests/siegefix_test.cpp`: fix 39, off 24, nosync 15, budget 7, range 5, corrupt 6,
  probe 27. All four fixes' harnesses pass with siegefix compiled in.
- Game folder: had no active `dinput8.dll` before the session; the build is left there as
  `dinput8.dll.siegefix`.

Pitfalls met:
- Bash heredocs through the tool turn `\\n` in embedded Python into a real newline: C string
  literals broke twice. Edit C sources with the Edit tool or check `grep -n '^"'` after.
- Paths under `E:/SteamLibrary/.../Total War SHOGUN 2` need quotes in every shell call.
- The first "army" candidate (unit +0x748) was a zone record (0x40-byte array): geographic, not
  a side. Ground truth by start position also misled (attacker groups on the wall line); only
  the battle's own alliance lists were reliable.

Next (not started):
- Live check by the user: order a wall unit off the wall during an assault, fix off and on.
- Larger sieges / other maps; the player as the attacker (B then speeds the climbing units).
- Public release source and the combined package (fixes/README.md, step 6).
