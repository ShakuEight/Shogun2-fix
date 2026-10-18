// keys.h -- dev-build keys and tones shared by the fixes (never in the public DLL).
//
// Rules (fixes/README.md): letter keys only, never F-keys, and a real key counts only while a
// window of the game process has the focus (GetAsyncKeyState also sees keys typed into other
// programs). Every fix toggles with I and plays pathfix's tones (one high = on, one low = off,
// three long low = refused); see fixes/README.md, "One toggle key and one sound for every fix".
// Still on their old letters until next changed: artillery-targeting-fix U, fire-by-rank-fix Y,
// siege-responsiveness-fix J. Pathfix's extra keys: O, P.
//
// Two ways to use them:
//   - a fix with its own polling loop calls Keys_RealDown / Keys_Edge (pathfix's dump thread);
//   - a fix that only needs "key pressed -> callback" registers it with Keys_Watch; one shared
//     thread polls every watched key every 50 ms and calls the callback on the press edge.
//     The offline harness presses a key through a test bit instead (bit `testMask` of
//     *testBits, set by the fix's own test export).
//
// Tones: in-memory 16-bit mono WAVs played with PlaySound(SND_MEMORY | SND_ASYNC) on the normal
// audio device (Beep() was not audible for the user). A new tone cuts the previous one off.
#pragma once
#include <windows.h>
#include <cstdint>

bool Keys_GameHasFocus();
// The real key is down and the game has the focus.
bool Keys_RealDown(int vk);
// Press edge of `now` against the stored state; updates *down.
bool Keys_Edge(bool now, bool* down);

typedef void (*KeyPressFn)();
// Registers `onPress` for letter `vk` (and for bit `testMask` of *testBits, may be null).
// Starts the shared poll thread on the first registration. False when the table is full.
bool Keys_Watch(int vk, volatile LONG* testBits, LONG testMask, KeyPressFn onPress);
// Removes every registration of `onPress`; the thread stops (joined, up to 1 s) when none is left.
void Keys_Unwatch(KeyPressFn onPress);

// One segment of a tone: frequency in Hz (0 = silence) and length in ms.
struct ToneSeg { int hz; int ms; };
// Builds a WAV image for the segments (5 ms ramps, no clicks); null when out of memory.
uint8_t* Tone_Make(const ToneSeg* seg, int n);
// Plays a WAV image built by Tone_Make, asynchronously; false when PlaySound fails.
bool Tone_Play(const uint8_t* wav);
