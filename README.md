# Shogun2-fix

Unofficial runtime fixes for **Total War: SHOGUN 2** battles, for the original 32-bit release
(`Empire.Retail.dll`). All fixes ship in one file, `dinput8.dll`, which the game loads from its
own folder. Install by copying it in, uninstall by deleting it.

| Fix | Problem it fixes | State |
|---|---|---|
| [**pathfix**](fixes/pathfix/README.md) | Units ordered through gates, doorways and round corners stop in a stretched block and file through one man at a time | Public release 1.0 |
| [**artillery-targeting-fix**](fixes/artillery-targeting-fix/README.md) | Artillery ordered to fire at a unit aims at the end of the enemy line instead of its middle | Dev build |
| [**fire-by-rank-fix**](fixes/fire-by-rank-fix/README.md) | Matchlock units with Fire by Rank wait about 10 s before their first volley; the Hattori and Ikko Ikki gun units stop firing and reloading after three volleys; a man who has fired waits for his whole rank before he reloads | Dev build |

## For players

1. Close the game.
2. Copy `dinput8.dll` from the release package into the Shogun 2 game folder, next to
   `Shogun2.exe`. In Steam: right-click the game, then Manage, then Browse local files.
3. To uninstall, delete `dinput8.dll` from the game folder.

The fixes are always on, have no settings and write no files. Use them for single player and
replays only, since a changed simulation desyncs multiplayer. If the game files differ from the
version the fixes were built for, nothing is installed and the game runs unmodified. The
package's `README.txt` has the details.

## How it works

The game imports `dinput8.dll`, and Windows looks in the game folder first. The proxy in
`proxy/` loads the real `System32\dinput8.dll` and forwards DirectInput to it. On the game's
first DirectInput call, it installs every compiled-in fix through a small registry.

Each fix patches a few instructions in the battle engine. Every patch site is byte-checked
against the analysed binary first. The hook stack layouts are generated, not hand-calculated,
and every fix carries run-time self-checks that switch it off on failure. How the repository,
the builds and the tests fit together is described in [ARCHITECTURE.md](ARCHITECTURE.md).

There are two builds of the DLL:

| | Dev build | Public release |
|---|---|---|
| Built by | `build.bat`, into `build\` | `release.bat`, into `build_release\`, packaged in `release\` |
| Per fix | its dev source, with keys, log, optional capture and test exports | its release source: the fix only, always on |
| Purpose | research and further work | players |

## Build, test, release

Requirements: Visual Studio 2022 Build Tools (x86 MSVC), CMake, Python 3 with numpy. The tests
need the game's `empire.retail.dll` but no running game. Set `GAME_DIR` if the game is not in
`E:\SteamLibrary\steamapps\common\Total War SHOGUN 2`.

```
cmd /c build.bat      dev DLL and every fix's offline harness
cmd /c test.bat       every fix's harness against the dev DLL
cmd /c release.bat    dev build, public build, all tests including each fix's release
                      equivalence runs, then the package release\<name>-<version>\ and its zip
```

## Repository layout

```
proxy/              dinput8 proxy core: DLL entry, export forwarding, logging, fix registry,
                    shared hook engine, guarded memory access, keys and tones
fixes/
  README.md         how to add a fix
  pathfix/          the path fix: dev build v42, public release source, harness, docs
  artillery-targeting-fix/  the artillery targeting fix (artfix, was aimfix): dev build, harness, docs (no release source yet)
  fire-by-rank-fix/ the fire-by-rank fix (fbrfix): dev build, harness, docs (no release source yet)
docs/re/            what is known about Empire.Retail.dll, shared by all fixes: function
                    maps x86 to x64 and engine decodes (Ghidra output stays local)
tools/
  ghidra/           headless Ghidra scripts and launchers
  re/               stack_calc.py (hook stack offsets), prologue checks, anchors
  runs/             collect_runs.py (capture runs from the game folder)
  release/          package.py (release packaging)
  autorun/          dev tool: version.dll proxy + driver that plays a replay unattended
                    (game minimized, closed at the end of the battle)
CMakeLists.txt      the proxy; one include line per fix (fixes/<name>/<name>.cmake)
build.bat, test.bat, release.bat
```

The repository holds the current dev build and the public release of each fix. Kept on disk
but not in git: the build trees, the packaged releases, capture runs (`fixes/*/data/`), Ghidra
logs and data, and each fix's earlier builds, research log, run analysis and offline models.
Ghidra databases, decompiled function bodies, disassembly listings and other data dumped from
the game are never committed; `docs/README.md` lists them and the scripts that regenerate them.

## Adding a fix

See [fixes/README.md](fixes/README.md). Each fix gets its own folder under `fixes/`, two entry
points, a CMake include, a dev source, a release source, an offline harness and its own README.

## License

Copyright 2026 ShakuEight. Licensed under the [Apache License, Version 2.0](LICENSE); see also
[NOTICE](NOTICE).

## Disclaimer

This is an unofficial fan project, not affiliated with or endorsed by Creative Assembly or SEGA.
Total War and SHOGUN are trademarks of their owners. The repository contains no game code or game
data beyond the short byte signatures and offsets the fixes use to find and verify their patch
sites in the player's own copy of the game.
