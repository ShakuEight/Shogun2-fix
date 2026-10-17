# tools

Shared tooling, not tied to one fix.

| Folder | Content |
|---|---|
| `ghidra/` | Headless Ghidra scripts (Jython, run with `-scriptPath E:\_SHOGUN\Shogun2_fixes\tools\ghidra -readOnly -noanalysis`) and two launchers: `headless_decompile.bat <project> <program> <out_dir> <addr>...` and `ghx86.bat <logname> <script> <args>` |
| `re/` | `stack_calc.py` (hook context offsets; never calculate them by hand), `check_prologues.py`, `compare_anchors.py`, `propagate_map.py`, `inventory.py` |
| `runs/` | `collect_runs.py`: copies new run folders from `<game>\pathfix_runs\` into `fixes\pathfix\data\runs\` (or `--dest`), never overwriting |
| `release/` | `package.py <version> <name> <readme>`: packages `build_release\dinput8.dll` into `release\<name>-<version>\` and a zip; called by `release.bat` |
| `autorun/` | Unattended replay runs: `version.dll` proxy (CMake option `TOOL_AUTORUN`) + `autorun.py <replay> [--capture]` driver; see [autorun/README.md](autorun/README.md). `discover_chain.py`: read-only pointer-path search in the running game |

`ghidra/find_string_refs.py <out.txt> <strings.txt> [<decompile_dir>]` lists every code reference
to a list of strings (one per line, `w:` for UTF-16, `s:` to keep substring hits) with the
containing function, and can decompile them. `decompile_funcs.py` creates a missing function in
memory (pointer-table handlers) instead of skipping the entry.

Ghidra notes: version 12.0.3 lives in `E:\_SHOGUN\ghidra_12.0.3_PUBLIC_20260210\`, projects in
`E:\_SHOGUN\GHIDRA\` (`s2x86` = the original DLL, program name `empire.retail.dll`; `s2ce` = the
Complete Edition x64). Call `launch.bat` directly with a larger heap than `analyzeHeadless.bat`'s
2 GB. The launcher splits arguments on `=`. Run the `.bat` launchers from PowerShell as
`cmd /c "..."`; exit code 255 is a launcher quirk and the outputs are still written.
