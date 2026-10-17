#pragma once
// fixes.h -- the list of game fixes compiled into this dinput8.dll.
//
// Each fix lives in fixes/<name>/ and exposes two plain functions:
//   void <Name>_Install();   called once, on the game's first DirectInput8Create call
//   void <Name>_Remove();    called once, from DLL_PROCESS_DETACH
// A fix is compiled in when CMake option FIX_<NAME> is ON, which also defines
// the preprocessor symbol FIX_<NAME>. Register it in proxy/fixes.cpp.
// See fixes/README.md for the rules a fix has to follow.

struct FixModule {
    const char* name;
    void (*install)();
    void (*remove)();
};

void Fixes_InstallAll();   // installs every compiled-in fix, in list order
void Fixes_RemoveAll();    // removes them in reverse order
