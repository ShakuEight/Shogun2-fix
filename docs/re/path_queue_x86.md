# Pathfinder request queue and per-tick budgets (x86)

2026-10-18, static (s2x86, `empire.retail.dll`, image base 0x10000000) plus the siegefix probe
(fixes/siege-responsiveness-fix). Behaviour in prose; the local decompiles are in
`docs/re/walk_decompiles/x86/` (FUN_103aac30, FUN_10385fd0, FUN_1039a500, FUN_1035fb40,
FUN_103df3f0). The request layout is in `request_radius_x86.md`.

Names: P = pathfinder (`[[e+0x28]+8]+0xB8` for an entity e), Q = path request (0x48 bytes,
built on the caller's stack), R = path result (pooled; it holds a copy of Q at +0x24).

## Who asks for a path

- The entity movement state machine FUN_103df3f0 (one per soldier; e is the soldier) builds Q in
  state 1 and calls the query FUN_10385fd0 (call at 0x103DF508, return 0x103DF50D, ECX = P,
  EBX = e). Q+0x30 is the request mode, copied from e+0x448.
- Mode 0: the query searches at once (synchronous A*), on the calling thread. A soldier's slot
  order inside his unit's formation is always mode 0.
- Mode 1 and 2: the query queues the request (unless the start and end region are the same, the
  path cache has a match, or the failure list rejects it: those are answered at once). The
  entity then waits in state 2 until R+0x20 is set.
- The second caller of the query, at 0x1038AE18 (FUN_1038ad60), sends mode 2.

## The queue

- Doubly linked list of R: previous R+0x18, next R+0x1C; head P+0x148, tail P+0x14C, and P+0x150 =
  the last mode-1 request.
- FUN_10385fd0 inserts a mode-1 request right after P+0x150, so mode-1 requests are served before
  every mode-2 request; any other mode is appended at the tail. FUN_1035fb40 builds the R
  (ECX = R, argument 1 = P, argument 4 = Q, arguments 5 / 6 = its neighbours); the caller links
  the head and tail after it returns.

## The per-tick update FUN_103aac30

Called once per battle tick (0.1 game s) from FUN_101b3080, after the entity updates. In order:

1. Incremental searches (list P+0x114, next at +0x24) run while their work counter P+0x368 is
   below 200 (compare at 0x103AACC1, `CMP [ESI+0x368], 200`). The counter is cleared at
   0x103AAE20.
2. The queue is served in order by FUN_1039a500 (cache, failure list, then A*) while the A* node
   counter P+0x470 is below 500 (compare at 0x103AAEA4, immediate at 0x103AAEAA); the first
   request of a tick is always served. Each served R gets its result at +0x21 and the ready byte
   +0x20 = 1.
3. P+0x470 is cleared at 0x103AAF09, after the queue. It counts the nodes of every A* search
   since the previous clear, the immediate (mode 0) searches the entity updates made during the
   tick included. So when many soldiers search at once, the budget is spent before the queue is
   reached, and the queue gets one request per tick.

The game's own counters at P+0x680..+0x6C4 count queries, cache hits, searches and failures
(P+0x698 cache hits, +0x69C searches that found a path, +0x6A0 failed searches, +0x6A8
failure-list hits, +0x6AC unreachable, +0x6B0 queued).

## A request's life

- R is reference counted (R+8). When the entity asks again while its request is still queued (a
  new order, or a replan), the movement state machine stores the new result and releases the old
  one; at reference count 0 the free routine (0x1037EC90) unlinks it from the queue (head, tail
  and P+0x150 fixed up) and returns it to the pool. So a re-ordered soldier loses his place and
  joins at the back.
- The probe saw no request served after more than 120 ticks: by then its entity had always asked
  again.

## Battle, alliances, armies, the local player

Decoded 2026-10-18 from the rules-log writer FUN_104635d0, the advice conditions FUN_10145be0 /
FUN_10145e30 / FUN_10149740, the console command cycle_local_player (FUN_102a09b0) and a reader
at 0x102400D1; checked against the siegefix probe's memory dumps.

- World object W = [P] = [e+0x28] (the entity's world). [W+8] = the object whose +0xB4 is the
  battle and +0xB8 the pathfinder (the battle tick object of the autorun pointer path).
- Battle +0x88 alliance count, +0x8C alliance array. Alliance +0x10 its index, +0x20 army count,
  +0x24 army array. Army (0x368 bytes) +0xFC its alliance (FUN_1014f2a0), +0x114 unit count,
  +0x118 unit array, +0x1E0 / +0x1E4 dead or fled units.
- Local players: W+0x281E4 points to (army index, alliance index) pairs, W+0x281E0 their count
  (the cycle_local_player command steps through them). In the user's siege replay: one pair,
  army 0 of alliance 0, the defender's 5 units; alliance 1 had one army of 30 units.
- A soldier's group (soldier +0x3B4, the object the formation code calls the unit) holds its
  army unit at group +0x510. A unit can have several groups (69 groups for the attacker's 30
  units during the climb). Entity +0x2C is a per-entity id, not a side.
- Not the local player: FUN_10140150's object (0x11A4D840) +0x5C is -1 in battle; it is a
  debug-draw selection, compared with the entity id.

## Measured in a siege

siegefix probe on the user's "wall siege bug" replay (vanilla recording, the user defended with 5
units against 30), from the start of the climb (160 game s) to the end (290 s), fix off:

- 500 to 1100 requests queued, 140 to 180 joining per tick, 3 to 5 served per tick; the node
  budget was hit on every tick from 240 s on.
- Every queued request was mode 2 from the movement state machine. Only 7 % of them were ever
  served, the defender's included; the served ones waited up to 120 ticks.
- The immediate searches used about 2000 nodes per tick late in the battle, before the queue was
  reached, so the queue usually got its one request per tick.
- The queue costs about 0.44 microseconds per A* node (500 nodes about 0.2 ms); the expensive
  part of the update is the incremental pass (up to 17 ms in one tick).

Figures with the fix on are in `fixes/siege-responsiveness-fix/README.md`.
