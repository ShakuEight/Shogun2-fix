# Architecture

How the repository is put together: the runtime, the two builds, the build system, the tests,
and the shared reverse-engineering base. Each fix's own story is in its folder. The path fix's
story, from the first hypothesis to release 1.0.1, is in
[fixes/pathfix/ARCHITECTURE.md](fixes/pathfix/ARCHITECTURE.md).

Function names such as `FUN_101bc880` are Ghidra names in the original 32-bit
`Empire.Retail.dll` at image base `0x10000000`. They are rebased at run time.

## 1. Runtime

```
Shogun2.exe
  loads dinput8.dll from its own folder              proxy/dllmain.cpp
    -> real System32\dinput8.dll, 5 exports forwarded proxy/dinput8_proxy.cpp
    -> first DirectInput8Create: Fixes_InstallAll()  proxy/fixes.cpp
         -> <Name>_Install() of every compiled-in fix (in list order)
  process exit: Fixes_RemoveAll()                     (reverse order; every patched byte restored)
```

Every fix follows the same conventions:

- **Change the game's inputs, not its outputs.** A fix edits a value the engine is about to use,
  such as a waypoint, a radius, a flag or an argument. The engine then does the rest itself,
  collision included.
- **Byte-check before patching.** All patch sites, and any game helper a fix calls, are compared
  with the bytes recorded from the analysed binary. One mismatch and the fix installs nothing.
- **Generated stack offsets.** Hook stubs save registers in a fixed order. The offsets into that
  context come from `tools/re/stack_calc.py` and are checked live on every hit.
- **Self-checks at run time.** A failed check switches the fix off for the session.
- **Game time.** Time windows run on the engine's tick counter, not the wall clock, so they
  scale with the battle speed.

## 2. Two builds of every fix

| | Dev build | Public release |
|---|---|---|
| CMake | default tree `build/` | `-DPUBLIC_RELEASE=ON`, tree `build_release/` |
| Fix sources | `fixes/<name>/src/` | `fixes/<name>/release/` |
| Proxy logging | `proxy/log.cpp`: `<game>\pathfix_log.txt` (local time on every line), plus a run folder per recording | compiled out (`PATHFIX_NO_LOG`) |
| Extras | keys, tones, recording (P key, or `PATHFIX_CAPTURE=1`), crash dump, test exports | none |
| Output | `build\dinput8.dll` | `release\<name>-<version>\dinput8.dll` and a zip |

The release source of a fix is its dev source's production code path with everything else
removed. It is not re-measured in battle. Instead it is proven equivalent offline (section 4).
The run-folder and log names still say `pathfix` because the path fix was the first user. They
are proxy-level names.

## 3. Build system

The root `CMakeLists.txt` builds the proxy and contains one `include()` per fix, guarded by
that fix's `FIX_<NAME>` option. `fixes/<name>/<name>.cmake` adds the fix's sources to the
`dinput8` target: the dev source, or with `PUBLIC_RELEASE` the release source and version
resource. It also adds the fix's harness executable in the dev tree. Each fix registers its two
entry points in `proxy/fixes.cpp` under `#ifdef FIX_<NAME>`.

Scripts:

| Script | Does |
|---|---|
| `build.bat` | dev tree: DLL and harnesses |
| `test.bat` | one line per fix: its test runner against the dev DLL |
| `release.bat` | `build.bat`, the public tree, every fix's runner with `--with-release`, then `tools/release/package.py` |

## 4. Tests

Each fix has an offline harness in `fixes/<name>/tests/`. The harness maps the real
`empire.retail.dll` without starting the game, patches stub returns where a game function would
reach into a running battle, and loads the proxy. It then drives the hooks through the game's own
code on fake game objects, such as soldiers, portals and path nodes, and checks what the fix
wrote. A runner script runs the harness in several modes and requires exact pass counts.

**Release equivalence.** In `release` mode the harness runs the same scenarios twice: once
through the dev DLL switched to the production behaviour by a test export, once through the
public DLL. The runner requires every reference result line to appear unchanged in the public
run, including the computed values. The public run adds checks that it has no test exports,
patches only its own hooks, and writes no files.

## 5. Shared reverse-engineering base

- **`docs/re/`** holds what is known about the binary, written as our own notes: function maps
  between the x86 original and the x64 Complete Edition, struct layouts, and decodes of the
  movement system. The Ghidra output behind them (decompiles, listings, prologue dumps, CSV
  exports) stays local and out of git; `docs/README.md` lists it and how to regenerate it.
- **The x64 Complete Edition** is used only as a second compilation of the same source code.
  Its unwind tables give clean function bounds and its register arguments a clearer decompile.
  Fixes target the x86 original only.
- **`tools/ghidra/`** holds headless Ghidra scripts, read-only and without analysis, on the
  `s2x86` and `s2ce` projects. **`tools/re/`** holds the stack-offset generator, prologue checks
  and anchor matching.
- **Capture.** A dev build can record what its hooks see into run folders: one per P recording in
  play, or one per launch with `PATHFIX_CAPTURE=1`. `tools/runs/collect_runs.py` collects them into the fix's `data/`.

## 6. What lives where, and what is in git

| Path | In git |
|---|---|
| `proxy/`, `tools/`, `fixes/*/src`, `release`, `tests`, `docs/runbook.md`, the READMEs | yes |
| `docs/re/` notes (prose; quoted game code replaced by markers) | yes |
| `docs/re/` Ghidra output: decompiles, listings, prologue dumps, CSV exports; Ghidra databases; `private/` | no, never |
| `LICENSE` (Apache-2.0), `NOTICE` | yes; also shipped in every release package |
| `fixes/*/archive/` (earlier builds), `docs/research/`, `analysis/`, `sim/` | no, local only |
| `fixes/*/data/` (capture runs), `data/`, `logs/` | no |
| `build/`, `build_release/`, `release/` | no |

## 7. Shared infrastructure still inside the path fix

The path fix's dev source grew as one file and still owns code that later fixes will want: the
hook engine, the crash filter, key polling and tones, the capture thread, and the game clock.
They move into `proxy/` when the second fix needs them, as listed in
[fixes/README.md](fixes/README.md). The path fix's release source has its own minimal hook
engine.
