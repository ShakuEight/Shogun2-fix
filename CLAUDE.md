# Shogun2-fix

Runtime fixes for Total War: Shogun 2 (the original 32-bit `Empire.Retail.dll`), all hosted by one
`dinput8.dll` proxy. GitHub: https://github.com/ShakuEight/Shogun2-fix (local clone at
`E:\_SHOGUN\Shogun2_fixes\`, renamed from `E:\_SHOGUN\pathfix\` on 2026-10-17; build trees made before the rename were moved to `fixes/pathfix/archive/builds/`).

| Fix | Folder | State |
|---|---|---|
| pathfix: formation through gates, doorways, corners | `fixes/pathfix/` | public release 1.0, dev build v42 |
| artillery-targeting-fix (code name artfix, was aimfix): artillery aims at the middle of the target unit | `fixes/artillery-targeting-fix/` | dev build (2026-10-18), no release |
| fire-by-rank-fix (code name fbrfix): matchlock target acquisition, fire by rank, Hattori / Ikko Ikki reload | `fixes/fire-by-rank-fix/` | research (2026-10-18) |

Shared code for all fixes lives in `proxy/` since 2026-10-18: hook engine `hook.h`, guarded
memory and battle tick `gamemem.h`, keys and tones `keys.h` (dev only). API in `fixes/README.md`.

Developer tools are separate proxy DLLs under `tools/` (never in a release):
`tools/autorun/` = `version.dll`, unattended replay runs (see "Build and test loop"). To play a
replay or test a build in the live game, use the project skill `autorun-replay`
(`.claude/skills/autorun-replay/SKILL.md`) instead of asking the user to click through the game.

Each fix keeps its own history and notes in `fixes/<name>/CLAUDE.md` (Claude Code loads it when
working in that folder). The pathfix history, every phase and run from the first hypothesis to
release 1.0, is in `fixes/pathfix/CLAUDE.md`. Shared engine knowledge goes here or in `docs/re/`.

## Session rules (mandatory)

- Never overwrite an existing file blind. Before creating a file, `ls` the
  target path. On collision: write to `<name>_tmp`, `mv <old> <old>.bak`,
  `mv <tmp> <target>`. Never rm-then-regenerate.
- No emoji in scripts, logs, or generated files.
- Prefer deterministic, offline analysis (Python over captured CSV/logs) to
  reasoning about live behaviour. Human-readable log output over raw hex.
- Never hand-calculate x86 stack offsets; use `stack_calc.py`. Manual offsets
  produced garbage caller RVAs (0xAEE8xxxx) once already.
- Inject known data and capture it rather than speculatively chasing call
  chains.
- Public repository (GitHub ShakuEight/Shogun2-fix, Apache-2.0): never commit
  Ghidra databases, decompiled function bodies, disassembly listings, prologue
  / memory dumps or other data extracted from the game, nor earlier builds
  (git holds only each fix's current dev build and public release; archive/,
  analysis/, sim/, docs/research/ stay local and ignored); describe behaviour in
  prose and keep quotes in the local `private/` copies (`docs/README.md` lists
  what is local only). Before every commit, check that nothing new matches
  inline brackets quoting an x86 or x64 decompile, decompiler variables (iVar1, param_1 = ..., DAT_...) or
  address+bytes+mnemonic listing lines.
- Git identity: `ShakuEight <247325717+ShakuEight@users.noreply.github.com>`
  (repo-local config). Never the user's personal email or Windows user name,
  in commits or in files (use `%USERPROFILE%` in paths).

## Repository layout (2026-10-16, second restructure)

- `README.md` repository overview; `ARCHITECTURE.md` how the repository and the two builds work.
- `proxy/` dinput8 proxy core: DLL entry, export forwarding, logging (`log.cpp`; compiled out
  with `PATHFIX_NO_LOG` in the public DLL), fix registry (`fixes.cpp`: each fix exposes
  `<Name>_Install` / `<Name>_Remove`, compiled in by CMake option `FIX_<NAME>`).
- `fixes/<name>/` everything of one fix: `src/` (dev source), `release/` (public source and player
  readme), `<name>.cmake` (build wiring, one include line in the root CMakeLists.txt), `tests/`
  (harness + runner), `docs/runbook.md`, `README.md`, `ARCHITECTURE.md`, `CLAUDE.md`; local only,
  not in git: `docs/research/` (research log), `analysis/`, `sim/`, `archive/` (old versions),
  `data/` (capture runs). `fixes/README.md` = how to add one.
- `docs/re/` game-binary reference shared by all fixes: notes in git (function maps, the
  movement-system decodes; quotes redacted), Ghidra output local only (decompiles, listings,
  prologues, functions_x86.csv and other CSV exports; see docs/README.md). Unredacted note
  originals in `private/full_docs/` (local only).
- `tools/ghidra/` headless scripts + `headless_decompile.bat`, `ghx86.bat` (-scriptPath
  `E:\_SHOGUN\Shogun2_fixes\tools\ghidra`); `tools/re/` stack_calc.py etc.; `tools/autorun/` (dev-tool
  version.dll proxy + `autorun.py` replay driver; local runs in `tools/autorun/data/`); `tools/runs/collect_runs.py`
  (into `fixes/pathfix/data/runs/` by default); `tools/release/package.py`.
- `data/` shared RE data (call graphs), `logs/` Ghidra logs: not in git.
- Build trees: `build/` (dev: `build.bat`), `build_release/` (public DLL: `release.bat`, CMake
  option `PUBLIC_RELEASE`). Packaged public release: `release/<name>-<version>/` + zip. All
  three not in git.
- Scripts: `build.bat`, `test.bat` (one runner line per fix), `release.bat` (build both, all tests
  including each fix's release equivalence runs, package). Run from PowerShell as
  `cmd /c "E:\_SHOGUN\Shogun2_fixes\<script>.bat"`.
- Git replaces the `.bak` rule for tracked files; untracked files still follow it.
  `fixes/pathfix/archive/MOVES.csv` and `MOVES_2.csv` (local) map every path of the two
  restructures. The public history is one squashed commit (2026-10-17); the earlier history is
  in `E:\_SHOGUN\pathfix_backups\*.bundle`.

## Paths

- Project root: `E:\_SHOGUN\Shogun2_fixes\` (git repo; the old `dinput8_proxy\` subfolder no longer exists)
- Original game: `E:\SteamLibrary\steamapps\common\Total War SHOGUN 2\`
- Target binary (original): `Empire.Retail.dll` (32-bit, Warscape engine)
- Complete Edition (released 2026-10-13, Steam app 3846830, x64):
  `E:\SteamLibrary\steamapps\common\Shogun 2 Complete Edition\`
  Battle-code module: `empire.retail.x64.dll` (64-bit, 48.5 MB, linked
  2026-10-08, image base 0x180000000; see `docs/re/inventory.md`).
- Ghidra 12.0.3: `E:\_SHOGUN\ghidra_12.0.3_PUBLIC_20260210\`, projects in
  `E:\_SHOGUN\GHIDRA\`
- RPFM 4.7.100 CLI: `E:\_SHOGUN\_rpfm\rpfm_cli.exe -g shogun_2`, schema
  `E:\_SHOGUN\_rpfm\schemas\schema_sho2.ron` (restored from the Recycle Bin 2026-10-18; the GUI
  and the rest of the old `_rpfm` folder are still there). Tables as TSV:
  `pack extract -p <pack> -t <schema> -f "<path in pack>;<out dir>"`; example script
  `fixes/fire-by-rank-fix/analysis/extract_db.py` (local).
- The loose analysis files of earlier phases (analyze_capture.py, path.csv, ...) are no longer
  in `E:\_SHOGUN\` (analyze_capture.py is in the Recycle Bin); `rpfm_agent.py` still is.
- User data / replays: `%APPDATA%\The Creative Assembly\Shogun2\` (CE may use
  a different folder; check after first launch).

## Cross-version anchors (x86 -> x64 function matching)

Addresses change completely in CE; these survive recompilation and are how
we re-find the same functions. Search the x64 module for them, then walk
xrefs (see `tools/anchor_hunt.py`).

| Anchor | Type | Leads to (x86 name) |
|--------|------|---------------------|
| "expecting at most 4 arguments (location, angle, width, move_fast)" | string | FUN_101f6bf0 move cmd parser |
| 182.04443f (65536/360) | float const | FUN_101f6bf0 angle conversion |
| "Move Orientation Width", "Move Rotation Angle" | strings | UI side of the move command |
| "ENTITY_ACTION_ATTACK_CORRIDOR_FACTOR" + "EntityActionAttack.cpp" | strings | FUN_1009f4e0 config registration |
| 1.1f (0x3f8ccccd) next to the above | float const | corridor factor default |
| "FIND_WAYPOINT", "MOVE_TO_WAYPOINT", "ARRIVED_AT_WAYPOINT" | strings | waypoint state handlers (x86 0x101c3010 / 0x101c30e0) |
| "SoftCollisionObject.cpp" source path | string | soft collision module |
| "rocket" / "torpedo" string compares inside one huge function | pattern | FUN_1035ae20 unit setup (1720 lines, 3 path-object allocators) |
| `if (hw > obj+X) obj+X = hw` on a float, right after a 0.5 multiply | pattern | FUN_1041c5d0 path obj registration (max half-width) |
| vtable with 12 entries where entry [2] serializes pos, uint16, float | pattern | move order serialization vtable |
| "NAVAL_UNIT_RECORD" | string | FUN_10353dd0 naval path builder (exclude) |

Struct offsets will NOT carry over: pointer size doubles, so any field after
a pointer shifts (moveOrder+0x60 sits after four pointer arrays). Re-derive
from the x64 decompile.

## Toolchain

- Ghidra: static analysis of Empire.Retail.dll (all FUN_xxxxxxxx names below
  are Ghidra names at image base 0x10000000; RVA = addr - 0x10000000).
- Cheat Engine: dynamic memory, write breakpoints, Lua caller histograms.
- CMake + NMake (MSVC), 32-bit build for the original; a Complete Edition
  target needs a separate x64 configuration and MinHook/Detours instead of
  hand-rolled trampolines (RIP-relative stolen bytes).
- RPFM for pack files (CLI path under Paths); rpfm_agent.py + Ollama for local-LLM middleware.
- Battle state machines: one runner `FUN_11374cf0` for all of them; state names come from each
  state's vtable slot +0x10 (docs/re/missile_fire_x86.md). A transition hook there is the
  cheapest way to watch any behaviour (fire-by-rank-fix probe).
- hooks.cpp v8b + analyze_capture.py: binary ring-buffer capture pipeline.

## Hard-won findings (do not re-learn these)

- FUN_10399F80 is a position COPIER into an ~8 KB temporary query buffer, not
  a setter of persistent entity state. Hook at RVA 0x399F9B fires (~1400
  writes/frame from 20+ callers) but modifying there changes only temp copies.
  Real entity positions live in heap (seen at e.g. 0x06A78F70 in CE).
- Null branch is the active path in FUN_10399F80 ([ECX+0x10] == null writes
  xyz directly; non-null delegates to FUN_1036e710).
- FUN_1037a850 / FUN_1035ae20 fire once per command, not per tick. Early
  hooks there never fired during movement for that reason.
- Y-height memory scans are unreliable across maps; vtable-pointer scans are
  structurally sounder for entity discovery.
- A background thread writing at 30 fps loses to the game's 60 fps writer.
- Config param system: FUN_1009f4e0 registers named floats (value at
  obj+0x5C), e.g. ENTITY_ACTION_ATTACK_CORRIDOR_FACTOR = 1.1f. Patchable
  without hooks; untested as a fix lever.
- Console/startup commands (FUN_1126bab0 registrar) run from user.script.txt
  at startup; a command's usage text can name it differently from its
  registered name (`exit_after_battle` usage = `force_exit_after_battle`).
  The tick object's byte +0x58 is NOT battle-over (also set on no-tick
  frames); the battle phase at [[tickobj+0xB4]+0x14] is (decided 7/9/10/11).

## Decision 2026-10-13: fix target is x86

The funneling bug reproduces in the Complete Edition (tested). The fix
targets the ORIGINAL x86 Empire.Retail.dll. The CE x64 module is used only
as a second compilation of the same source: cleaner decompile (unwind
tables, register args), label transfer back to the x86 project, and a
second opinion wherever the x86 decompile is ambiguous. Do not port the
hook to x64 and do not spend time on CE struct offsets beyond what is
needed to read a function.

## Build and test loop

- Build: CMake + NMake 32-bit; output dinput8.dll into the game folder.
- Verify dinput8.dll is actually imported by the target binary before
  relying on the proxy (CE may use a different input path; fall back to
  version.dll / winmm.dll / dxgi.dll).
- Repro: custom battle, one unit, move command through a narrow gap; same
  setup every time so captures are comparable.
- Unattended replay runs (2026-10-17): `python tools\autorun\autorun.py
  "<replay name or path>" [--capture]`. No CLI switch and no menu automation:
  the driver appends `game_startup_mode replay "<file>";` to
  `%APPDATA%\The Creative Assembly\Shogun2\scripts\user.script.txt` (restored
  afterwards), starts the game minimized, and the dev-tool proxy
  `version.dll` (tools/autorun, CMake option TOOL_AUTORUN, installed with
  `autorun.py --install`) reports the battle phase over a named pipe and posts
  WM_CLOSE when the battle is decided. Battles run at full speed minimized.
  Details: tools/autorun/README.md, docs/re/console_commands_x86.md.
- Multiplayer: an injected movement fix desyncs; single player / replay only.
  Replays played with a movement fix active desync too (logs\replay_desync.txt)
  and may end differently from the recording.
