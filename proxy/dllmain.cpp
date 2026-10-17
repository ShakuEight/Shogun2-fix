// dllmain.cpp -- DLL entry point for dinput8 proxy

#include "log.h"
#include "fixes.h"

// Defined in dinput8_proxy.cpp
bool LoadOriginalDinput8();
void FreeOriginalDinput8();

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            LogInit();
            Log("dinput8 proxy loaded (module 0x%p)", hModule);

            if (!LoadOriginalDinput8()) {
                Log("FATAL: Cannot proceed without real dinput8.dll");
            }

            Log("Proxy ready. Hooks install on first DirectInput8Create call.\n");
            break;

        case DLL_PROCESS_DETACH:
            Log("DLL_PROCESS_DETACH");
            Fixes_RemoveAll();
            FreeOriginalDinput8();
            LogClose();
            break;
    }
    return TRUE;
}
