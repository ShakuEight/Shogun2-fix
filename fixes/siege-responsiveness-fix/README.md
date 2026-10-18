# siege-responsiveness-fix

**Problem.** In a large siege, once the attackers climb the walls, the defender's units answer
move orders late or not at all. A unit manning the wall takes long to leave it, and a unit ordered
onto the wall takes long to get there. Attack orders stay responsive.

**Cause.** Each soldier asks the pathfinder for his route. Moves onto and off walls, and the
attackers' climbing, put that request in the pathfinder's queue. The queue is served in order and
only until an A* node budget of 500 per tick (0.1 game s) is spent. The soldiers' immediate route
searches of the same tick count against that budget too. Thousands of climbing attackers fill the
queue, and a soldier who gets a new order before his request is served drops to the back. In the
user's replay, the defender's requests waited up to 9 s, and 39 % of them were never served.
Details: `ARCHITECTURE.md`, engine reference `docs/re/path_queue_x86.md`.

**Fix.** Two parts, both on:

- **A.** The queue gets its own budget, raised to 2000 nodes per tick (under 1 ms).
- **B.** Route requests of the local player's soldiers skip the queue and are searched at once,
  as the game already does for a soldier's place in his formation.

**State.** Dev build (2026-10-18), compiled into the same `dinput8.dll` as the other fixes. No
public release yet. Single player and replays only.

## Results

The user's replay `wall siege bug`: 5 defending units against 30, recorded without any fix. All
runs are at speed 1, with the artillery and fire-by-rank fixes off. Figures run from the start of
the climb (160 game s) to the end (290 s).

| | Fix off | A only | B only | A + B (default) |
|---|---|---|---|---|
| Player's queued route requests | 1498 | 1091 | none | none |
| ... searched at once instead | none | none | 2290 | 2295 |
| ... served before the soldier asked again | 61 % | 89 % | all | all |
| ... wait, median / 95 % / max (ticks) | 5 / 76 / 92 | 1 / 9 / 9 | 0 | 0 |
| Attacker's requests served | 6 % | 38 % | 6 % | 36 % |
| ... wait, median / 95 % / max (ticks) | 27 / 99 / 120 | 1 / 5 / 20 | 26 / 101 / 121 | 1 / 5 / 20 |
| Requests waiting in the queue, mean | 341 | 90 | 312 | 88 |
| Pathfinder update, 99th percentile / max (ms) | 9.8 / 18.1 | 9.6 / 17.0 | 7.6 / 17.4 | 9.6 / 17.6 |

Compatibility: with siegefix on, the fire-by-rank fix measured the same on its own replay
(`firebyrank_generic`: same delays and volleys, shots within replay drift). The siege replay
also ran with every dev fix at its defaults. All four fixes' offline harnesses pass in one DLL,
and `tools/re/check_prologues.py` finds no shared patch site.

A run with the fix on drifts from the recording, because routes arrive at other times, so the
battle may end differently. The figures above are per request and per tick.

## Use (dev build)

| Variable | Effect |
|---|---|
| `SIEGEFIX_OFF=1` | start with the fix off; J toggles it (with the game focused) |
| `SIEGEFIX_BUDGET=<n>` | queue budget per tick, 500..20000 nodes (default 2000) |
| `SIEGEFIX_QUEUE_OWN=0` | the queue shares its budget with the immediate searches, as in the game |
| `SIEGEFIX_PLAYER_SYNC=0` | the player's requests are queued, as in the game |
| `SIEGEFIX_INCR=<n>` | cap of the incremental searches, 200..20000 (default 200, unchanged) |
| `SIEGEFIX_PROBE=1` | research probe: `<game>\siegefix_runs\<stamp>\` |

Build, tests and replay runs: `docs/runbook.md`. Comparison of probe runs:
`python tests\compare_runs.py <run> [<run> ...]`.
