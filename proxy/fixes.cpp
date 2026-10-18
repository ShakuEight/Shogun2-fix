// fixes.cpp -- registry of the fixes compiled into this dinput8.dll (see fixes.h).

#include "fixes.h"
#include "log.h"

#ifdef FIX_PATHFIX
void Pathfix_Install();    // fixes/pathfix/src/pathfix.cpp
void Pathfix_Remove();
#endif
#ifdef FIX_AIMFIX
void Aimfix_Install();     // fixes/aimfix/src/aimfix.cpp
void Aimfix_Remove();
#endif

static const FixModule kFixes[] = {
#ifdef FIX_PATHFIX
    { "pathfix", Pathfix_Install, Pathfix_Remove },
#endif
#ifdef FIX_AIMFIX
    { "aimfix", Aimfix_Install, Aimfix_Remove },
#endif
    { nullptr, nullptr, nullptr }
};

static const int kFixCount = (int)(sizeof(kFixes) / sizeof(kFixes[0])) - 1;

void Fixes_InstallAll() {
    Log("Fixes compiled in: %d", kFixCount);
    for (int i = 0; i < kFixCount; i++) {
        Log("Installing fix '%s'", kFixes[i].name);
        kFixes[i].install();
    }
}

void Fixes_RemoveAll() {
    for (int i = kFixCount - 1; i >= 0; i--)
        kFixes[i].remove();
}
