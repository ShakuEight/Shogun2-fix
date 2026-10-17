#pragma once

#define VERSION_EXPORT_COUNT 17

// Load C:\Windows\SysWOW64\version.dll (via GetSystemDirectoryA) and resolve all 17 exports.
// Returns false if the DLL or any export is missing (the stubs of missing ones would crash).
bool LoadRealVersion();
void FreeRealVersion();
