# wall-manning-fix: how it works and how it was found

## The user's report

In a siege defence the player puts infantry on the walls by ordering a unit onto a wall section.
A unit at A ordered to man the wall at B often goes to an unrelated point C first and then to B.
Sharp corners on the way make it more likely. Replay `wall_manning_pathin`: three identical
yari ashigaru units ordered onto the wall, one went straight, two took the long way. C was open
ground. The fix should apply to every unit (player and AI).

## Finding the cause

1. **Ruled out first: the soldiers' route search.** The candidates from the existing notes were
   the path cache reusing another unit's corridor, the A* turning greedy after 75 nodes, and
   narrow portals skipped in the first pass (`docs/re/request_radius_x86.md`). The pathfix
   capture of the replay (fix off, in sync) showed instead that the two detouring units first
   marched to C *in formation*: the unit path builder `FUN_1039bb40` built a straight unit path
   ending at C, every man got the per-second formation slot orders, the unit stood still at C for
   5 to 9 s, and only then did the men walk to the wall one by one. C was the destination of an
   ordinary unit move, not a pathfinding detour.
2. **Who orders the move to C.** The research probe hooked the unit move creator `FUN_103db410`
   and logged its caller: both detouring units' moves came from `FUN_1017e5a0`, the enter of a
   state of one squad task. Its state vtables, getters and constructor were traced statically
   (capstone over the DLL; the code is not a defined function in Ghidra): the wall task, vtable
   0x115BF91C, constructor `FUN_1010d810`.
3. **How the wall task works** (`docs/re/wall_manning_x86.md`): state 1 takes the bounding box of
   the wall positions and asks `FUN_1016af90` whether the unit is outside a circle around it
   (half the diagonal plus the unit's arrival radius). If so, `FUN_10175f10` computes the staging
   point C and state 2 marches the formation there and waits until it has re-formed; state 3
   then assigns the men to the wall positions and sends each to his own. For the castle in the
   replay C lies on a line from the wall section into the castle, about 38 m out, independent
   of the unit's own position.
4. **The third unit** also got a staging move (to a C 13 m from where it stood), but the move
   action found the unit already within its arrival radius, so no march happened: the "straight"
   unit in the recording.

The staging march exists to bring the unit near the wall in formation before the men split up.
Measured, it gains nothing: the men keep the order's pace on their own (2.1 vs 2.4 m/s walking,
3.9 vs 4.2 m/s running), and the march adds the detour plus the re-form wait.

## The fix

One entry hook, `FUN_1016af90` (stolen `83 EC 18 57 8B F9`), the staging test. For the call from
the wall task (return address 0x101B0CAD) the original runs through the trampoline; when the game
says "far" and the fix is on, the hook answers "close enough" (AL = 0) instead. The wall task then
takes its own branch for a unit that is already near: no staging point, straight to state 3. The
men's slots, their assignment and the stances are the game's. The other caller of the test
(`FUN_10197e00`) is left alone.

`WALLFIX_MAX_DIST` (default 0 = off) keeps the staging march for a unit farther than that from
the centre of its wall positions, for players who want far-away units to arrive in formation.

## Research probe (dev build only, `WALLFIX_PROBE=1`)

Entry hooks (probe only): the defend order candidate `FUN_10254e80`, the unit move creator
`FUN_103db410`, the move controller build `FUN_103e1250`, the four form-up creators
`FUN_101a53f0` / `5500` / `5600` / `56e0`, the form-up action `FUN_103be5d0` and its go-to-slot
step `FUN_103e1780`, and the soldier A* `FUN_10386d90` (wrapped through its trampoline). A thread
samples the behaviour stacks (task vtables, state names) of every unit seen and, every 0.2 game
s, the position of every man of every unit with a staging decision. Files: `docs/runbook.md`.

## Tests

`tests/wallfix_test.cpp` maps the real DLL, patches return stubs after the stolen bytes and
enters the staging test through the game's own CALL at 0x101B0CA8 (a RET placed at 0x101B0CAD),
so the return-address filter is tested as in the game. Modes fix 19, off 16, maxdist 4, probe
38, corrupt 3 checks; every mode also checks the original bytes after unload.
