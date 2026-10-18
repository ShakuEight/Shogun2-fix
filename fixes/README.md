# Fixes

Each folder here is one game fix compiled into the same `dinput8.dll`:

| Fix | Entry points | CMake option |
|---|---|---|
| `pathfix/` | `Pathfix_Install`, `Pathfix_Remove` | `FIX_PATHFIX` |
| `artillery-targeting-fix/` (artfix, was aimfix) | `ArtFix_Install`, `ArtFix_Remove` | `FIX_ARTILLERY_TARGETING` (dev build only so far) |
| `fire-by-rank-fix/` (fbrfix) | `FbrFix_Install`, `FbrFix_Remove` | `FIX_FIRE_BY_RANK` (dev build only so far) |
| `siege-responsiveness-fix/` (siegefix) | `SiegeFix_Install`, `SiegeFix_Remove` | `FIX_SIEGE_RESPONSIVENESS` (dev build only so far) |

## Adding a fix

1. **Folder.** Create `fixes/<name>/` and keep everything of the fix in it:

   | Path | Content |
   |---|---|
   | `src/` | the dev source (keys, log, capture, test exports allowed) |
   | `release/` | the public source (the fix only, always on), its version resource if any, and a `README.txt` for players |
   | `<name>.cmake` | build wiring (step 4) |
   | `tests/` | offline harness and runner (step 5) |
   | `docs/` | runbook; research notes in `docs/research/` (local only) |
   | `analysis/`, `sim/` | optional: run analysis, offline models (local only) |
   | `archive/` | earlier versions, once there are any (local only) |
   | `data/` | capture runs (local only) |
   | `README.md`, `ARCHITECTURE.md`, `CLAUDE.md` | what it does, how it was found, working notes for Claude Code |
2. **Entry points.** In `src/` and in `release/`, define the same two functions:

   ```cpp
   void <Name>_Install();   // called once, on the game's first DirectInput8Create call
   void <Name>_Remove();    // called once, from DLL_PROCESS_DETACH
   ```

   Install must byte-check every patch site before writing any, and install nothing on a
   mismatch. Remove must restore every patched byte.
3. **Register.** In `proxy/fixes.cpp`, declare the two functions and add a line to `kFixes`, both
   inside `#ifdef FIX_<NAME>`.
4. **CMake.** Write `fixes/<name>/<name>.cmake` (copy `fixes/pathfix/pathfix.cmake`). It adds
   `target_compile_definitions(dinput8 PRIVATE FIX_<NAME>)` and either the release sources
   (`if(PUBLIC_RELEASE)`) or the dev sources plus the harness executable (under `BUILD_TESTS`).
   In the root `CMakeLists.txt`, add `option(FIX_<NAME> ...)` and one
   `if(FIX_<NAME>) include(fixes/<name>/<name>.cmake) endif()`.
5. **Tests.** Add an offline harness and a runner in `tests/`, with a `release` mode that runs the
   same scenarios through the dev DLL and the public DLL and requires identical results (see
   `fixes/pathfix/tests/`). Add one line for the runner to `test.bat` and one, with
   `--with-release`, to `release.bat`. In the public DLL the logger is compiled out
   (`PATHFIX_NO_LOG`), so the release source must not depend on it.
6. **Docs.** Add the fix to the table in the root `README.md` and in the root `CLAUDE.md`. Once
   the public DLL holds more than one fix, `release.bat` packages under a combined name with a
   combined player readme.

## Rules every fix follows

- **Byte-check before patching.** Record the expected bytes from the analysed binary with
  `tools/ghidra/dump_prologue.py`, and check them against the on-disk DLL with
  `tools/re/check_prologues.py`.
- **Generate stack offsets.** Use `tools/re/stack_calc.py` for every hook context offset and keep
  the generated header in the fix's `src/`. Never calculate them by hand.
- **Self-check at run time** and switch the fix off for the session on failure, audibly.
- **Keys:** letter keys only, never F-keys, and only while the game window has the focus.
- **One toggle key and one sound for every fix (user decision, 2026-10-18).** Going forward every
  fix toggles on / off with **I** and plays pathfix's cues: one high tone (880 Hz, 160 ms) = on,
  one low tone (330 Hz, 300 ms) = off, three long low tones (200 Hz, 450 ms each) = refused or
  switched off by a self-check. Reason: in dev only one fix is under test at a time (the others
  are off by their `<FIX>_OFF=1` variable), and the public release has no keys at all, since all
  fixes are on together. Pathfix already works this way. Artillery-targeting-fix (U),
  fire-by-rank-fix (Y) and siege-responsiveness-fix (J) still use their own letter and
  `MessageBeep`; move each to I and the tones above the next time it is changed. A fix that
  needs a second dev key takes a free letter for that only (pathfix keeps O and P).
- **Single player only.** Anything that changes simulation desyncs multiplayer.
- **Nothing from the game in git.** Never commit Ghidra databases, decompiled function bodies,
  disassembly listings, prologue or memory dumps, or other data extracted from the game. Notes
  describe behaviour in prose; a quote of decompiler output or disassembly stays in the local
  `private/` copy. Byte signatures needed for the install check are the only exception.
- **Only current builds in git.** The repository holds each fix's current dev build and public
  release. Earlier builds, the research log, run analysis and offline models stay local;
  `.gitignore` keeps `archive/`, `analysis/`, `sim/` and `docs/research/` out.
- **Authorship.** Commits are by `ShakuEight <247325717+ShakuEight@users.noreply.github.com>`;
  no personal email address or local user name in commits or files.

## Shared infrastructure in proxy/

Moved out of `pathfix.cpp` and `aimfix.cpp` on 2026-10-18, when the fire-by-rank fix was started.
Every fix uses these instead of its own copy:

| File | What | Built into |
|---|---|---|
| `proxy/hook.h/.cpp` | `HookDef` / `HookSlot`; `Hook_GameModule` (optional SizeOfImage check), `Hook_SiteMatches` / `Hook_Expected` (byte check, rebased absolute addresses), `Hook_Prepare` (trampolines from one shared pool), `Hook_Patch`, `Hook_Remove`, `Hook_WriteCode` (a data patch inside an instruction, such as an immediate, after the caller byte-checked the whole instruction); `HOOK_DETOUR(name, handler, trampArray, i)` emits the naked stub `pushad, pushfd, push esp, call handler, popfd, popad, jmp [tramp]` | dev and public DLL |
| `proxy/gamemem.h/.cpp` | `Mem_Init` (user-mode ceiling), `PtrPlausible`, `PtrInUser`, `MemcpyTry`, `ReadDword`, `ReadByte`, `RangeReadable`, `CopyGuarded`, `WriteByteTry`, `WriteFloatTry`, `WriteDwordTry`, `GameTickAt(base)` (the autorun pointer path to the battle tick) | dev and public DLL |
| `proxy/keys.h/.cpp` | `Keys_GameHasFocus`, `Keys_RealDown`, `Keys_Edge` for fixes with their own polling loop; `Keys_Watch` / `Keys_Unwatch` for a press callback on the shared 50 ms poll thread (with a harness test bit); `Tone_Make` / `Tone_Play` (in-memory WAV tones) | dev DLL only |
| `proxy/log.h/.cpp` | run-folder logger (its folder and log names still say `pathfix_runs` and `pathfix_log.txt`) | dev DLL only (compiled out with `PATHFIX_NO_LOG`) |

Install order with the engine: byte-check every site (refuse on any mismatch), `Hook_Prepare`,
the fix's own setup, `Hook_Patch`. Never two hooks on one site: `tools/re/check_prologues.py`
lists every fix's sites, including patched immediates, and fails when two of them share a byte.

Still inside pathfix, because only pathfix needs them or because they are tied to its harness:
the crash filter and `crash.dmp` writer (only one top-level filter can win, so a second fix must
not install its own), the capture window, flush thread and CSV writers, and its game clock
`GameNow` (read from the formation controller in the `FUN_101bc880` hook; the harness drives
it with a fake clock object). New fixes read the battle tick with `GameTickAt`.
