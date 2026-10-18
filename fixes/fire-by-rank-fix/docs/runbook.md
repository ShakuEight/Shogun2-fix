# fire-by-rank-fix runbook: A/B runs in a replay

Unattended runs with `tools/autorun` (see the `autorun-replay` skill). Preconditions: Steam
running, the game not running, `version.dll` installed (`python tools\autorun\autorun.py
--install` after a rebuild with `-DTOOL_AUTORUN=ON`).

1. Build and test: `cmd /c "E:\_SHOGUN\Shogun2_fixes\build.bat"`, then `cmd /c
   "E:\_SHOGUN\Shogun2_fixes\test.bat"`.
2. Install the dev DLL: copy `build\dinput8.dll` into the game folder as `dinput8.dll`. List the
   folder first; keep any other build there under its own name. The path fix in the dev DLL starts
   off, and `--env ARTFIX_OFF=1` keeps the artillery fix out of the comparison.
3. Run each replay with the fix off and on, with the probe:

   ```
   python -u tools\autorun\autorun.py "firebyrank_hattori" --env FBRFIX_PROBE=1 --env ARTFIX_OFF=1 --env FBRFIX_OFF=1
   python -u tools\autorun\autorun.py "firebyrank_hattori" --env FBRFIX_PROBE=1 --env ARTFIX_OFF=1
   python -I fixes\fire-by-rank-fix\tests\compare_runs.py "<game>\fbrfix_runs\<off stamp>" "<game>\fbrfix_runs\<on stamp>"
   ```

   Add `--env FBRFIX_ALL_RANKS=1`, `--env FBRFIX_GRACE=<ticks>`, `--env FBRFIX_EARLY_RELOAD=0` or
   `--env FBRFIX_FIRE_GRACE=<ticks>` for the options. Each replay takes about 2.5 minutes at
   speed 1. A replay with any fix active no longer follows its recording once the simulation
   changes, so shot totals vary by +-20 % between variants (the fire-at-will units' totals show
   the spread); `idle`, `cycle` and `wrapleft` are per man and stable.
4. Read `pathfix_log.txt` in the game folder: `fbrfix: fix ON`, `early reload ON; fire grace
   ...`, seven hook lines with the probe (three without), and at exit the group test and
   fire-completed test counters.

## Probe files (`<game>\fbrfix_runs\<stamp>\`)

| File | Rows |
|---|---|
| `fbr_states.csv` | every state change of every machine that showed a shooting state (context, owner vtable, unit, old and new state, current group or row) |
| `fbr_rowdump.csv` | every soldier of a fire-by-rank unit on each group / row state and every game second while it waits (group, reload time, order bytes, class) |
| `fbr_reload.csv` | per soldier: reload time leaving 0 (fired), first going down (counting: the reload started) and reaching 0 (loaded); runs before part 2 have no `counting` and may show false `fired` rows after casualties (compare_runs.py drops shots less than 15 s apart) |
| `fbr_orders.csv` | orders given to the soldiers of those units (caller, order words, fire target) |
| `fbr_units.csv` | every unit's soldier positions once per game second |
| `fbr_unitinfo.csv` | once per unit: soldier count, record |
| `fbr_soldier_raw.csv` | once per group task: raw words of soldiers 0..2 |
| `fbr_fix.csv` | the fix's answers to the group test (ready, grace, reloading, waiting), skipped orders, and the fire-completed test (early: one reload order given, done, cut, firing; there not_loaded = men who fired, not_ready = men still in their fire order) |

Local analysis: `fixes/fire-by-rank-fix/analysis/metrics.py` (per unit: states, volleys, shots,
waits) and `group_wait.py` (who a group waits for).
