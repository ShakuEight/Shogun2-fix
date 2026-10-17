# docs

`re/` holds what is known about the game binary, independent of any one fix. Read it before
starting a new fix.

## In the repository

These are our own notes: layouts, call chains and behaviour described in prose, with Ghidra
function names and offsets.

| File | Content |
|---|---|
| `re/function_map_v1.md` to `v4.md` | x86 to x64 function pairs; only rows marked CONFIRMED are trusted |
| `re/inventory.md` | Modules of the original and the Complete Edition |
| `re/path_node_layout.md` | The unit path node (0xA0 bytes): links, corridor record, widths, follow points |
| `re/placement_chain_x86.md` | How path nodes reach the formation's row placer |
| `re/unit_finish_x86.md` | How a unit finishes a move order: behaviour stack, move controller, per-man ready test, the game's end-of-path release |
| `re/soldier_steering_x86.md`, `re/steering_x86.md` | Per-soldier orders, navmesh paths, per-tick motion, soft collision, crowd damping |
| `re/waypoint_choice_x86.md` | The funnel and how a corner apex is chosen |
| `re/request_radius_x86.md` | The soldier's replan request radius |

Where a note quoted decompiler output or disassembly, the quote is replaced by a marker such as
`[x86 decompile quote removed]` or `[decompiled statement removed]`.

## Local only, never in git

Ghidra databases, decompiled function bodies, disassembly listings and other data dumped from
the game binary stay out of the repository. `.gitignore` covers them. Each developer
regenerates them from their own copy of the game with the scripts in `tools/ghidra/`.

| Path | Content | Regenerate with |
|---|---|---|
| `re/walk_decompiles/`, `re/x64_decompiles/`, `re/anchor_decompiles/` | Ghidra decompiles, x86 and x64 | `tools/ghidra/headless_decompile.bat` (`decompile_funcs.py`) |
| `re/listings/` | Disassembly listings of hooked regions | `tools/ghidra/dump_listing.py` |
| `re/prologues_*.txt` | Entry bytes of every hooked function | `tools/ghidra/dump_prologue.py` |
| `re/functions_x86.csv` | Every function range in `Empire.Retail.dll` | `tools/ghidra/dump_functions.py` |
| `re/anchors_*.csv`, `re/walk_pairs*.csv`, `re/*readers*.csv`, `re/*writers*.csv` | Raw outputs of the anchor search and field sweeps | `tools/ghidra/anchor_hunt.py`, `sweep_*.py` |
| `/private/full_docs/` (repository root) | Unredacted originals of the notes above | not regenerated: local copy only |

Fix-specific research lives with the fix in `fixes/<name>/docs/research/`. Like the fix's run
analysis, offline models and earlier builds, it is kept locally and not published.
