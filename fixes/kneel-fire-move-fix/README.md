# kneel-fire-move-fix

**Problem.** Fall of the Samurai line infantry have the Kneel Fire ability: the front rank kneels
so that the first two ranks fire together. While it is switched on the unit refuses every move
order. To move it, the player has to switch Kneel Fire off, give the order, and switch it on again
once the unit has arrived.

**Cause.** Kneel Fire works exactly like Fire by Rank: switching it on only changes the unit's
firing drill, which is read when the unit starts a volley. The game's "can this unit take a move
order" test has one extra first step that answers no for a unit with Kneel Fire on (Fire by Rank
has no such step). The move orders (click, drag, and the deployment drag) drop every unit that
fails this test, in the order preview and again when the order executes. Details:
`ARCHITECTURE.md`, engine reference `docs/re/kneel_fire_x86.md`.

**Fix.** Move orders skip that one step. A unit with Kneel Fire on gets exactly the answer it
would get with Kneel Fire off: it moves, unless something else stops it (routing, a behaviour
that cannot be interrupted, and the like). Kneel Fire stays on, so when the unit fires again it
kneel-fires, as the player would get by switching it off and on by hand.

The rest of the game is unchanged: the AI still switches Kneel Fire off itself before it moves,
and a kneeling unit given an attack order (or firing at will) still shoots only what is in range
and does not walk after a target out of range.

**State.** Dev build (2026-10-20), compiled into the same `dinput8.dll` as the other fixes. No
public release yet. Single player and replays only.

## Results

- Offline harness (`tests/`): the game's own test runs on fake units through the redirected
  calls. A kneeling unit can move at all eight sites; a kneeling unit whose behaviour refuses is
  still refused; units without Kneel Fire keep the game's answer. 37 checks in five modes.
- The user's replay `kneel fire` (recorded with the dev build of 2026-10-20, all fixes at their
  defaults): it holds no move order for a kneeling unit (a refused order is never sent), so it
  cannot show the change. Played with the fix off and on, it stays in sync with the recording
  and both runs make the same 17 can-move tests (no kneeling unit among them). This shows that
  the fix does not touch recorded orders without a kneeling unit.
- Live check in the game: open.

## Interaction with the other fixes

Shares no patch site with any fix (`tools/re/check_prologues.py`).
