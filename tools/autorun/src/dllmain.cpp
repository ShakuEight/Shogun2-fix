// dllmain.cpp -- entry point of the autorun version.dll proxy (tools/autorun).
//
// In the game (shogun2.exe) it opens <game>\autorun_log.txt and starts the autorun threads; in any
// other process that loads version.dll from the game folder it only forwards the exports.

#include "autorun.h"
#include "log.h"
#include "version_proxy.h"

static bool g_isGame = false;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            g_isGame = Autorun_IsGameProcess();
            if (g_isGame) {
                LogInit();
                Log("version.dll proxy (autorun) loaded (module 0x%p)", hModule);
            }
            LoadRealVersion();
            if (g_isGame) Autorun_Start(hModule);
            break;

        case DLL_PROCESS_DETACH:
            if (g_isGame) {
                Log("DLL_PROCESS_DETACH (%s)", reserved ? "process exit" : "FreeLibrary");
                Log("SHUTDOWN");
            }
            // At process exit the other threads are already gone; free the real DLL only on an
            // explicit FreeLibrary.
            if (!reserved) FreeRealVersion();
            if (g_isGame) LogClose();
            break;
    }
    return TRUE;
}
