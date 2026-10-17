# Startup commands, replay mode and the battle-over signal (x86 Empire.Retail.dll)

Found 2026-10-17 while building the unattended replay runner (`tools/autorun/`). All addresses are
Ghidra addresses in the s2x86 project (image base 0x10000000); RVA = address - 0x10000000.
Decompiles and listings behind this note are local only (`docs/re/console_decompiles/`,
`docs/re/listings/startup_mode_cmd.txt`, `battle_chain.txt`, `battle_end_chooser.txt`).

## The command dictionary and user.script.txt

The engine registers named commands at static-initialisation time: one small initialiser per
command builds four engine strings (help text, usage line, category, command name) and calls the
registrar FUN_1126bab0 with them and the handler address; the dictionary object is passed in ECX.
The usage text and the registered name can differ: the usage of the battle-exit command says
`exit_after_battle <true/false>`, but the registered name is `force_exit_after_battle`
(initialiser FUN_100113c0, category `empire_battle`).

`%APPDATA%\The Creative Assembly\Shogun2\scripts\user.script.txt` (UTF-16LE with BOM, one
`name args;` per line) is executed through this dictionary at startup, before the frontend
exists. The `mod x.pack;` lines live there. `preferences.script.txt` uses the same syntax.

String arguments are read by FUN_11370100: whitespace ends a token unless it is inside double
quotes, so a path with spaces must be quoted, and `%NAME%` is expanded from the environment.

`docudemon;` in user.script.txt writes `<game folder>\docudemon.txt`, the "command library"
(about 70 commands, among them `quit`, `restart_replay`, `enable_ingame_console`,
`log_lua_output_to_file`, `battle_xml_to_file`). It does not list the preference-style commands
(`game_startup_mode`, `force_exit_after_battle`, `replay_speed`, ...). `help <file>;` produced no
file when run from user.script.txt.

## game_startup_mode replay "<file>"

Handler FUN_100945d0 (registered by FUN_1000b0f0). It reads the first token into the global mode
string at 0x117C7A60, then, if more arguments follow, a string into the global path at
0x117C7630 (and, for other modes, two more strings into 0x117C76F0 / 0x117C763C). When the mode
is `replay` it reads one more token and sets the loop flag 0x117C7149 if that token is
`loop_replay`.

The startup function FUN_10099d50 compares the mode string with a chain of names (`campaign`,
`campaign_edit`, `battle`, `front_end`, `debug_front_end`, `replay` / `cinematic_editor`,
`campaign_replay`, `campaign_load`, ...). For `replay` it first checks the path (resolving it
against the user folder if needed; a campaign replay switches the mode to `campaign_replay`, an
unreadable file falls back to `front_end`), then opens the replay with FUN_101ccf70, allocates
the 0x28350-byte battle environment (constructor FUN_102470f0), creates the battle mode object
(FUN_1029fd50) and enters the main loop. The frontend never appears.

So `game_startup_mode replay "C:\...\x.replay";` as the last line of user.script.txt starts the
game straight into the replay. Measured: the battle clock starts about 40 s after launch and runs
at real time (10 ticks per second), also while the window is minimized.

## End of a replayed battle

When the battle is decided the game shows its end-of-battle panel ("Close Defeat", buttons End
Battle / View Replays) and waits for a click; `force_exit_after_battle true;` does not dismiss it.
Its flag (byte 0x118D1753, handler 0x101796D0) is read only by FUN_101a98a0, the chooser of the
state after the battle-over phase, where it selects the same next state as the no-results path.
A WM_CLOSE posted to the game window (class `SHOGUN 2`) shuts the game down cleanly in about 3 s
from the frontend, from a running battle and from the end-of-battle panel. The engine restores
the minimized window when the panel opens.

## Battle phase and the battle-over byte

FUN_101b5d40 is the per-tick battle state machine. Its object keeps the phase at +0x14 (switch
over 0..10; the getters at 0x10180600 / 0x101807B0 / 0x10180490 / 0x10181EB0 show 5..8 =
fighting, 7 / 9 / 10 / 11 = decided, 11 = leave, 13 = results). It is called from the battle tick
function FUN_101b3080 with ECX = [tick object + 0xB4]. The tick object counts ticks at +0x50
(incremented every tick, 0.1 game s each; the counter pathfix reads as `[[ctrl+0x14]+8]+0x50`).

The byte at tick object +0x58 is not a battle-over flag: the tick function clears it every frame,
sets it on a frame that advances no tick, and sets it after a tick when the phase is decided. A
run that used it as the end signal stopped 26 s into the battle (a no-tick frame at tick 447,
run 20261017_230406). Use the phase.

Static path, found live (inject known data: `tools/autorun/discover_chain.py` searched forward
from the game-shell global for the clock object pathfix logs) and valid unchanged in the next
process:

    shell = [module + 0x17C755C]          the game shell (set by FUN_10099d50 before the main loop)
    owner = [[shell + 0x50] + 0x00]       vtable at module + 0x15D1764 (used as a check)
    tick object = [owner + 0x1C]          tick +0x50 (uint32)
    state machine = [tick object + 0xB4]  phase +0x14 (int32)

During the battle load the path is already valid with tick 0 (about 20 s); the tick then runs at
10 per second. When the battle is decided the tick keeps running about 12 s, then freezes. At
shutdown the owner's vtable changes before the path breaks.

The x64 Complete Edition was not checked; none of these offsets carry over.
