#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// True when this process is the game (shogun2.exe or shogun2.retail.exe). Other programs in the
// game folder (cef_process.exe, AwesomiumProcess.exe, the launcher) also load version.dll; for
// them the proxy only forwards.
bool Autorun_IsGameProcess();

// Start the watcher and pipe threads (called from DLL_PROCESS_ATTACH; never blocks).
void Autorun_Start(HMODULE self);
