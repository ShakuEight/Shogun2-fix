# Fixes

Each folder here is one game fix compiled into the same `dinput8.dll`. Today there is one:

| Fix | Entry points | CMake option |
|---|---|---|
| `pathfix/` | `Pathfix_Install`, `Pathfix_Remove` | `FIX_PATHFIX` |

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

## Shared infrastructure that still lives inside pathfix

`pathfix.cpp` was grown as one file and still owns code that a second fix will need. Move it into
`proxy/` when the second fix is written, not before, and run the pathfix harness after the move:

| What | Where in `fixes/pathfix/src/pathfix.cpp` | Conflict if duplicated |
|---|---|---|
| Hook engine: `HookDef` table, byte check, trampolines, naked stubs, `HandleHit` dispatch | hook table near the top, install at `Pathfix_Install` | Two engines patching the same function would chain unpredictably |
| Crash filter and `crash.dmp` writer | `AssertCrashFilter` and the fault ring | Only one top-level exception filter can win |
| Key polling (I / O / P), tones | `DumpThread`, `KeyEdge`, `FixBeep` | Two fixes reading the same keys |
| Capture window, flush thread, CSV writers | `DumpThread`, `FlushAll`, `WriteAllOnce` | Both would write into the same run folder |
| Game clock | `GameNow`, read in the `FUN_101bc880` hook | Needed by any game-time window |
| Guarded memory reads | `MemcpyTry`, `ReadDword`, `WriteByteTry` | None, only duplication |

The run-folder logger (`proxy/log.cpp`) is already shared. Its folder and log names still say
`pathfix_runs` and `pathfix_log.txt`.
