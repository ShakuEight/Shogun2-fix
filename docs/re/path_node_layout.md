# Path node layout (x86 empire.retail.dll and x64 CE)

Phase 8, Task 3. Static, from the instruction LISTING of the x86 segment search
FUN_10366e40 and its three helpers (`docs/listings/FUN_10366e40_x86.txt`,
`docs/listings/node_helpers_x86.txt`, dumped from s2x86 by
`tools/dump_listing.py`), cross-checked with the x64 decompile of 18049B660
already under `docs/x64_decompiles/FUN_10366e40.c`. The CE binary was not
opened.

"Measured" fills in from the v13 capture dumps (`docs/capture_v13_analysis.txt`,
item 3) once the capture has been run. Until then every row is static.

Update 2026-10-14 (capture v13, 8 node dumps): the static rows below are
confirmed (+0x08 equals the recorded next pointer in 8 of 8 dumps; +0x80 /
+0x88 hold map-range 2D points in all 8). Measured additions and the width
field are in the "Measured node fields" section at the end. The constant at
[0x115ac9c8] is 1.0 (read from .rdata).

Correction 2026-10-14 (Phase 9, `docs/ghidra_analysis_v8.md` Task 0 and A2):
the node is 0xA0 bytes. The v13 dumps were 0x100 bytes, so their tail
+0xA0..+0xFF is the neighbouring pool block (it repeats the node pattern:
+0xC4 = the +0x24 constant, +0xB0 a small integer like +0x10, +0xB4..+0xC0
map floats like +0x14..+0x20); the earlier "+0xA0 .. +0xA8 pointers" reading
(v7) is withdrawn. The layout below is now taken from the node creator
FUN_103592a0 and the record copy FUN_10351690, and the measured section is
rewritten accordingly (previous version: `docs/path_node_layout.md.bak2`).

## Cursor (8 bytes on x86, 16 on x64)

FUN_101a9750 copies it from [this+0x18]+0xA8 (x64 [this+0x28]+0x108) and passes
its address as ECX to FUN_10366e40.

| x86 | x64 | Field | Evidence | Status |
|-----|-----|-------|----------|--------|
| +0x0 | +0x0 | node pointer: the walk starts here | `MOV ESI,[ESI]` at 10366e9e, ESI = ECX on entry; written back at 10367087 `MOV [ECX],EAX` | static |
| +0x4 | +0x8 | float progress along that node | `MOVSS [ECX+0x4],XMM0` at 10367089; kept monotonic when the node is unchanged (10367076..10367084) | static |

## Node

| x86 | x64 | Field | Evidence (x86 listing) | Evidence (x64 decompile) | Status |
|-----|-----|-------|------------------------|--------------------------|--------|
| +0x08 | +0x10 | next node pointer (0 ends the list) | `CMP [ESI+0x8],EAX` at 10366ede; `MOV ESI,[ESI+0x8]` at 10367063, loop while non-zero | `*(longlong *)(lVar6 + 0x10) == 0`; `[decompiled statement removed]` | static |
| +0x24 | +0x30 | start of a sub-object, passed as `this` to the sample builder | `LEA ECX,[ESI+0x24]` then `CALL FUN_10397c30` at 10367008 | `FUN_1804d3a90(lVar6 + 0x30, ...)` | static |
| +0x80 | +0xC4 | 2D point (two floats), second endpoint of the segment | FUN_10381da0 is `LEA EAX,[ECX+0x80]; RET`, called with ECX = node at 10366ec5 | `FUN_18197eec0(&local_140, lVar6 + 0xcc, lVar6 + 0xc4)`, third argument | static |
| +0x88 | +0xCC | 2D point (two floats), first endpoint of the segment | FUN_103a5d40 is `LEA EAX,[ECX+0x88]; RET`, called with ECX = node at 10366ecd | same call, second argument | static |

How the endpoints are used: FUN_1008dac0 (34 bytes) copies two 8-byte points
into a 16-byte segment, its first stack argument to +0x0 and its second to +0x8.
FUN_10366e40 pushes the FUN_10381da0 result (node+0x80) first and the
FUN_103a5d40 result (node+0x88) second, so the segment is
(node+0x88, node+0x80). The x64 call FUN_18197eec0(out, node+0xCC, node+0xC4)
has the same order. The segment is then used as a 2D line: its length is
`sqrt(dx*dx + dz*dz)` from the four floats, and the entity position's
projection onto the segment, as a fraction of its length, is clamped to
0 .. [0x115ac9c8] (the value of that constant was not read).

Offset shifts: next +0x8 -> +0x10 and sub-object +0x24 -> +0x30 fit one extra
4 bytes per pointer for the pointer fields before them; the endpoints shift by
0x44, so the sub-object at +0x24 (x86) itself contains pointers. That sub-object
is not decoded here.

Not seen in FUN_10366e40: any per-node width or radius. The function reads only
the next pointer, the two endpoints (through the helpers) and the sub-object
address. Whether a node carries a width elsewhere is what v13 item 4 (small
positive floats per offset) looks at; no claim is made here.

## Related helper pairs seen in this read

| x86 | x64 | Role | Status |
|-----|-----|------|--------|
| FUN_10397c30 | 1804D3A90 | sample builder on node+0x24 / node+0x30 | CANDIDATE in function_map_v1; the same call position and arguments (point, 3, 0.1f / constant, 1) in both segment searches |
| FUN_10381da0 | (inlined) | node+0x80 accessor | static |
| FUN_103a5d40 | (inlined) | node+0x88 accessor | static |
| FUN_1008dac0 | FUN_18197eec0 | segment from two 2D points | same position and argument order; not fact-checked further |

## Node structure (creator FUN_103592a0 + record copy FUN_10351690), measured and static

FUN_103592a0(P, prev, next, record, flag) (`docs/walk_decompiles/x86/FUN_103592a0.c`)
writes the three link dwords, copies a 0x90-byte corridor record into the node at
+0x0C with FUN_10351690, stores the flag byte at +0x9C and splices the node into
the list. The record is built by FUN_103baac0 (`docs/walk_decompiles/x86/FUN_103baac0.c`);
"record[i]" below is its dword index. x64 offsets are from the x64 builder
1804D8D00 (`docs/x64_decompiles/FUN_1039bb40.c`) where it reads the same field.

| x86 | x64 | Field | Evidence | Status |
|-----|-----|-------|----------|--------|
| +0x00 | - | owner: P (cursor block) | FUN_103592a0 `[decompiled statement removed]`; equals a P address in 8 of 8 v13 dumps | static + measured |
| +0x04 | - | previous node | FUN_103592a0 links `param_1[1]` as prev | static |
| +0x08 | +0x10 | next node | as above | static + measured |
| +0x0C | +0x18 | record[0]: side 0 / 1 / 2 (0 = plain node; 1 / 2 = corner node, sign of the shift); builders skip a node when non-zero | FUN_103baac0 `[decompiled statement removed]`; 0 in all 8 v13 dumps | static + measured |
| +0x10 | - | record[1], 16 bits: rounded heading bucket | FUN_1039add0 passes ROUND(fpatan(...) * const) | static |
| +0x14..+0x20 | +0x20, +0x28 | record[2..5]: the two base points (x, z) | FUN_103baac0 param_4 / param_5 | static + measured |
| +0x24..+0x63 | +0x30.. | record[6..0x15]: sub-object (FUN_103591c0 copy); first dword constant 0x5F149940 in every dump, a pointer to a shared heap object (it lies below the rebased module, so it is not a vtable of empire.retail.dll); its last byte (+0x60) is a flag | dumps; FUN_103baac0 line 26-27 | static + measured |
| +0x64, +0x68 | +0xA8, +0xAC | record[0x16..0x17]: perpendicular unit vector at the start | FUN_103baac0 param_7 | static + measured |
| +0x6C, +0x70 | +0xB0, +0xB4 | record[0x18..0x19]: perpendicular at the end | param_8 | static + measured |
| +0x74 | +0xB8 | record[0x1a]: WIDTH at the start (full width) | param_9 | static + measured |
| +0x78 | +0xBC | record[0x1b]: WIDTH at the end | param_10 | static + measured |
| +0x7C | - | record[0x1c]: clearance to the nearest obstacle (FUN_10374270), 0 for plain nodes | param_12; 0 in all 8 dumps | static + measured |
| +0x80..+0x8C | +0xC4, +0xCC | record[0x1d..0x20]: the two SHIFTED points the per-tick search walks: base point + side * perp * (width - clearance) * 0.5 when width > clearance and side != 0, else equal to the base points | FUN_103baac0 lines 42-68; equal to +0x14..+0x20 in all 8 dumps (no corner node dumped) | static + measured |
| +0x90, +0x94 | - | record[0x21..0x22]: unit vector along the shifted segment | FUN_103baac0 lines 75-79 | static + measured |
| +0x98 | - | record[0x23] byte: 1 valid segment, 0 when the shifted points coincide | FUN_103baac0 lines 41, 72 | static + measured |
| +0x9C | +0xE0 | flag byte from FUN_103592a0 (param_6); builders skip the node when non-zero | FUN_103592a0, FUN_1039bb40 line 129 | static |

Two creation paths seen in FUN_1039add0: plain nodes from FUN_103baac0 with
side 0 and both widths = the length of a two-point segment returned by
FUN_103703b0 (line 191-207), and nodes from a precomputed record table at
DAT_11a3c470 (line 139). FUN_1039bb40 then inserts corner nodes (side 1 / 2)
whose clearance comes from FUN_10374270 and whose shift therefore grows with
the width.

Cursor block P (pooled, 0x100 apart), as FUN_1039bb40 uses it (decompile lines
74 and 105-122): +0x14 dword set to 3 when the build finishes, +0x18 byte flag
selecting which of two counters is incremented, +0x28 start of a sub-object (its
first dword is tested for 0 before the cursor is set), +0xA0 set from
FUN_103596c0, +0xA8 / +0xAC the cursor (node, progress) from FUN_10179d20, +0xB0
byte set to 1 together with the cursor.
