# guard-mode-ranged-fix

**Problem.** A unit in guard mode holds its position, and a melee unit in guard mode does not
pursue. A missile unit (bows, matchlocks) in guard mode does: ordered to attack an enemy unit, it
walks after that enemy when it leaves the range, above all when it routs, and it keeps shooting
at a shattered enemy.

**Cause.** Guard mode is one byte on the battle unit, and the only pursuit test that reads it is
in the melee attack task. A missile unit's attack runs the missile attack task. When its target
leaves the range, that task's fire and can-shoot states switch to an intercept state, and the
group walks towards the target. Guard mode is never asked. Fire at will picks its target among
the enemies in range by distance; a shattered enemy only scores a little lower and is still
chosen. Details: `ARCHITECTURE.md`, engine reference `docs/re/guard_mode_x86.md`.

**Fix.** For missile units in guard mode only:

- **No chase.** Once the unit has had a target in range, "go and intercept it" is answered no.
  The task ends as it does for an order that may not move, and the unit stays where it stands.
  An attack order on a target that was never in range still walks into range.
- **No shots at shattered units.** A shattered target (morale level 7) counts as invalid for the
  missile attack task, so the attack order on it ends.
- **Fire at will skips shattered units.** Shattered enemies are taken out of fire at will's
  choice. The unit takes another enemy in range, or holds fire when there is none.

Units not in guard mode, melee units and naval units behave as before.

**State.** Dev build (2026-10-20), compiled into the same `dinput8.dll` as the other fixes. No
public release yet. Single player and replays only.

## Results

The user's replay `guard mode`: two groups of one yari, one matchlock and two bow units, one
group in guard mode, attack orders on enemy units that then rout. Recorded with the dev build of
2026-10-19 (all fixes on, pathfix off), which these runs keep. Speed 1, `--linger 90`: the battle
is decided at tick 953 (95 s) and the replay plays on to tick 1262. Distances are from each
unit's position at the first enemy rout (tick 847) to its last position.

| Guard-mode missile units | Fix off | Fix on |
|---|---|---|
| Distance walked after the first rout | 47, 35, 32 m | 0.7, 1.1, 1.2 m |
| Chases (missile task into INTERCEPT after the target was in range) | 3 | 0 |
| Attack orders ended on a shattered target | | 5 |
| Shattered enemies left out of fire at will's choice | | 265 |
| Unit attack tasks started against a shattered enemy after the rout | | 2 (chosen while it was still wavering; it shattered the next tick and the tasks ended) |

In this replay every enemy unit is shattered after the rout, so with the fix the guard-mode
missile units hold fire; the offline harness covers the switch to another enemy in range.
The group not in guard mode chases and shoots at shattered units with and without the fix.

The fix-off run stays in sync with the recording (identical numbers in two runs, no new entry in
`replay_desync.txt`). With the fix on the replay drifts from the first intervention on (tick 847),
so later numbers of the other units differ from the recording.

## Interaction with the other fixes

- Shares no patch site with any fix (`tools/re/check_prologues.py`). Its probe hooks use the
  state transition site (also fire-by-rank-fix's and zombie-yari-wall-fix's probe) and the attack
  command post (also zombie-yari-wall-fix's probe); never two probes on together.
- All runs above have the other dev fixes at their defaults.

## Use (dev build)

| Variable | Effect |
|---|---|
| `GUARDFIX_OFF=1` | start with the fix off; I toggles it (with the game focused) |
| `GUARDFIX_PROBE=1` | research probe: `<game>\guardfix_runs\<stamp>\` |

Live check build (off at launch, I on with the high tone, I again off with the low tone): CMake
`-DGUARDFIX_START_OFF=ON`; in such a build `GUARDFIX_ON=1` starts it on instead.

Build, tests and replay runs: `docs/runbook.md`.
