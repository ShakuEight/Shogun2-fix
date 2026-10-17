#pragma once
// Read-only locations in the ORIGINAL x86 empire.retail.dll (build 6262, SizeOfImage 0x01D88000)
// that the autorun proxy samples to report the battle state. RVAs are relative to the module's
// load address. No code is patched; a wrong build only makes the chain fail its checks and the
// state stays FRONTEND (logged once).
//
// How they were found (docs/re/console_commands_x86.md, tools/autorun/docs/runbook.md):
//  - shell global: the frontend/battle loop stores its game-shell object here after creating the
//    current game mode (the startup function, right before the main message pump).
//  - the path shell -> +0x50 -> +0x00 -> +0x1C to the battle tick object was found live with
//    tools/autorun/discover_chain.py against the clock object pathfix logs, and confirmed in a
//    second process.
//  - the tick object is the `this` of the battle tick function: tick counter at +0x50 (0.1 game s
//    per tick, the same counter pathfix reads as [[ctrl+0x14]+8]+0x50); [+0xB4] is the battle
//    state machine, whose phase at +0x14 is decided when it is 7, 9, 10 or 11 (the engine's own
//    test, 0x101807B0). The byte at tick object +0x58 is NOT a battle-over flag: the tick function
//    also sets it on every frame that advances no tick (seen at battle start, run 20261017_230406).

#define AR_IMAGE_SIZE          0x01D88000u   // SizeOfImage of the supported build

#define AR_SHELL_GLOBAL_RVA    0x017C755Cu   // pointer to the game shell
#define AR_SHELL_OFF_MODE      0x50u         // shell -> object holding the battle owner
#define AR_MODE_OFF_OWNER      0x00u         // -> battle owner (has a vtable)
#define AR_OWNER_VTABLE_RVA    0x015D1764u   // expected vtable of the battle owner (check)
#define AR_OWNER_OFF_TICKOBJ   0x1Cu         // -> battle tick object

#define AR_TICK_OFF_TICK       0x50u         // uint32 battle tick (0.1 game s)
#define AR_TICK_OFF_IDLE       0x58u         // byte: no tick this frame, or battle decided (info only)
#define AR_TICK_OFF_STATEOBJ   0xB4u         // -> battle state machine
#define AR_STATE_OFF_PHASE     0x14u         // int32 battle phase (0..13)

// Phase is decided (battle over): the set of 0x101807B0, plus anything from 12 up.
#define AR_PHASE_DECIDED(p)    ((p) == 7 || (p) == 9 || (p) == 10 || (p) == 11 || (p) >= 12)
#define AR_PHASE_MAX           32            // larger values = chain read garbage
