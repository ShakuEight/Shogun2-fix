# single-unit-group-fix

**Request.** In battle, G groups the selected units (a group card frame with a number to select
them with), Ctrl+G and Ctrl+number make a locked group (the units keep their formation). The game
refuses all three for a single unit: a group needs at least two. Players want groups of one
unit, soft and locked. A requested feature, not a bug.

**Cause.** Three tests in the battle interface:

1. The function that turns the selection into a group returns at once when fewer than two units
   are selected. G, Ctrl+G and Ctrl+number all end there.
2. The function that places a group's yellow frame hides it when the group has fewer than two
   units.
3. G goes through the group button, and the orders panel makes that button inactive when fewer
   than two units are selected. An inactive button ignores its key.

The battle command and the engine's group code have no unit count limit, so the game itself
handles a group of one. Details in `ARCHITECTURE.md` and `docs/re/unit_groups_x86.md`.

**Fix.** All three tests now refuse only an empty selection or group (one byte each, 1 to 0). A
group of one always gets the single-row frame layout, because the two-row layout's odd-count
branch would read a second card.

Soft groups (G) live only in the interface; locked groups (Ctrl+G, Ctrl+number) are also engine
groups. Both kinds work for one unit with the fix.

**State.** Dev build (2026-10-19), no public release source.

## Results

| Check | Result |
|---|---|
| Harness: the game's own group function, fake selection of 0 / 1 / 2 units | fix on: 1 and 2 grouped, 0 refused; fix off: only 2 grouped |
| Harness: the game's own frame test, groups of 0 / 1 / 2 / 3 units | fix on: 1 shown in the single-row layout, 2 and 3 untouched, 0 hidden; fix off: 1 hidden |
| Replay "control group" (two-unit groups), fix on | engine group commands as recorded, in sync |
| Replay "control group 2" (groups of one, recorded with the first build) | four one-unit groups created by the engine, units added and removed, ended normally |
| Live, first build | Ctrl+number and Ctrl+G make a locked group of one, Ctrl+G again makes it soft; G does nothing; no frame and number around the card |
| Live, second build | frame and number shown around a one-unit group; G still nothing; the probe showed the group button set "inactive" at one selected unit and "active" at two |
| Replay "control group 3" (Ctrl+1 on one unit, G on one, G on two, G on one), third build | one engine group (the Ctrl+1 group; G groups are interface-only), in sync |
| Live, third build (land and naval) | G on one unit makes a soft group with frame and number, G again dissolves it, Ctrl+G and Ctrl+number as before; naval the same |
| Replay "control group 4 naval", third build | no engine group commands (soft groups only), ended normally |

A replay replays battle commands, not key presses, and runs no selection manager. It cannot show
the interface side.

## Interaction with the other fixes

None. The fix and probe sites are separate from every other fix's sites
(`tools/re/check_prologues.py`: no overlapping sites).

## Use (dev build)

| Variable | Effect |
|---|---|
| `GROUPFIX_OFF=1` | fix off at launch |
| `GROUPFIX_ON=1` | fix on at launch in a live-check build |
| `GROUPFIX_PROBE=1` | research probe into `<game>\groupfix_runs\<stamp>\group_events.csv` |
| `GROUPFIX_PROBE=0` | probe off in a `GROUPFIX_PROBE_DEFAULT` build |

I toggles the fix (one high tone = on, one low tone = off; three long low tones = refused). The
CMake options `GROUPFIX_START_OFF` and `GROUPFIX_PROBE_DEFAULT` build DLLs that start with the
fix off or the probe on (`docs/runbook.md`).
