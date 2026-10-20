// fixes.cpp -- registry of the fixes compiled into this dinput8.dll (see fixes.h).

#include "fixes.h"
#include "log.h"

#ifdef FIX_PATHFIX
void Pathfix_Install();    // fixes/pathfix/src/pathfix.cpp
void Pathfix_Remove();
#endif
#ifdef FIX_ARTILLERY_TARGETING
void ArtFix_Install();     // fixes/artillery-targeting-fix/src/artfix.cpp
void ArtFix_Remove();
#endif
#ifdef FIX_FIRE_BY_RANK
void FbrFix_Install();     // fixes/fire-by-rank-fix/src/fbrfix.cpp
void FbrFix_Remove();
#endif
#ifdef FIX_SIEGE_RESPONSIVENESS
void SiegeFix_Install();   // fixes/siege-responsiveness-fix/src/siegefix.cpp
void SiegeFix_Remove();
#endif
#ifdef FIX_WALL_MANNING
void WallFix_Install();    // fixes/wall-manning-fix/src/wallfix.cpp
void WallFix_Remove();
#endif
#ifdef FIX_ZOMBIE_YARI_WALL
void ZombieFix_Install();  // fixes/zombie-yari-wall-fix/src/zombiefix.cpp
void ZombieFix_Remove();
#endif
#ifdef FIX_WALL_COLLAPSE
void CollapseFix_Install();  // fixes/wall-collapse-fix/src/collapsefix.cpp
void CollapseFix_Remove();
#endif
#ifdef FIX_SINGLE_UNIT_GROUP
void GroupFix_Install();     // fixes/single-unit-group-fix/src/groupfix.cpp
void GroupFix_Remove();
#endif
#ifdef FIX_GUARD_MODE_RANGED
void GuardFix_Install();     // fixes/guard-mode-ranged-fix/src/guardfix.cpp
void GuardFix_Remove();
#endif

static const FixModule kFixes[] = {
#ifdef FIX_PATHFIX
    { "pathfix", Pathfix_Install, Pathfix_Remove },
#endif
#ifdef FIX_ARTILLERY_TARGETING
    { "artillery-targeting-fix", ArtFix_Install, ArtFix_Remove },
#endif
#ifdef FIX_FIRE_BY_RANK
    { "fire-by-rank-fix", FbrFix_Install, FbrFix_Remove },
#endif
#ifdef FIX_SIEGE_RESPONSIVENESS
    { "siege-responsiveness-fix", SiegeFix_Install, SiegeFix_Remove },
#endif
#ifdef FIX_WALL_MANNING
    { "wall-manning-fix", WallFix_Install, WallFix_Remove },
#endif
#ifdef FIX_ZOMBIE_YARI_WALL
    { "zombie-yari-wall-fix", ZombieFix_Install, ZombieFix_Remove },
#endif
#ifdef FIX_WALL_COLLAPSE
    { "wall-collapse-fix", CollapseFix_Install, CollapseFix_Remove },
#endif
#ifdef FIX_SINGLE_UNIT_GROUP
    { "single-unit-group-fix", GroupFix_Install, GroupFix_Remove },
#endif
#ifdef FIX_GUARD_MODE_RANGED
    { "guard-mode-ranged-fix", GuardFix_Install, GuardFix_Remove },
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
