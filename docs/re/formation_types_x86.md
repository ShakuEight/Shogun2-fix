# Formation types and the yari wall's slot layout (x86)

2026-10-19, static (s2x86, `empire.retail.dll`, image base 0x10000000; read-only Ghidra runs;
second opinion from the Complete Edition build, s2ce, `empire.retail.x64.dll`). Written for
pathfix v43 (`fixes/pathfix/`), after the report that yari wall units still path badly through a
gatehouse and round a sharp corner. Prose only; the decompiles stay local in
`docs/re/walk_decompiles/x86/` and `docs/re/walk_decompiles/x64/`, the vtable listings in
`docs/re/listings/vtables_formation_x86.txt` / `_x64.txt`, the prologues in
`docs/re/prologues_v43.txt`.

Names as in `rout_x86.md`: the **group** is the object a pathfix controller holds at +4 (soldier
count +0x18, array +0x1C); the other pathfix notes call it "the unit". The **army unit** (battle
unit) is `[group+0x510]`.

## Where the formation object lives

- The formation object is `[group+0x624]`. The per-second slot laying `FUN_101bc880` takes it
  from `[[ctrl+4]+0x624]` (`FUN_100dbce0` returns `[ctrl+4]`) and calls its vtable slot 9
  (+0x24) at 0x101BC9F8, with four stack arguments: the output slot list, the target built from
  the advanced cursor copy (`FUN_1036b980`), the width `[[ctrl+0x18]+0x6C]` (the path object's
  requested width, i.e. the order's drag width) and the men count `[group+0x18]`. The return
  address is 0x101BC9FB. Slot 9 is thiscall with `RET 0x10` in both types read here.
- The factory `FUN_1014e9e0` (x64 `FUN_180215390`) runs on the group: it sets byte +0x2780 and
  stores the new object at +0x624; `FUN_1014e990` passes the four spacing floats of the army unit
  `[group+0x510]` at +0x3B4, +0x3B8, +0x3AC, +0x3B0 (in that order) and the type from
  `FUN_101672b0` (the active formation abilities, table in `soldier_steering_x86.md` S1).
- The formation change handler `FUN_1014e870` (x64 not paired yet) runs on the army unit
  (ECX; stack: one dword and a flag byte; `RET 8`). With the flag set it first asks the main
  group's current formation (`[[army+0xB9C]+0x624]`) for the files of the order width
  (`[[army+0x440]+0x24]`) and the men (`[army+0x384]`), creates the new formation through
  `FUN_1014e990`, then turns that file count back into a width with the new formation's slot +0x14
  and re-forms the unit through `FUN_10154180` (or `FUN_1017c7e0` in one state). So a formation
  change keeps the file count, not the width.

## The two vtables

| | type 0 block_formation | type 3 pike_wall_fomation (yari wall) |
|---|---|---|
| constructor | `FUN_103bd900` (x64 `FUN_1804fc470`), object 0x18 bytes | `FUN_103bdab0` (x64 `FUN_1804fc5d0`), object 0x24 bytes, built on type 0's constructor |
| vtable | 0x115E05E0 (x64 0x1821C0980) | 0x115E0760 (x64 0x1821C0CB0) |
| +0x08 .. +0x14 | army unit +0x3B4, +0x3B8, +0x3AC, +0x3B0 | army unit +0x3B4, +0x3B8, then 1.0, 1.0 |
| +0x18 .. +0x20 | (none) | the army unit; copies of +0x3B4 and +0x3B8 |
| slot +0x10 files for width W | `FUN_103ce750(W, +0x08, +0x10)`, at most the men | `FUN_103ce750(W, +0x1C, 1.0)`, at most the men |
| slot +0x24 (9) slot generator | `FUN_103c4720` | 0x103C5D30 (no function in s2x86; x64 `FUN_180504e90`) |

`FUN_103ce750` is the shared files formula, round((W + 0.001 - offset) / spacing) + 1 (see
`placement_chain_x86.md` row 11). Type 3 also overrides slots 0..6, 10 and 11 (destructor, clone,
depth, the per-man file and rank indices); slots 7, 8, 12 and 13 are type 0's.

## The block generator (type 0, `FUN_103c4720`)

Builds a row generator from the four floats and lays the rows along the path pieces of the target
(`placement_chain_x86.md`, rows 9..12): the front row at the target, the following rows backwards
along the path, each row's files from min(requested width, the node width) by the files formula,
corner rows fanned round the pivot. Slots off the navmesh get no target (`FUN_103dafb0`).

## The yari wall generator (type 3, 0x103C5D30)

Read in both builds, which agree:

1. Files = the object's own slot +0x10 for the width (1 m per file, offset +0x1C); ranks = men /
   files, the remainder in an extra short rank. A special case moves one file into the rank of
   the remainder when the men exceed twice a count read from the army unit (+0x334 in x86,
   +0x530 in x64) and the division leaves a single rank.
2. One point and one heading: the location of the target (its +0x28 in x64) and the heading of
   the target's path piece (a vfunc of the object at target +0x20 in x64).
3. A straight grid: file k at (k + 1) / 2 m left or right of the centre line (centre out,
   alternating), rank r at r m behind the front, both from the heading's unit vectors (so the row
   and file spacing is exactly 1 m). The first ranks of the first files get a 1 m stagger.
4. Every slot: flag byte 1 (the slot laying then skips the navmesh test and always issues a go-to
   order), location looked up in the navmesh in mode 3, slot type 5 for the front rank, 4 for the
   second, 3 for the rest and for the remainder (`FUN_101bc880` maps slot types 3 / 4 / 5 to
   animation codes, the yari wall's bracing stances).

So the yari wall never walks the path pieces and never clamps a row to the node width. Its file
count follows only the requested width, which is the player's drag: the depth of a yari wall is
whatever the player sets (men / files). In a gatehouse or round a corner the grid is laid where
the front stands, straight and full width, and its slots lie in walls and behind buildings; each
man then plans his own path to his slot (`soldier_steering_x86.md` S3), so men route round both
sides of the obstacle.

On straight open ground the block generator run on the yari wall's object gives the same grid:
the same files (the object's +0x08 equals +0x1C and its spacing floats are 1.0), rows 1 m apart
backwards from the same front point. It differs only where the path narrows (files clamped to the
node width) or bends (rows follow the pieces), and in the slot flag and slot types (block slots
are navmesh-tested and carry no stance). Pathfix v43 uses exactly that (variant G + Y).

## Hook points used by pathfix v43

| Site | Stolen bytes | Use |
|---|---|---|
| 0x103C5D30 (yari wall slot 9) | `SUB ESP,0x84` (6) | Y: answers for the generator with `FUN_103c4720` when the block's span is narrow or bent |
| `FUN_1014e870` (formation change) | `PUSH ECX; PUSH EBX; MOV BL,[ESP+0x10]` (6) | event rows only |

`FUN_10154180` (the unit re-form) is zombie-yari-wall-fix's probe site and is not hooked by
pathfix (`tools/re/check_prologues.py` refuses shared bytes).
