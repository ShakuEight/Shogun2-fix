# wall-collapse-fix

**Problem.** In a siege defence you can deploy units anywhere on the walls, including next to
pieces the campaign already damaged. Now and then a piece that looked fine in deployment collapses
the moment you click Start Battle, and the men standing on it die (replay "broken wall bug").

**Cause.** The pieces that collapse are gate doors. When the campaign leaves a castle's gates
open to everyone, the game smashes every gate door that is still intact on the first tick of the
battle. Deployment shows those doors intact, often next to a gate tower that the campaign damage
already destroyed, and their gatehouse top is a normal wall position. Men deployed over the door
die with it. Details in `ARCHITECTURE.md` and `docs/re/building_destruction_x86.md`.

**Fix.** The battle start no longer damages the gates. The gates stay open and passable for every
side for the whole battle, exactly as the game already treats them, so the attacker loses nothing.
Artillery, rams and fire still destroy walls and gates during the battle, and the men on a piece
destroyed that way still die.

**State.** Dev build (2026-10-19), no public release source.

## Results

Replay "broken wall bug" (castle with open gates), dev build with all fixes at their defaults,
probe on:

| | Fix off | Fix on |
|---|---|---|
| Intact gate doors destroyed at Start Battle | 2 | 0 |
| Men lost by the unit on the gatehouse | 10 (200 to 190, one tick after) | 0 |
| Replay in sync | yes | desyncs at tick 453, the tick the men die in the recording |

Replay "wall siege bug" (vanilla wall assault, gates not open), every other fix off, fix on: 946
battle start calls passed through, no door kept, replay in sync to the end.

## Interaction with the other fixes

None. The fix patches one game function no other fix touches (`tools/re/check_prologues.py`:
no overlapping sites). The probe hooks are separate sites too.

## Use (dev build)

| Variable | Effect |
|---|---|
| `COLLAPSEFIX_OFF=1` | fix off at launch |
| `COLLAPSEFIX_ON=1` | fix on at launch in a live-check build |
| `COLLAPSEFIX_PROBE=1` | research probe into `<game>\collapsefix_runs\<stamp>\` |

I toggles the fix (one high tone = on, one low tone = off; three long low tones = refused at
install). The CMake option `COLLAPSEFIX_START_OFF` builds a DLL that starts with the fix off
(`docs/runbook.md`).
