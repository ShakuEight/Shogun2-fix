# kneel-fire-move-fix: how it works and how it was found

## The user's report

Fall of the Samurai gun units with the Kneel Fire toggle: when it is on, the front rank kneels so
that the second rank can fire as well, but the unit no longer takes move orders. To move, the
player switches it off, moves, and switches it on again. Wanted: the unit takes move orders while
Kneel Fire is on.

Replay `kneel fire` (2026-10-20 10:19), recorded with the dev `dinput8.dll` of that morning
(pathfix off, the other eight fixes on; the recording's game-folder log is kept locally as
`data/recording_pathfix_log_20261020_1016.txt`).

## Finding the cause (static)

1. **The ability number.** The technology effect `enable_ability_kneel_fire` maps to ability
   0x39. The battle unit keeps its active abilities as a bit set at +0xED4, so "Kneel Fire on" is
   bit 0x39. The DB record (`kneel_fire_ability`) has no movement flag; its tooltip says the
   ability "works when the unit is stationary".
2. **What switching it does.** The ability handler treats Fire by Rank and Kneel Fire in one case:
   on sets the firing drill (unit +0xED0) to rank_fire or kneel_fire, off restores the unit's
   default drill. The drill is read only when the missile attack task starts a volley: the
   kneel_fire drill builds a kneel-fire volley task, rank_fire the group fire task. A move order
   replaces the attack task, as it does for Fire by Rank.
3. **The refusal.** Only three places test bit 0x39. Two are the AI's line behaviour (HOLD_THE_LINE
   switches Fire by Rank and Kneel Fire on when entered and off when left; the AI turns it off
   before moving). The third is the "can this unit take a move order" test `FUN_1015c3f0`, whose
   first step answers no when the bit is set; the steps after it are the real ones (battle phase,
   the behaviour on top of the unit's stack, rout and similar states).
4. **Where it bites.** The test has five callers. Four are the move-order placement builders and
   the selection test. The click move (`BCQ_MULTIPLE_SELECTION_ORDER_MOVE`), the move with
   orientation, the drag move with orientation and width, and the deployment drag
   (`..._TELEPORT_TO_LOCATION`) run these builders twice: in the UI preview and in the command
   executor. A kneeling unit gets no placement, so no command is sent, and an executor would
   drop it anyway. The fifth caller is the unit's own target step (AI and attack orders): a unit
   that cannot move takes a missile target only when it is in range.

## The fix

The eight calls in the four order functions are redirected to a wrapper (only the CALL's
displacement is written; the whole instruction is byte-checked). The wrapper first asks the
game's own test. When the answer is no and the unit has Kneel Fire on, it asks the test again
from just past the kneel step: a small entry stub sets up the function's own frame (PUSH ESI;
MOV ESI,ECX) and jumps to `FUN_1015c3f0`+0x1A. The first 32 bytes of the function are checked at
install, and nothing else jumps into the skipped step. So a kneeling unit gets the exact answer
it would get with Kneel Fire off, and every other unit gets the game's answer.

Choices:
- **Kneel Fire stays on** while the unit moves (as Fire by Rank does). The drill only matters
  when a volley starts, so the unit kneel-fires again after its move. Switching it off and back
  on around the move, as the AI does, would need arrival detection and extra commands, for the
  same end result.
- **The unit's target step is left alone.** Changing it would make kneeling units (the AI's
  too) walk after targets out of range on attack orders and fire at will. That is a change of
  behaviour beyond the report, and it would change replays.
- **Simulation.** The executors use the redirected calls, so a move order that includes a
  kneeling unit plays out differently with the fix. Recorded orders without a kneeling unit are
  unaffected: the replay stays in sync with the fix on. A replay recorded with the fix on, and
  multiplayer against a player without it, desync. Single player only, as for every fix here.

Self-check: for a unit without Kneel Fire, the game's answer and the answer from past the kneel
step must agree. The first 500 refusals are compared; a mismatch switches the fix off for the
session (log line and the three-tone cue).

## Dev build

- On from launch; `KNEELFIX_OFF=1` starts it off. The CMake option `KNEELFIX_START_OFF` (live
  check builds) starts it off unless `KNEELFIX_ON=1`. I toggles it (one high tone on, one low
  tone off, three long low tones refused after a self-check failure).
- `KNEELFIX_PROBE=1` writes `<game>\kneelfix_runs\<stamp>\kneel_calls.csv`: every call for a
  kneeling unit on a change of answer per unit and site, and every call whose answer the fix
  changed (ms, game tick, site, unit, kneel bit, game answer, answer, fix on).
- The log's last line gives the number of tests per site, kneeling units the game refused, how
  many the fix let move, and the self-check counts.
