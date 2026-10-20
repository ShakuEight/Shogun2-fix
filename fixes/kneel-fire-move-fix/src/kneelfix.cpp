// kneelfix.cpp -- kneel fire move fix for Total War: Shogun 2 (original x86 Empire.Retail.dll).
// Folder fixes/kneel-fire-move-fix/, code name kneelfix.
//
// Dev build. Fault: Fall of the Samurai line infantry with the Kneel Fire ability switched on
// refuse every move order; the player has to switch the ability off, move, and switch it on again.
//
// Engine decode (fixes/kneel-fire-move-fix/ARCHITECTURE.md, docs/re/kneel_fire_x86.md):
// - Kneel fire is special ability 0x39 (bit 0x39 of the unit's active-ability set at +0xED4,
//   vfunc +4 = test bit). The ability handler FUN_101ac250 treats it exactly like fire by rank
//   (ability 7): switching it on sets the unit's firing drill +0xED0 to 6 (kneel_fire; fire by rank
//   sets 5), switching it off restores the unit's default drill. The drill is read only when a
//   missile attack task starts firing (FUN_10177780 builds the kneel-fire volley task for drill 6).
// - FUN_1015c3f0 (ECX = unit) is the "can this unit take a move order" test. Its first step is
//   "kneel fire on -> no"; fire by rank has no such step. Only then come the real tests (battle
//   phase, the behaviour on top of the unit's stack, routing and the like).
// - The move commands BCQ_MULTIPLE_SELECTION_ORDER_MOVE / _MOVE_ORIENTATION /
//   _MOVE_ORIENTATION_AND_WIDTH run their placement builders FUN_10400870 (click) and FUN_10401c50
//   (drag, also through FUN_1048c7d0) both for the UI preview and in the command executors; the
//   builders drop every unit that fails FUN_1015c3f0. FUN_104367a0 asks it for the selection too.
//
// The fix (on from launch; KNEELFIX_OFF=1 starts off, I toggles; a build with the CMake option
// KNEELFIX_START_OFF starts off unless KNEELFIX_ON=1) redirects the eight calls of FUN_1015c3f0 in
// those four functions (the whole CALL instruction is byte-checked, only its displacement is
// written). When the game says "no" and the unit's kneel fire is on, the wrapper asks the game's
// own test again from just past its kneel step (KneelSkipEntry: the function's PUSH ESI; MOV ESI,ECX,
// then a jump to +0x1A), so a kneeling unit gets exactly the answer it would get with kneel fire
// off. Kneel fire stays on: the unit moves, and its next volley is a kneel-fire volley again.
// The ninth caller, the unit's own target step FUN_10185da0 (AI and attack orders: a kneeling
// unit shoots only what is in range and does not walk after a target), is left alone.
// Self-check: for a unit without kneel fire the two answers must agree (the first 500 such calls
// are compared); a mismatch switches the fix off for the session (log and the three-tone cue).
//
// The research probe (KNEELFIX_PROBE=1) writes <game>\kneelfix_runs\<stamp>\kneel_calls.csv: every
// call of a redirected site for a unit with kneel fire on, on each change of answer per
// (unit, site), and every call where the fix changed the answer.

#include "log.h"
#include "hook.h"
#include "gamemem.h"
#include "keys.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint32_t g_base = 0;

// ---------------------------------------------------------------------------
// The fix sites: CALL rel32 FUN_1015c3f0 (docs/re/prologues_kneelfix.txt)
// ---------------------------------------------------------------------------

static const uint32_t RVA_CAN_MOVE = 0x15C3F0;         // FUN_1015c3f0
static const uint32_t RVA_CAN_MOVE_RESUME = 0x15C40A;  // just past its kneel step (+0x1A)
static const uint32_t ABILITY_KNEEL_FIRE = 0x39;
static const uint32_t U_OFF_ABILITIES = 0xED4;

enum {
    S_CLICK_ONE = 0, S_CLICK_SEL1, S_CLICK_SEL2, S_DRAG_SEL1, S_DRAG_SEL2, S_SEL_ANY1, S_SEL_ANY2, S_DRAG_ONE,
    SITE_COUNT
};

struct CallSite {
    const char* name;
    const char* role;
    uint32_t rva;   // the CALL instruction
};

static const CallSite g_sites[SITE_COUNT] = {
    { "FUN_10400870+0x7A", "click move: the one selected unit", 0x4008EA },
    { "FUN_10400870+0x123", "click move: selection filter", 0x400993 },
    { "FUN_10400870+0x663", "click move: selection filter (placements)", 0x400ED3 },
    { "FUN_10401c50+0xFF", "drag move: selection filter", 0x401D4F },
    { "FUN_10401c50+0x819", "drag move: selection filter (placements)", 0x402469 },
    { "FUN_104367a0+0x8B", "selection: any unit can move (first loop)", 0x43682B },
    { "FUN_104367a0+0xD2", "selection: any unit can move (second loop)", 0x436872 },
    { "FUN_1048c7d0+0x47", "drag move: the one selected unit", 0x48C817 },
};

static HookDef SiteDef(int i) {
    HookDef d = {};
    d.func = g_sites[i].name;
    d.role = g_sites[i].role;
    d.rva = g_sites[i].rva;
    d.len = 5;
    int32_t rel = (int32_t)(RVA_CAN_MOVE - (g_sites[i].rva + 5));
    d.expected[0] = 0xE8;
    memcpy(d.expected + 1, &rel, 4);
    d.relocOff = -1;
    d.probeOnly = false;
    return d;
}

// FUN_1015c3f0 itself is not patched, but KneelSkipEntry enters it at +0x1A: its first 26 bytes
// (PUSH ESI; MOV ESI,ECX; the kneel test; JNZ to the "no" exit) and the instruction at the entry
// point must be the analysed ones (docs/re/prologues_kneelfix_skip.txt, prologues_kneelfix.txt).
static const uint8_t kCanMoveHead[] = {
    0x56, 0x8B, 0xF1, 0x8B, 0x86, 0xD4, 0x0E, 0x00, 0x00, 0x8D, 0x8E, 0xD4, 0x0E, 0x00, 0x00, 0x6A,
    0x39, 0x8B, 0x40, 0x04, 0xFF, 0xD0, 0x84, 0xC0, 0x75, 0x45,
    0x8B, 0x46, 0x14, 0x8B, 0x48, 0x08,
};

static bool g_sitePatched[SITE_COUNT] = {};

// ---------------------------------------------------------------------------
// State, counters
// ---------------------------------------------------------------------------

static volatile LONG g_fixOn = 0;
static volatile LONG g_selfOff = 0;
static volatile LONG g_testKeys = 0;
static bool g_keyWatched = false;
static uint8_t* g_toneOn = nullptr;
static uint8_t* g_toneOff = nullptr;
static uint8_t* g_toneRefused = nullptr;

static volatile LONG g_cCalls[SITE_COUNT] = {};
static volatile LONG g_cKneelNo = 0;      // the game said no to a unit with kneel fire on
static volatile LONG g_fChanged = 0;      // ... and the fix answered yes
static volatile LONG g_cChecked = 0;      // self-check comparisons (kneel fire off, game said no)
static volatile LONG g_fMismatch = 0;
static const LONG SELF_CHECK_CALLS = 500;

static bool FixOn() { return g_fixOn != 0; }
static uint32_t Rd(uint32_t a) { uint32_t v = 0; ReadDword(a, &v); return v; }

// ---------------------------------------------------------------------------
// Probe (KNEELFIX_PROBE=1)
// ---------------------------------------------------------------------------

static bool g_probe = false;
static CRITICAL_SECTION g_cs;
static FILE* g_fCalls = nullptr;
static volatile LONG g_rows = 0;
static const LONG MAX_ROWS = 200000;
static LARGE_INTEGER g_qpf, g_qp0;

static uint32_t Ms() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint32_t)((t.QuadPart - g_qp0.QuadPart) * 1000 / g_qpf.QuadPart);
}

static bool OpenProbe() {
    char game[MAX_PATH];
    HMODULE exe = GetModuleHandleA(nullptr);
    if (!GetModuleFileNameA(exe, game, MAX_PATH)) return false;
    char* slash = strrchr(game, '\\');
    if (!slash) return false;
    slash[1] = 0;
    SYSTEMTIME st;
    GetLocalTime(&st);
    char dir[MAX_PATH], path[MAX_PATH];
    _snprintf(dir, MAX_PATH, "%skneelfix_runs", game);
    CreateDirectoryA(dir, nullptr);
    _snprintf(dir, MAX_PATH, "%skneelfix_runs\\%04d%02d%02d_%02d%02d%02d", game, st.wYear, st.wMonth, st.wDay, st.wHour,
              st.wMinute, st.wSecond);
    if (!CreateDirectoryA(dir, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    _snprintf(path, MAX_PATH, "%s\\kneel_calls.csv", dir);
    g_fCalls = fopen(path, "w");
    if (!g_fCalls) return false;
    fprintf(g_fCalls, "ms,tick,site,unit,kneel,game,answer,fix_on\n");
    QueryPerformanceFrequency(&g_qpf);
    QueryPerformanceCounter(&g_qp0);
    Log("kneelfix: probe writing %s", dir);
    return true;
}

// Last logged answer per (unit, site): rows only on changes, and on every changed answer.
struct CallSeen { uint32_t unit; uint8_t game[SITE_COUNT], answer[SITE_COUNT]; };
static const int SEEN_TABLE = 512;
static CallSeen g_seen[SEEN_TABLE];

static void CallRow(int site, uint32_t unit, bool kneel, uint32_t game, uint32_t answer) {
    if (!g_probe) return;
    EnterCriticalSection(&g_cs);
    CallSeen& s = g_seen[((unit >> 3) * 2654435761u) & (SEEN_TABLE - 1)];
    if (s.unit != unit) {
        memset(&s, 0xFF, sizeof(s));
        s.unit = unit;
    }
    bool row = !(s.game[site] == game && s.answer[site] == answer && game == answer);
    s.game[site] = (uint8_t)game;
    s.answer[site] = (uint8_t)answer;
    if (row && g_fCalls && g_rows < MAX_ROWS) {
        g_rows++;
        fprintf(g_fCalls, "%u,%u,%d,%X,%d,%u,%u,%d\n", Ms(), GameTickAt(g_base), site, unit, kneel ? 1 : 0, game, answer,
                FixOn() ? 1 : 0);
        fflush(g_fCalls);
    }
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// The redirected calls (game thread)
// ---------------------------------------------------------------------------

typedef uint32_t (__fastcall* CanMoveFn)(uint32_t unit);
typedef uint8_t (__fastcall* BitTestFn)(uint32_t set, uint32_t edx, uint32_t bit);   // __thiscall, 1 stack arg

extern "C" uint32_t g_kneelResume = 0;   // base + RVA_CAN_MOVE_RESUME

// FUN_1015c3f0 from just past its kneel step: the function's own frame (PUSH ESI; MOV ESI,ECX), then
// its code from +0x1A on, which returns to our caller with AL = the answer (upper EAX bits undefined).
static __declspec(naked) uint32_t __fastcall KneelSkipEntry(uint32_t /*unit*/) {
    __asm push esi
    __asm mov esi, ecx
    __asm jmp dword ptr [g_kneelResume]
}

static bool KneelOn(uint32_t unit) {
    uint32_t set = unit + U_OFF_ABILITIES;
    uint32_t vt = Rd(set);
    if (!PtrPlausible(vt)) return false;
    uint32_t fn = Rd(vt + 4);
    if (!PtrPlausible(fn)) return false;
    return ((BitTestFn)(uintptr_t)fn)(set, 0, ABILITY_KNEEL_FIRE) != 0;
}

static void SelfCheckFailed(uint32_t unit) {
    InterlockedExchange(&g_selfOff, 1);
    InterlockedExchange(&g_fixOn, 0);
    Log("kneelfix: SELF-CHECK, unit 0x%08X without kneel fire: the game said no and the test past the kneel step "
        "said yes; fix switched off for this session", unit);
    Tone_Play(g_toneRefused);
}

static uint32_t HandleSite(int site, uint32_t unit) {
    InterlockedIncrement(&g_cCalls[site]);
    uint32_t game = ((CanMoveFn)(uintptr_t)(g_base + RVA_CAN_MOVE))(unit) & 0xFF;
    if (game || g_selfOff || !PtrPlausible(unit)) return game;
    bool kneel = KneelOn(unit);
    uint32_t answer = game;
    if (kneel) {
        InterlockedIncrement(&g_cKneelNo);
        if (FixOn()) {
            answer = KneelSkipEntry(unit) & 0xFF;
            if (answer) InterlockedIncrement(&g_fChanged);
        }
    } else if (FixOn() && g_cChecked < SELF_CHECK_CALLS) {
        InterlockedIncrement(&g_cChecked);
        if (KneelSkipEntry(unit) & 0xFF) {
            InterlockedIncrement(&g_fMismatch);
            SelfCheckFailed(unit);
        }
    }
    if (kneel) CallRow(site, unit, kneel, game, answer);
    return answer;
}

static uint32_t __fastcall Site0(uint32_t u) { return HandleSite(0, u); }
static uint32_t __fastcall Site1(uint32_t u) { return HandleSite(1, u); }
static uint32_t __fastcall Site2(uint32_t u) { return HandleSite(2, u); }
static uint32_t __fastcall Site3(uint32_t u) { return HandleSite(3, u); }
static uint32_t __fastcall Site4(uint32_t u) { return HandleSite(4, u); }
static uint32_t __fastcall Site5(uint32_t u) { return HandleSite(5, u); }
static uint32_t __fastcall Site6(uint32_t u) { return HandleSite(6, u); }
static uint32_t __fastcall Site7(uint32_t u) { return HandleSite(7, u); }
static const CanMoveFn g_wrappers[SITE_COUNT] = { Site0, Site1, Site2, Site3, Site4, Site5, Site6, Site7 };

// ---------------------------------------------------------------------------
// Key, harness exports
// ---------------------------------------------------------------------------

// I toggles the fix (dev build; focus only through proxy/keys). Harness: KneelFixTestKeys bit 0.
extern "C" __declspec(dllexport) void __stdcall KneelFixTestKeys(LONG bits) { InterlockedExchange(&g_testKeys, bits); }

static void ToggleKey() {
    if (InterlockedCompareExchange(&g_selfOff, 0, 0)) {
        Log("kneelfix: I, refused (switched off by the self-check)");
        Tone_Play(g_toneRefused);
        return;
    }
    bool on = !FixOn();
    InterlockedExchange(&g_fixOn, on ? 1 : 0);
    Log("kneelfix: I, fix %s (so far: kneeling units refused by the game %ld, let move %ld)", on ? "ON" : "OFF",
        g_cKneelNo, g_fChanged);
    Tone_Play(on ? g_toneOn : g_toneOff);
}

// Harness only (fixes/kneel-fire-move-fix/tests): fix and probe counters.
extern "C" __declspec(dllexport) void __stdcall KneelFixTestCounters(long* out) {
    out[0] = g_probe ? 1 : 0;
    out[1] = g_fixOn;
    out[2] = g_base ? 1 : 0;
    long calls = 0;
    for (int i = 0; i < SITE_COUNT; i++) calls += g_cCalls[i];
    out[3] = calls;
    out[4] = g_cKneelNo;
    out[5] = g_fChanged;
    out[6] = g_cChecked;
    out[7] = g_fMismatch;
    out[8] = g_selfOff;
    int patched = 0;
    for (int i = 0; i < SITE_COUNT; i++) patched += g_sitePatched[i] ? 1 : 0;
    out[9] = patched;
    out[10] = g_rows;
}

// ---------------------------------------------------------------------------
// Install / remove
// ---------------------------------------------------------------------------

static bool EnvIs(const char* name, char c) {
    char v[8] = "";
    return GetEnvironmentVariableA(name, v, sizeof(v)) > 0 && v[0] == c;
}

static bool WriteRel(int i, uint32_t to) {
    uint32_t at = g_base + g_sites[i].rva;
    int32_t rel = (int32_t)(to - (at + 5));
    return Hook_WriteCode((uint8_t*)(uintptr_t)(at + 1), &rel, 4);
}

static void UnpatchSites() {
    for (int i = 0; i < SITE_COUNT; i++) {
        if (!g_sitePatched[i]) continue;
        WriteRel(i, g_base + RVA_CAN_MOVE);
        g_sitePatched[i] = false;
    }
}

void KneelFix_Install() {
    Log("kneelfix: kneel fire move fix (dev build)");
    Mem_Init();
    uint32_t size = 0;
    uint8_t* base = Hook_GameModule(false, &size);
    if (!base) { Log("kneelfix: REFUSED, empire.retail.dll is not loaded"); return; }
    if (size != HOOK_ANALYSED_IMAGE_SIZE) { Log("kneelfix: REFUSED, SizeOfImage 0x%08X is not the analysed build", size); return; }
    for (int i = 0; i < SITE_COUNT; i++) {
        HookDef d = SiteDef(i);
        if (!Hook_SiteMatches(base, d)) {
            Log("kneelfix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", d.rva, d.func);
            return;
        }
    }
    if (memcmp(base + RVA_CAN_MOVE, kCanMoveHead, sizeof(kCanMoveHead)) != 0) {
        Log("kneelfix: REFUSED, bytes at RVA 0x%06X (FUN_1015c3f0, the can-move test) differ from the analysed binary",
            RVA_CAN_MOVE);
        return;
    }
    g_base = (uint32_t)(uintptr_t)base;
    g_kneelResume = g_base + RVA_CAN_MOVE_RESUME;
    InitializeCriticalSection(&g_cs);
    static const ToneSeg on[] = { { 880, 160 } };
    static const ToneSeg off[] = { { 330, 300 } };
    static const ToneSeg refused[] = { { 200, 450 }, { 0, 150 }, { 200, 450 }, { 0, 150 }, { 200, 450 } };
    g_toneOn = Tone_Make(on, 1);
    g_toneOff = Tone_Make(off, 1);
    g_toneRefused = Tone_Make(refused, 5);
#ifdef KNEELFIX_START_OFF
    bool fixOff = !EnvIs("KNEELFIX_ON", '1');      // live-check build: off at launch, I turns it on
    const char* offWhy = "OFF at launch (build option KNEELFIX_START_OFF)";
#else
    bool fixOff = EnvIs("KNEELFIX_OFF", '1');
    const char* offWhy = "OFF (KNEELFIX_OFF=1)";
#endif
    if (EnvIs("KNEELFIX_PROBE", '1')) {
        if (OpenProbe()) g_probe = true;
        else Log("kneelfix: probe not started (no run folder)");
    }
    InterlockedExchange(&g_fixOn, fixOff ? 0 : 1);
    for (int i = 0; i < SITE_COUNT; i++) {
        if (!WriteRel(i, (uint32_t)(uintptr_t)g_wrappers[i])) {
            Log("kneelfix: REFUSED, cannot write the call at RVA 0x%06X; restoring", g_sites[i].rva);
            UnpatchSites();
            g_probe = false;
            g_base = 0;
            return;
        }
        g_sitePatched[i] = true;
        Log("kneelfix: call %s (%s) at RVA 0x%06X redirected", g_sites[i].name, g_sites[i].role, g_sites[i].rva);
    }
    Log("kneelfix: fix %s: units with kneel fire on take move orders (kneel fire stays on)", fixOff ? offWhy : "ON");
    g_keyWatched = Keys_Watch('I', &g_testKeys, 1, ToggleKey);
    Log("kneelfix: I toggles the fix on / off%s", g_keyWatched ? "" : " (ERROR: key poll not started)");
}

void KneelFix_Remove() {
    if (g_keyWatched) { Keys_Unwatch(ToggleKey); g_keyWatched = false; }
    if (!g_base) return;
    UnpatchSites();
    long calls = 0;
    for (int i = 0; i < SITE_COUNT; i++) calls += g_cCalls[i];
    Log("kneelfix: can-move tests %ld (click one %ld, click selection %ld / %ld, drag selection %ld / %ld, selection "
        "%ld / %ld, drag one %ld); kneeling units refused by the game %ld, let move %ld; self-check %ld compared, %ld "
        "mismatches%s",
        calls, g_cCalls[0], g_cCalls[1], g_cCalls[2], g_cCalls[3], g_cCalls[4], g_cCalls[5], g_cCalls[6], g_cCalls[7],
        g_cKneelNo, g_fChanged, g_cChecked, g_fMismatch, g_selfOff ? " (fix switched off by the self-check)" : "");
    if (g_probe) {
        EnterCriticalSection(&g_cs);
        if (g_fCalls) { fclose(g_fCalls); g_fCalls = nullptr; }
        g_probe = false;
        LeaveCriticalSection(&g_cs);
        Log("kneelfix: probe closed: %ld rows", g_rows);
    }
}
