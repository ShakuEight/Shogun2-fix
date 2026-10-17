// dinput8_proxy.cpp -- Load real dinput8.dll and forward exports

#include "log.h"
#include <cstdint>

// ---------------------------------------------------------------------------
// Original DLL handle and function pointers
// ---------------------------------------------------------------------------

static HMODULE g_realDinput8 = nullptr;

// DirectInput8Create is the only function the game actually calls.
// The others are COM registration stubs.
typedef HRESULT (WINAPI *pfn_DirectInput8Create)(
    HINSTANCE hinst, DWORD dwVersion, const void* riidltf,
    void** ppvOut, void* punkOuter);

static pfn_DirectInput8Create g_real_DirectInput8Create = nullptr;

// COM stubs
typedef HRESULT (WINAPI *pfn_DllCanUnloadNow)(void);
typedef HRESULT (WINAPI *pfn_DllGetClassObject)(const void*, const void*, void**);
typedef HRESULT (WINAPI *pfn_DllRegisterServer)(void);
typedef HRESULT (WINAPI *pfn_DllUnregisterServer)(void);

static pfn_DllCanUnloadNow      g_real_DllCanUnloadNow = nullptr;
static pfn_DllGetClassObject    g_real_DllGetClassObject = nullptr;
static pfn_DllRegisterServer    g_real_DllRegisterServer = nullptr;
static pfn_DllUnregisterServer  g_real_DllUnregisterServer = nullptr;

// ---------------------------------------------------------------------------
// Load / unload
// ---------------------------------------------------------------------------

bool LoadOriginalDinput8() {
    char sysDir[MAX_PATH];
    GetSystemDirectoryA(sysDir, MAX_PATH);
    strcat(sysDir, "\\dinput8.dll");

    g_realDinput8 = LoadLibraryA(sysDir);
    if (!g_realDinput8) {
        Log("FATAL: Could not load real dinput8.dll from %s (error %u)", sysDir, GetLastError());
        return false;
    }

    Log("Loaded real dinput8.dll from %s (handle 0x%p)", sysDir, g_realDinput8);

    g_real_DirectInput8Create = (pfn_DirectInput8Create)GetProcAddress(g_realDinput8, "DirectInput8Create");
    g_real_DllCanUnloadNow    = (pfn_DllCanUnloadNow)GetProcAddress(g_realDinput8, "DllCanUnloadNow");
    g_real_DllGetClassObject  = (pfn_DllGetClassObject)GetProcAddress(g_realDinput8, "DllGetClassObject");
    g_real_DllRegisterServer  = (pfn_DllRegisterServer)GetProcAddress(g_realDinput8, "DllRegisterServer");
    g_real_DllUnregisterServer = (pfn_DllUnregisterServer)GetProcAddress(g_realDinput8, "DllUnregisterServer");

    Log("  DirectInput8Create = 0x%p", g_real_DirectInput8Create);

    if (!g_real_DirectInput8Create) {
        Log("FATAL: Real dinput8.dll missing DirectInput8Create");
        return false;
    }

    return true;
}

void FreeOriginalDinput8() {
    if (g_realDinput8) {
        FreeLibrary(g_realDinput8);
        g_realDinput8 = nullptr;
        Log("Unloaded real dinput8.dll");
    }
}

// ---------------------------------------------------------------------------
// Exported functions
// ---------------------------------------------------------------------------

// Fixes install on the first DirectInput8Create call (proxy/fixes.cpp)
#include "fixes.h"

static bool g_hooksInstalled = false;

extern "C" {

__declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, const void* riidltf,
    void** ppvOut, void* punkOuter)
{
    if (!g_hooksInstalled) {
        g_hooksInstalled = true;
        Log("First DirectInput8Create call -- installing hooks...");
        Fixes_InstallAll();
    }

    if (!g_real_DirectInput8Create) return E_FAIL;
    HRESULT hr = g_real_DirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    Log("DirectInput8Create(ver=0x%08X) -> 0x%08X", dwVersion, hr);
    return hr;
}

__declspec(dllexport) HRESULT WINAPI DllCanUnloadNow(void) {
    return g_real_DllCanUnloadNow ? g_real_DllCanUnloadNow() : S_FALSE;
}

__declspec(dllexport) HRESULT WINAPI DllGetClassObject(const void* a, const void* b, void** c) {
    return g_real_DllGetClassObject ? g_real_DllGetClassObject(a, b, c) : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DllRegisterServer(void) {
    return g_real_DllRegisterServer ? g_real_DllRegisterServer() : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DllUnregisterServer(void) {
    return g_real_DllUnregisterServer ? g_real_DllUnregisterServer() : E_FAIL;
}

} // extern "C"
