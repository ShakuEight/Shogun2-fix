# zombie-yari-wall-fix (zombiefix): history and working notes

A yari wall unit that breaks keeps standing and killing. Shared rules, paths and engine knowledge
are in the root CLAUDE.md; the engine decode is `docs/re/rout_x86.md`; the design is in
`ARCHITECTURE.md`; numbers in `README.md`. Local only (git-ignored): `analysis/` (zombie_timeline.py,
kills_after_rout.py, find_kills_field.py, scan_disp.py, find_ptr.py, state_vt.py, disasm.py; Ghidra
decompiles in `analysis/decompiles/`), `data/`, `docs/research/` (research log).

## Session 1, 2026-10-19 (dev build, not released)

User report: see ARCHITECTURE.md. Replay `zombie_yariwall.replay` (09:53), "no other fix was
active" per the user, but the game folder log of the recording shows the dev build of
2026-10-18 23:08 loaded (artfix, fbrfix, siegefix, wallfix on, pathfix off; that build is the
game folder's `dinput8.dll.wallfix`, SHA-256 451ACC90...). User decisions: probe replay clean
(chosen first), folder name `zombie-yari-wall-fix`; mid-session: "a zombie is a unit whose kill
count goes up after it broke".

- Static: morale block (+0xCC4: level +0xCE8, rout state +0xCEC, rally +0xD10) from the script
  conditions `FUN_101b3f60`; unit groups (+0xB90 list, +0xB9C main); soldier links (+0x214 unit,
  +0x3B4 group); kill outcomes `FUN_10435410` (pike) and `FUN_10430cf0` (knockdown); interrupt
  machine at unit +0xE78 with the rout state (enter `FUN_10170cc0`, tick `FUN_101bd3b0`, group
  set-up `FUN_101a9bf0`, byte group +0x2781, rout task 0x115E01F4).
- Probe runs (game folder `zombiefix_runs\`): 102709 (probe-only build: desync at 65.6 s, no
  zombie case judged); 103038 and 103837 (dev build with all fixes at their defaults + probe: in
  sync, identical; 103837 with 0x2010-byte unit dumps): kill count found at unit +0xD88; zombies
  163E4DA0 +9, 16440CB0 +19, 16619008 +34, 16669B90 +40 kills after breaking (the last two = the
  user's two); they broke mid-charge, a melee attack task (0x115C0DFC, from the deferred command
  `FUN_103d0470`) started in the rout tick, then the default re-form task (0x115C0644 from
  `FUN_101b6490`) took the stack and the rout task never came back.
- Fix: entry hook on the rout tick `FUN_101bd3b0` (7 bytes `56 57 8B F9 8B 47 10`,
  docs/re/prologues_zombiefix_fix.txt): a live group marked set up (+0x2781 = 1) without a rout
  task on its stack gets the byte cleared, the game's loop sets it up again.
- Runs 104606 (fix off, with the task constructor probes: in sync, identical to 103038; the
  attack task at the rout tick for exactly the four zombies) and 104910 (fix on: 4 re-arms, one
  per group, all four ran; desync from tick 657, right after the first re-arm).
- Kill count added to `zombie_pos.csv` (every 0.5 s); final runs 105338 (fix off, in sync) and
  105619 (fix on): zombies 4 -> 0, kills more than 5 s after breaking 94 -> 0, the four re-armed
  groups (stack empty when found) ran 21..66 m in 20 s and kept 56 / 36 / 89 / 87 men (off:
  12 / 0 / 16 / 14). Table in README.md.
- Harness `tests/zombiefix_test.cpp`: fix 23, off 18, probe 46, corrupt 3.
- Game folder: had no active `dinput8.dll` at the start and has none at the end. Session builds
  were installed as `dinput8.dll` for the runs (intermediate copies removed); the final dev
  build (all fixes + zombiefix, SHA-256 0711A050...) sits as `dinput8.dll.zombiefix`. Probe runs
  in `zombiefix_runs\` take 1.2 GB.

Pitfalls met:
- A replay recorded with dev fixes on must be replayed with them on: the user may not know which
  fixes were loaded; the game folder `pathfix_log.txt` of the recording tells (it is rewritten at
  every launch, so read it before the first run).
- The autorun driver ends the run when the battle is decided, and the replay does not continue
  after that either (`--linger` only shows the same tick).
- Bash heredocs turn `\n` and `\t` inside embedded Python into real control characters (also in
  wallfix's notes); write edit scripts to the scratchpad with the Write tool.
- PowerShell `Set-Content -Encoding utf8` writes a BOM; strip it.
- The probe's transition log is about 120 MB per run of this replay.

Next (not started):
- User live check: break yari wall units in the middle of a charge, fix on and off (I).
- Other machines with a rout interrupt state (vtables 0x115B5180, 0x115B9368, 0x115DA7C4: naval,
  siege?) have empty ticks; the fix only covers the land unit's machine.
- Public release source and the combined package (fixes/README.md).
