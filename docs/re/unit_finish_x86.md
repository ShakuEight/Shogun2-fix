# How a unit finishes a move order (x86, decoded 2026-10-17)

Static decode of the original 32-bit `Empire.Retail.dll` (Ghidra project `s2x86`, image base
0x10000000), checked against recorded runs `20261017_104106` and `20261017_114804` (a replay of
four 200-man units sent through a gatehouse to one spot, with a shift-queued second order).
Prose only; the decompiles stay local.

## Behaviour stack

- Each unit keeps a stack of behaviours: pointers at `unit+0x400 + 8*i` (each entry also has a
  size word), count at `unit+0x440`, current index `unit+0x454`, flags at `unit+0x450 / +0x451`.
- `FUN_101b7e20` ticks the stack: the top behaviour gets its update (vtable slot 1), the others
  their background call (slot 2). When the top behaviour's byte `+0x0C` is 1, the stack pops it
  through `FUN_1019e480(index)`, which calls each popped behaviour's virtual destructor
  (slot 0) and lowers the count. The next queued order starts once its task is on top.
- Shift-queued orders live in the same stack. The order task (`FUN_1016d9d0` for one class,
  `FUN_101b6c40` for another) also pops what lies above it when the path changes, and moves the
  queue on with `FUN_1019e480(current + 1)`.
- Behaviours of 0x78 bytes and others are placed with `FUN_10139e50(size, owner)`.

## The move controller

- Vtable at 0x115C1740: slot 0 `FUN_10145330` (scalar destructor, body `FUN_10138ff0`), slot 1
  `FUN_101b7b70` (per-tick update), slot 2 a no-op, slot 3 shared.
- Built in place by `FUN_10128570` (0x78 bytes; base `FUN_10126e30` stores the unit at `+0x04`
  and the stack owner at `+0x08`), which also lays the block once. For move orders the builder is
  the command handler `FUN_103e1250`, reached through a large dispatch table at 0x115DF1xx.
- `FUN_101b7b70`: updates the cursor (`FUN_101a9750`) unless `ctrl+0x3c` is set, lays the slot
  block once per game second (`FUN_101bc880`), and at the end stores "every soldier ready"
  (`FUN_1014ef80` over the unit's soldier list `+0x1C`, count `+0x18`) into `ctrl+0x0C`.
- `FUN_101bc880` copies the cursor `ctrl+0x34 / +0x38`, advances the copy by D (11 m measured;
  call at 0x101BC983), and sets `ctrl+0x3c` when the advanced copy is at the path end
  (`FUN_10381c20`: the node has no next node and t >= 1). It then loops over the soldiers but
  **re-orders only men who are not ready** (or all men when a flag is set): per man
  `FUN_10173680` chooses a go-to order (`FUN_103dbbf0`) or a formation move (`FUN_1018abb0`,
  given once per man, flagged in a per-controller byte array). On the last node, with the
  advanced copy not at the end, men behind the advanced point by more than `ctrl+0x40` get
  go-to orders; at the end every man gets a formation move.
- The leader (index `ctrl+0x30`, 2 in every run) is the soldier whose path projection is the
  unit cursor. For him the loop also advances the cursor when he is close to his slot, but only
  while he is not ready.

## Per-man ready test

`FUN_103cbe40(soldier)`: a man is NOT ready when byte `s+0x384` is 1 and byte `s+0x308` is not 1,
unless the object at `[s+0x248]` exists and passes `FUN_1039ed40` (ready unless its byte `+0x94`
is 1, then a virtual check through its `+0x08` object, slot +0x28). Measured: men on go-to orders
have `s+0x308` 0 and stay not ready until they stand in their slot; men on formation moves count
as ready while still walking (through the `[s+0x248]` clause). The class of `[s+0x248]` is not
identified.

## Consequence (the stuck units of v41)

When every remaining man has a formation move, the controller is popped at once and the unit
re-forms at the destination layout. When some men keep go-to orders and cannot reach their
slots (units stacked on one spot), the controller never becomes ready and the queued order never
starts. In the vanilla game the leader's slot runs ahead of him until the front reaches the path
end, so the formation moves always come. Variant G's anchor could park the leader in a slot
short of the end; being ready he was never re-ordered, the cursor stopped, the front stayed
short of the end. Dev build v42 / release 1.0 add the park release (see
`fixes/pathfix/README.md`, section 3.6).
