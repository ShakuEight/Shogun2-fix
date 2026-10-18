# wall-manning-fix

**Problem.** In a siege defence, a unit ordered to man a wall section often marches to an
unrelated spot on open ground first, stops there, and only then walks onto the wall. It should
go straight from where it stands to the wall.

**Cause.** The man-the-wall order runs in three steps. First it checks whether the unit stands
outside a circle around the wall positions it is to hold. If it does, the game picks a staging
point and marches the whole unit there in formation, waits until the formation has re-formed,
and only then sends each man to his place on the wall. For a castle wall the staging point lies
on a fixed line from the wall section into the castle, about 35 to 40 m from the wall. Its
position does not depend on where the unit comes from, so a unit approaching from another side
walks to the staging point and back. Sharp corners make it worse: the formation has to round
them twice. Details: `ARCHITECTURE.md`, engine reference `docs/re/wall_manning_x86.md`.

**Fix.** The staging test answers "close enough" for the wall order, so the game skips the
staging march. The men go straight from where they stand to their wall positions, each on his
own route and at the pace of the order (walk or run). Nothing else in the order changes: the same
wall positions, the same assignment of men to positions, the same stances on the wall.

**State.** Dev build (2026-10-18), compiled into the same `dinput8.dll` as the other fixes. No
public release yet. Single player and replays only.

## Results

The user's replay `wall_manning_pathin`: three 150-man yari ashigaru units of the defender ordered
onto two wall sections; recorded with siegefix, artfix and fbrfix on and pathfix off, which these
runs keep. Speed 1. Times are game seconds from the order; a man counts as arrived once he stays
within 1 m of his final position.

| Unit | | Fix off | Fix on |
|---|---|---|---|
| East (from the north-east) | median man / last man | 38.8 / 48.7 s | 21.6 / 32.5 s |
| | route per man (straight line) | 85 m (43 m) | 43 m (42 m) |
| West (from the south) | median man / last man | 26.3 / 35.3 s | 17.3 / 28.1 s |
| | route per man (straight line) | 117 m (65 m) | 67 m (63 m) |
| Third (already near its section) | median man / last man | 16.5 / 34.7 s | 16.3 / 32.1 s |
| | route per man (straight line) | 58 m (54 m) | 58 m (54 m) |

The third unit is the one that went straight in the recording: the game found it already within
its arrival radius of the staging point, so it never marched. The fix leaves such a unit as it
was.

With pathfix on (dev build, variant G = release 1.0) the figures are the same: see "Interaction
with the other fixes" below.

A run with the fix on drifts from the recording, because the men arrive at other times, so the
battle may end differently. The figures above are per unit and per order.

## Interaction with the other fixes

- **pathfix**: pathfix shapes formation marches (where the slot block is laid in narrow places,
  corner waypoints, request radius). With wallfix on, a wall order has no formation march any
  more; the men's walk onto the wall was already individual in the game. Measured on the same
  replay with the pathfix dev build in variant G (`PATHFIX_ON=1`):

  | | wallfix off | wallfix on |
  |---|---|---|
  | East: median / last man, route | 39.1 / 48.7 s, 87 m | 21.6 / 32.5 s, 43 m |
  | West: median / last man, route | 26.6 / 35.7 s, 119 m | 17.3 / 28.1 s, 67 m |
  | pathfix radius lanes set / corner waypoints adjusted | 158 / 0 | 0 / 0 |

  With wallfix off, pathfix acted on the staging march (radius lanes) without a measurable
  change. With wallfix on, every figure is identical to the run without pathfix: pathfix has
  nothing left to act on in a wall order, and the two fixes do not interfere.
- **siege-responsiveness-fix**: unchanged. The men's route requests onto the wall go through the
  same query siegefix speeds up; all runs above have siegefix on.
- No shared patch site with any fix (`tools/re/check_prologues.py`).

## Use (dev build)

| Variable | Effect |
|---|---|
| `WALLFIX_OFF=1` | start with the fix off; I toggles it (with the game focused) |
| `WALLFIX_MAX_DIST=<m>` | keep the staging march for a unit farther than this from its wall positions (default 0 = always skip) |
| `WALLFIX_PROBE=1` | research probe: `<game>\wallfix_runs\<stamp>\` |

Build, tests and replay runs: `docs/runbook.md`.
