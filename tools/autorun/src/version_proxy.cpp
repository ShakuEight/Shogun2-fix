// version_proxy.cpp -- load the real version.dll and forward its 17 exports.
//
// Each export is a naked stub that jumps through a pointer resolved with GetProcAddress, so no
// signatures are needed and the stack is untouched. The real DLL is the one in the system
// directory (SysWOW64 for this 32-bit DLL, which GetSystemDirectoryA returns in a WOW64 process).

#include "log.h"
#include "version_proxy.h"

static const char* const kNames[VERSION_EXPORT_COUNT] = {
    "GetFileVersionInfoA",       "GetFileVersionInfoByHandle", "GetFileVersionInfoExA",
    "GetFileVersionInfoExW",     "GetFileVersionInfoSizeA",    "GetFileVersionInfoSizeExA",
    "GetFileVersionInfoSizeExW", "GetFileVersionInfoSizeW",    "GetFileVersionInfoW",
    "VerFindFileA",              "VerFindFileW",               "VerInstallFileA",
    "VerInstallFileW",           "VerLanguageNameA",           "VerLanguageNameW",
    "VerQueryValueA",            "VerQueryValueW",
};

extern "C" FARPROC g_realVersion[VERSION_EXPORT_COUNT] = {};
static HMODULE g_realVersionDll = nullptr;

bool LoadRealVersion() {
    char path[MAX_PATH];
    UINT n = GetSystemDirectoryA(path, MAX_PATH);
    if (n == 0 || n + 13 >= MAX_PATH) return false;
    strcat(path, "\\version.dll");
    g_realVersionDll = LoadLibraryA(path);
    if (!g_realVersionDll) {
        Log("FATAL: cannot load the real version.dll from %s (error %lu)", path, GetLastError());
        return false;
    }
    int missing = 0;
    for (int i = 0; i < VERSION_EXPORT_COUNT; i++) {
        g_realVersion[i] = GetProcAddress(g_realVersionDll, kNames[i]);
        if (!g_realVersion[i]) {
            missing++;
            Log("real version.dll has no export %s", kNames[i]);
        }
    }
    Log("Loaded real version.dll from %s (handle 0x%p), %d of %d exports resolved", path, g_realVersionDll,
        VERSION_EXPORT_COUNT - missing, VERSION_EXPORT_COUNT);
    return missing == 0;
}

void FreeRealVersion() {
    if (g_realVersionDll) {
        FreeLibrary(g_realVersionDll);
        g_realVersionDll = nullptr;
    }
}

// A stub whose real function is missing jumps to address 0; LoadRealVersion logged it. The game
// imports only GetFileVersionInfoW, GetFileVersionInfoSizeW and VerQueryValueW.
#define VERSION_STUB(index, name)                                                   \
    extern "C" __declspec(naked) void Proxy_##name() {                              \
        __asm { jmp dword ptr [g_realVersion + (index) * 4] }                       \
    }

VERSION_STUB(0, GetFileVersionInfoA)
VERSION_STUB(1, GetFileVersionInfoByHandle)
VERSION_STUB(2, GetFileVersionInfoExA)
VERSION_STUB(3, GetFileVersionInfoExW)
VERSION_STUB(4, GetFileVersionInfoSizeA)
VERSION_STUB(5, GetFileVersionInfoSizeExA)
VERSION_STUB(6, GetFileVersionInfoSizeExW)
VERSION_STUB(7, GetFileVersionInfoSizeW)
VERSION_STUB(8, GetFileVersionInfoW)
VERSION_STUB(9, VerFindFileA)
VERSION_STUB(10, VerFindFileW)
VERSION_STUB(11, VerInstallFileA)
VERSION_STUB(12, VerInstallFileW)
VERSION_STUB(13, VerLanguageNameA)
VERSION_STUB(14, VerLanguageNameW)
VERSION_STUB(15, VerQueryValueA)
VERSION_STUB(16, VerQueryValueW)
