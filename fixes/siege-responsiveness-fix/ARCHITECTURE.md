# siege-responsiveness-fix: how it was found and how it works

Engine reference: `docs/re/path_queue_x86.md` (the pathfinder request queue, the per-tick budgets,
the battle / alliance / army layout and the local player). Runs and numbers: `README.md`.

## The report

User (2026-10-18): in a large siege, once the attackers climb the walls, the defender's units
become unresponsive. A unit on the wall takes long to leave it, a unit ordered onto the wall
takes long to get there, while attack orders stay responsive. The user's theory was that the AI
issues so many commands that the player's are blocked. Repro: the user's replay
`wall siege bug.replay`, recorded without any fix (5 defending units against 30).

## What the game does

Every soldier walks with his own movement state machine. To get a route he builds a path request
and asks the pathfinder. A request is either searched at once (a soldier's slot order inside his
formation) or put in the pathfinder's request queue, and the soldier waits until the queue has
served it. Moves onto and off walls, and the attackers' climbing, use queued requests.

Once per battle tick (0.1 game s) the pathfinder serves the queue in order, and stops for the tick
when its A* node counter reaches 500. That counter also holds the nodes of every immediate search
made during the tick. When a soldier asks again before his request was served (a new order, or a
replan), his old request is dropped and the new one joins at the back.

## What the probe measured

The probe (`SIEGEFIX_PROBE=1`) hooks the pathfinder's tick and the path query and records every
queued request from joining to being served, and every tick of the queue.

From the start of the climb in the user's replay:

- 500 to 1100 requests waited, 140 to 180 joined per tick, and 3 to 5 were served per tick.
- The immediate searches alone used about 2000 nodes per tick, so the queue mostly got its one
  guaranteed request.
- Only 6 % of the attacker's queued requests and 61 % of the defender's were ever served before
  the soldier asked again. A defender's request that was served had waited up to 92 ticks (9 s).

So the user's theory is close: the attackers' requests flood the queue, and the queue is not the
battle's command queue but the pathfinder's. Why attack orders stay responsive was not measured;
a likely reason is that a soldier closing in on an enemy is steered towards him without waiting
for a queued route.

## The fix

Two parts, both on by default and both switched by J in the dev build.

**A. The queue gets its own, larger budget.** A mid-function hook where the queue is reached zeroes
the node counter, so the tick's immediate searches no longer use it up. A 4-byte patch raises the
budget's immediate from 500 to 2000. The whole compare instruction is byte-checked first and the
immediate is restored at unload. The queue costs about 0.44 microseconds per node, so 2000 nodes
are under 1 ms per tick.

**B. The local player's requests skip the queue.** An entry hook on the path query looks at every
queued request from the movement state machine. When the soldier belongs to the local player's
army, the request is turned into an immediate one, the way the game already handles every slot
order. A unit order then costs one A* search per soldier, once, at the moment of the order.

Finding the local player's army took most of the session. No field of the soldier or his unit
holds a side. The chain that works is:

1. The world object holds the local players as (army index, alliance index) pairs. The console
   command cycle_local_player steps through them.
2. The battle lists alliances, each alliance its armies, and each army its units.
3. A soldier's group holds the army unit it belongs to.

The fix reads the local army's unit list every 200 ms and checks the soldier's army unit against
it.

## Compatibility

- **Patch sites.** The fix shares no patch site with pathfix, artillery-targeting-fix or
  fire-by-rank-fix. `tools/re/check_prologues.py` lists all 38 sites of the four fixes, including
  the two patched immediates. It now fails when two sites share a byte.
- **Movement.** Pathfix changes how soldiers walk to their slots, which are immediate requests.
  This fix changes when queued requests are answered. They meet only in the path query, which
  pathfix does not hook.
- **Simulation timing.** Like the other movement fixes, it changes path timing. Replays drift
  from their recording and multiplayer desyncs, so it is for single player only.
