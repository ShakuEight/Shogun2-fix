// guardfix.cpp -- guard mode ranged fix for Total War: Shogun 2 (original x86 Empire.Retail.dll).
// Folder fixes/guard-mode-ranged-fix/, code name guardfix.
//
// Dev build. Fault: a unit in guard mode holds its position, and a melee unit in guard mode does
// not pursue. A missile unit (bows, matchlocks) in guard mode that was ordered to attack an enemy
// unit still walks after it when the enemy leaves its range (a routing enemy above all), and it
// keeps shooting at a shattered enemy.
//
// Engine decode (fixes/guard-mode-ranged-fix/ARCHITECTURE.md, docs/re/guard_mode_x86.md):
// - Guard mode is byte +0xEB1 of the battle unit (the object a soldier points at with +0x214).
//   The battle command BCQ_MULTIPLE_SELECTION_ORDER_CHANGE_GUARD_MODE (executor FUN_102a8340, per
//   selection FUN_1045d0b0) sets it through FUN_10186130 (skipped while the unit routs); unit setup
//   FUN_10150ac0 copies the default from +0x17A.
// - The melee no-pursuit: the melee attack task's PURSUE_STATE update (FUN_1019fa80) keeps chasing
//   only while FUN_101af030 says so, and that test answers no when the unit's +0xEB1 is set.
// - A missile unit's attack order runs the missile attack squad task (vtable 0x115C0F24, built by
//   FUN_10127460 from the deferred attack command FUN_103d0470). Its target battle unit is at
//   task +0x24, task +0x2D is a "never intercept" byte of the order. Its FIRE, FIRE_UNFORMED and
//   PERFORM_CAN_SHOOT_TEST updates ask FUN_101aefc0 ("+0x2D clear and the last can-shoot test
//   failed") and then go to INTERCEPT_STATE, whose enter pushes a direct intercept task (0x115C0A48)
//   that walks the group towards the target: the chase. Guard mode is never read on that path.
//   Without INTERCEPT the same updates fall through to FIRE when the test passed, else to
//   CANNOT_SHOOT_STATE, which ends the task. The same updates, and INTERCEPT's, first ask
//   FUN_101b2460 whether the target is still a valid one; true ends the task.
//
// The fix (on from launch; GUARDFIX_OFF=1 starts off, I toggles; a build with the CMake option
// GUARDFIX_START_OFF starts off unless GUARDFIX_ON=1) redirects seven calls of the missile attack
// task's updates (the whole CALL instruction is byte-checked, only its displacement is written):
// - Lever 1, no chase: the three calls of FUN_101aefc0. For a unit in guard mode, "go and
//   intercept" is answered no once the unit has had this target in range (its task was in FIRE or
//   FIRE_UNFORMED against it), so a target that leaves the range is let go: the task ends through
//   CANNOT_SHOOT as for an order with +0x2D set, and the unit stays where it stands. An order on a
//   target that was never in range still walks into range (guard mode does not refuse the order).
// - Lever 2, no shots at shattered units: the four calls of FUN_101b2460 (FIRE, FIRE_UNFORMED,
//   PERFORM_CAN_SHOOT_TEST, INTERCEPT). For a unit in guard mode the target counts as invalid when
//   its morale level (+0xCE8) is 7 (shattered), so the task ends; in INTERCEPT the target also
//   counts as invalid once it was in range (a chase already under way when guard mode is set).
// - Lever 2, fire at will: an entry hook on FUN_101752d0, fire at will's choice among the enemies
//   in range. For a missile unit in guard mode the shattered enemies are taken out of the list,
//   so fire at will takes another enemy in range or holds fire (without it, a dropped target was
//   replaced by the next shattered enemy at once, a new task every 0.2 s). A missile unit is one
//   whose missile attack task passed a redirected call in this battle.
// Units not in guard mode are never touched (every wrapper returns the game's own answer first).
// Self-check: a guard byte that is not 0 or 1 is counted; after 10 the fix switches itself off
// for the session (log and the three-tone refused cue).
//
// The research probe (GUARDFIX_PROBE=1) writes <game>\guardfix_runs\<stamp>\:
//   guard_states.csv  state machine transitions (FUN_11374cf0+0x1B) of unit tasks and of the
//                     squad tasks of interest (missile attack, intercept, group / row fire, melee
//                     attack), with the guard byte, the missile task's target and its morale
//   guard_calls.csv   the seven redirected calls: every change of answer per task and site, and
//                     every call where the fix changed the answer
//   guard_pos.csv     every unit's main group twice per game second: men, centre, guard, morale,
//                     order queue, the main group's behaviour stack
//   guard_orders.csv  every change of a unit's order queue head, as hex

#include "log.h"
#include "hook.h"
#include "gamemem.h"
#include "keys.h"
#include "stack_offsets_guard.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

static uint32_t g_base = 0;

// ---------------------------------------------------------------------------
// Probe hook (stolen bytes: docs/re/prologues_fbrfix_mid.txt, the site fire-by-rank-fix and
// zombie-yari-wall-fix also probe; never two probes on at once)
// ---------------------------------------------------------------------------

enum {
    H_TRANSITION = 0,  // FUN_11374cf0+0x1B: after the old state's exit, before the new is stored (ESI = ctx, EDI = next)
    H_FAW,             // FUN_101752d0 entry: fire at will's choice among the enemies in range (cdecl: unit, list, ...) (the fix)
    H_POST,            // FUN_103daa80 entry: group attack command posted (ECX = group, arg 1 = {target unit, ...}) (probe)
    H_UTASK,           // FUN_1012d660 entry: unit attack task constructor (ECX = task, owner, unit, arg 3 = {target, ...}) (probe)
    HOOK_COUNT
};

static const HookDef g_hookDefs[HOOK_COUNT] = {
    { "FUN_11374cf0+0x1B", "state transition", 0x1374D0B, 5,
      { 0x89, 0x7E, 0x04, 0x8B, 0xCF }, -1, true },                        // MOV [ESI+4],EDI; MOV ECX,EDI
    // docs/re/prologues_guardfix_faw.txt
    { "FUN_101752d0", "fire at will target choice (fix)", 0x1752D0, 7,
      { 0x83, 0xEC, 0x58, 0x8B, 0x4C, 0x24, 0x60 }, -1, false },           // SUB ESP,0x58; MOV ECX,[ESP+0x60]
    // docs/re/prologues_zombiefix_stop.txt (zombiefix's probe uses the same site; never both probes on)
    { "FUN_103daa80", "group attack command post", 0x3DAA80, 6,
      { 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x04 }, -1, true },                  // PUSH ESI; MOV ESI,ECX; MOV EAX,[ESI+4]
    // docs/re/prologues_guardfix_task.txt
    { "FUN_1012d660", "unit attack task constructor", 0x12D660, 5,
      { 0x56, 0xFF, 0x74, 0x24, 0x0C }, -1, true },                        // PUSH ESI; PUSH [ESP+0xC]
};

static HookSlot g_slots[HOOK_COUNT] = {};
extern "C" uint8_t* g_guardTrampolines[HOOK_COUNT] = {};

// ---------------------------------------------------------------------------
// The fix sites: CALL rel32 instructions in the missile attack task's state updates
// (docs/re/prologues_guardfix.txt). The whole instruction is checked; the displacement is
// rewritten to the wrapper and back.
// ---------------------------------------------------------------------------

enum SiteKind { K_OOR = 0, K_TGT = 1 };
enum {
    S_OOR_FIRE = 0, S_OOR_UNFORMED, S_OOR_TEST,
    S_TGT_FIRE, S_TGT_UNFORMED, S_TGT_TEST, S_TGT_INTERCEPT,
    SITE_COUNT
};

struct CallSite {
    const char* name;
    const char* role;
    uint32_t rva;         // the CALL instruction
    uint32_t calleeRva;   // its target in the analysed binary
    SiteKind kind;
    bool firing;          // inside FIRE / FIRE_UNFORMED: the target is (or was just) in range
};

static const CallSite g_sites[SITE_COUNT] = {
    { "FUN_100fa100+0x2E", "FIRE update: out of range, intercept?", 0x0FA12E, 0x1AEFC0, K_OOR, true },
    { "FUN_100fa1d0+0x2E", "FIRE_UNFORMED update: out of range, intercept?", 0x0FA1FE, 0x1AEFC0, K_OOR, true },
    { "FUN_100fa300+0x2E", "PERFORM_CAN_SHOOT_TEST update: out of range, intercept?", 0x0FA32E, 0x1AEFC0, K_OOR, false },
    { "FUN_100fa100+0x1B", "FIRE update: target invalid?", 0x0FA11B, 0x1B2460, K_TGT, true },
    { "FUN_100fa1d0+0x1B", "FIRE_UNFORMED update: target invalid?", 0x0FA1EB, 0x1B2460, K_TGT, true },
    { "FUN_100fa300+0x1F", "PERFORM_CAN_SHOOT_TEST update: target invalid?", 0x0FA31F, 0x1B2460, K_TGT, false },
    { "0x100f8070+0x1B", "INTERCEPT update: target invalid?", 0x0F808B, 0x1B2460, K_TGT, false },
};

static HookDef SiteDef(int i) {
    HookDef d = {};
    d.func = g_sites[i].name;
    d.role = g_sites[i].role;
    d.rva = g_sites[i].rva;
    d.len = 5;
    int32_t rel = (int32_t)(g_sites[i].calleeRva - (g_sites[i].rva + 5));
    d.expected[0] = 0xE8;
    memcpy(d.expected + 1, &rel, 4);
    d.relocOff = -1;
    d.probeOnly = false;
    return d;
}

static bool g_sitePatched[SITE_COUNT] = {};

// ---------------------------------------------------------------------------
// Engine layout (see the header)
// ---------------------------------------------------------------------------

static const uint32_t CTX_OFF_STATE = 0x04;                                  // machine context: current state
static const uint32_t TASK_CTX_OFF = 0x18, TASK_OFF_OWNER = 0x04;            // task: context, group (squad) / unit (unit task)
static const uint32_t MTASK_OFF_TARGET = 0x24, MTASK_OFF_NOFOLLOW = 0x2D;    // missile attack task: target unit, no-intercept byte
static const uint32_t MTASK_OFF_KEEPSTATE = 0x2C;
static const uint32_t GRP_OFF_COUNT = 0x18, GRP_OFF_MEN = 0x1C, GRP_OFF_UNIT = 0x510;
static const uint32_t GRP_OFF_OWNER = 0x20;                                  // group: behaviour stack owner
static const uint32_t OWN_OFF_STACK = 0x400, OWN_OFF_COUNT = 0x440, OWN_OFF_CUR = 0x454;
static const uint32_t SOL_OFF_GROUP = 0x3B4, SOL_OFF_UNIT = 0x214, SOL_OFF_POS = 0x48;
static const uint32_t U_OFF_LEVEL = 0xCE8, U_OFF_ROUT = 0xCEC, U_OFF_GUARD = 0xEB1;
static const uint32_t U_OFF_GA_N = 0xB8C, U_OFF_GA = 0xB90, U_OFF_MAIN = 0xB9C;
static const uint32_t U_OFF_Q = 0x454, U_Q_STRIDE = 0x90, U_OFF_Q_N = 0x9EC, U_OFF_Q_CUR = 0x9F0;
static const uint32_t LEVEL_SHATTERED = 7;
static const uint32_t MAX_MEN = 400;
static const uint32_t RVA_MISSILE_TASK_VT = 0x15C0F24;    // missile attack squad task (FUN_10127460)
static const uint32_t RVA_INTERCEPT_TASK_VT = 0x15C0A48;  // direct intercept squad task (FUN_101283f0)
static const uint32_t RVA_GROUPFIRE_TASK_VT = 0x15C1048;  // fire by rank group fire task (FUN_101295d0)
static const uint32_t RVA_ROWFIRE_TASK_VT = 0x15C0564;    // row fire task (FUN_10127d40)
static const uint32_t RVA_MELEE_TASK_VT = 0x15C0DFC;      // melee attack squad task (FUN_101273f0)
static const uint32_t RVA_YARI_TASK_VT = 0x15C82B4;       // yari wall attack task (FUN_101ceb40)

static inline uint32_t R32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint32_t Rd(uint32_t addr) { uint32_t v = 0; ReadDword(addr, &v); return v; }
static inline float RdF(uint32_t addr) { uint32_t v = Rd(addr); float f; memcpy(&f, &v, 4); return f; }
static inline uint8_t RdB(uint32_t addr) { uint8_t v = 0; ReadByte(addr, &v); return v; }
static inline uint32_t Rva(uint32_t a) { return a >= g_base && a < g_base + HOOK_ANALYSED_IMAGE_SIZE ? a - g_base : 0; }
static uint32_t Tick() { return GameTickAt(g_base); }

// A group: plausible men list whose first man links back to it.
static bool IsGroup(uint32_t g) {
    uint32_t n, a, s0;
    if (!PtrPlausible(g) || !ReadDword(g + GRP_OFF_COUNT, &n) || !ReadDword(g + GRP_OFF_MEN, &a)) return false;
    if (n == 0 || n > MAX_MEN || !PtrPlausible(a) || !ReadDword(a, &s0) || !PtrPlausible(s0)) return false;
    return Rd(s0 + SOL_OFF_GROUP) == g;
}

// Plausible battle unit: its main group or one of its listed groups is a group.
static bool IsBattleUnit(uint32_t u) {
    if (!PtrPlausible(u)) return false;
    if (IsGroup(Rd(u + U_OFF_MAIN))) return true;
    uint32_t n = Rd(u + U_OFF_GA_N), a = Rd(u + U_OFF_GA);
    return n > 0 && n <= 16 && PtrPlausible(a) && IsGroup(Rd(a));
}

static uint32_t UnitOfGroup(uint32_t g) { return IsGroup(g) ? Rd(Rd(Rd(g + GRP_OFF_MEN)) + SOL_OFF_UNIT) : 0; }

static bool GroupCentre(uint32_t g, uint32_t* alive, float* x, float* z) {
    uint32_t n = Rd(g + GRP_OFF_COUNT), men = Rd(g + GRP_OFF_MEN);
    *alive = 0;
    if (n == 0 || n > MAX_MEN || !PtrPlausible(men)) return false;
    double sx = 0, sz = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t s = Rd(men + 4 * i);
        if (!PtrPlausible(s)) continue;
        sx += RdF(s + SOL_OFF_POS);
        sz += RdF(s + SOL_OFF_POS + 8);
        (*alive)++;
    }
    if (!*alive) return false;
    *x = (float)(sx / *alive);
    *z = (float)(sz / *alive);
    return true;
}

// Distance between the main groups of two units, -1 when unknown.
static float UnitDistance(uint32_t a, uint32_t b) {
    uint32_t na, nb;
    float ax, az, bx, bz;
    if (!PtrPlausible(a) || !PtrPlausible(b)) return -1.0f;
    if (!GroupCentre(Rd(a + U_OFF_MAIN), &na, &ax, &az) || !GroupCentre(Rd(b + U_OFF_MAIN), &nb, &bx, &bz)) return -1.0f;
    float dx = ax - bx, dz = az - bz;
    return sqrtf(dx * dx + dz * dz);
}

// ---------------------------------------------------------------------------
// Fix state
// ---------------------------------------------------------------------------

static volatile LONG g_fixOn = 0;
static volatile LONG g_selfOff = 0;       // switched off by the self-check
static volatile LONG g_cCalls[SITE_COUNT] = {};
static volatile LONG g_cGuardYes = 0;     // "intercept" asked for a guard unit
static volatile LONG g_fNoChase = 0;      // intercept answered no (lever 1)
static volatile LONG g_fFresh = 0;        // intercept left to the game: target never in range
static volatile LONG g_fShattered = 0;    // shattered target answered invalid (lever 2)
static volatile LONG g_fStopped = 0;      // INTERCEPT under way stopped (target was in range)
static volatile LONG g_fBadGuard = 0;     // guard byte neither 0 nor 1
static volatile LONG g_fMarks = 0;        // (unit, target) pairs seen in range
static volatile LONG g_cFaw = 0;          // fire at will choices (all units)
static volatile LONG g_fIgnored = 0;      // shattered enemies taken out of a guard missile unit's choice (lever 2)
static bool g_keyWatched = false;
static volatile LONG g_testKeys = 0;
static uint8_t* g_toneOn = nullptr;
static uint8_t* g_toneOff = nullptr;
static uint8_t* g_toneRefused = nullptr;
static const LONG BAD_GUARD_LIMIT = 10;

static bool FixOn() { return InterlockedCompareExchange(&g_fixOn, 0, 0) != 0; }

// Pairs (unit, target) that the unit has had in range: its missile task was in FIRE or
// FIRE_UNFORMED against that target. Game thread only (the wrappers). A full table overwrites
// the oldest entry.
struct Mark { uint32_t unit, target, tick; };
static const int MARK_TABLE = 512;
static Mark g_marks[MARK_TABLE];
static int g_markNext = 0;

static bool Marked(uint32_t unit, uint32_t target) {
    for (int i = 0; i < MARK_TABLE; i++)
        if (g_marks[i].unit == unit && g_marks[i].target == target) return true;
    return false;
}

static void MarkInRange(uint32_t unit, uint32_t target) {
    uint32_t t = Tick();
    for (int i = 0; i < MARK_TABLE; i++)
        if (g_marks[i].unit == unit && g_marks[i].target == target) { g_marks[i].tick = t; return; }
    g_marks[g_markNext].unit = unit;
    g_marks[g_markNext].target = target;
    g_marks[g_markNext].tick = t;
    g_markNext = (g_markNext + 1) % MARK_TABLE;
    InterlockedIncrement(&g_fMarks);
}

// Units whose missile attack task passed one of the redirected calls: the missile units (fire at
// will's list is filtered for these only). Game thread only.
static const int MISSILE_TABLE = 256;
static uint32_t g_missile[MISSILE_TABLE];
static int g_missileN = 0;

static bool IsMissileUnit(uint32_t u) {
    for (int i = 0; i < g_missileN; i++) if (g_missile[i] == u) return true;
    return false;
}

static void NoteMissileUnit(uint32_t u) {
    if (IsMissileUnit(u)) return;
    if (g_missileN < MISSILE_TABLE) g_missile[g_missileN++] = u;
    else g_missile[(g_missileN++) % MISSILE_TABLE] = u;
    if (g_missileN >= 2 * MISSILE_TABLE) g_missileN = MISSILE_TABLE;
}

// A new battle (the battle tick went back): forget the in-range pairs and the missile units, whose
// addresses the next battle may reuse. Game thread only.
static uint32_t g_lastTick = 0;
static void BattleCheck() {
    uint32_t t = Tick();
    if (t < g_lastTick) {
        memset(g_marks, 0, sizeof(g_marks));
        g_markNext = 0;
        memset(g_missile, 0, sizeof(g_missile));
        g_missileN = 0;
    }
    g_lastTick = t;
}

// Guard mode of a unit: 1 / 0, or -1 when the byte is not a bool (counted; the self-check).
static void SelfCheckFailed();
static int GuardOf(uint32_t unit) {
    uint8_t g;
    if (!ReadByte(unit + U_OFF_GUARD, &g)) return -1;
    if (g > 1) {
        if (InterlockedIncrement(&g_fBadGuard) == BAD_GUARD_LIMIT) SelfCheckFailed();
        return -1;
    }
    return g;
}

// ---------------------------------------------------------------------------
// Probe files
// ---------------------------------------------------------------------------

static bool g_probe = false;
static CRITICAL_SECTION g_cs;      // files
static char g_dir[MAX_PATH] = "";
static FILE* g_fStates = nullptr;
static FILE* g_fCalls = nullptr;
static FILE* g_fPos = nullptr;
static FILE* g_fOrders = nullptr;
static FILE* g_fPosts = nullptr;
static LARGE_INTEGER g_qpf = {};
static volatile LONG g_rows = 0;
static const LONG MAX_ROWS = 6000000;
static volatile LONG g_cTrans = 0, g_cTransLogged = 0, g_cCallRows = 0, g_cPos = 0, g_cOrders = 0, g_cPosts = 0;
static DWORD g_lastFlush = 0;

static uint32_t Ms() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return g_qpf.QuadPart ? (uint32_t)(now.QuadPart * 1000 / g_qpf.QuadPart) : GetTickCount();
}

static bool RowBudget() { return InterlockedIncrement(&g_rows) < MAX_ROWS; }

static FILE* OpenCsv(const char* name, const char* header) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", g_dir, name);
    FILE* f = fopen(path, "w");
    if (!f) return nullptr;
    setvbuf(f, nullptr, _IOFBF, 1 << 20);
    fputs(header, f);
    return f;
}

static bool OpenProbe() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    char* slash = strrchr(exe, '\\');
    if (slash) slash[1] = 0;
    char root[MAX_PATH];
    snprintf(root, sizeof(root), "%sguardfix_runs\\", exe);
    CreateDirectoryA(root, nullptr);
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_dir, sizeof(g_dir), "%s%04d%02d%02d_%02d%02d%02d\\", root, st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute, st.wSecond);
    if (!CreateDirectoryA(g_dir, nullptr)) { Log("guardfix: cannot create %s", g_dir); return false; }
    g_fStates = OpenCsv("guard_states.csv",
                        "ms,tick,kind,owner,ctx,unit,group,guard,level,rout,old,old_state,new,new_state,target,t_level,"
                        "t_rout,b2c,b2d,dist\n");
    g_fCalls = OpenCsv("guard_calls.csv",
                       "ms,tick,site,task,unit,guard,target,t_level,t_rout,b2d,game,answer,marked,dist\n");
    g_fPos = OpenCsv("guard_pos.csv", "tick,unit,main,guard,men,x,z,level,rout,qn,qcur,qvt,stack\n");
    g_fOrders = OpenCsv("guard_orders.csv", "tick,unit,qn,qcur,hex\n");
    g_fPosts = OpenCsv("guard_posts.csv", "ms,tick,event,ret,c1,c2,c3,c4,group,unit,guard,target,t_level,t_rout,dist,w1,w2,w3\n");
    if (!g_fStates || !g_fCalls || !g_fPos || !g_fOrders || !g_fPosts) {
        Log("guardfix: cannot open the probe files in %s", g_dir);
        return false;
    }
    Log("guardfix: probe on, writing %s", g_dir);
    return true;
}

static void FlushAll() {
    FILE* fs[] = { g_fStates, g_fCalls, g_fPos, g_fOrders, g_fPosts };
    for (FILE* f : fs) if (f) fflush(f);
}

// Under g_cs.
static void MaybeFlush() {
    DWORD now = GetTickCount();
    if (now - g_lastFlush < 2000) return;
    g_lastFlush = now;
    FlushAll();
}

static void Hex(const uint8_t* b, uint32_t n, char* out) {
    static const char* hx = "0123456789ABCDEF";
    for (uint32_t i = 0; i < n; i++) { out[2 * i] = hx[b[i] >> 4]; out[2 * i + 1] = hx[b[i] & 15]; }
    out[2 * n] = 0;
}

// ---------------------------------------------------------------------------
// State names: slot +0x10 of a state's vtable is "MOV EAX,[table]; RET"; [table] is the name
// (docs/re/missile_fire_x86.md). Cached per state object.
// ---------------------------------------------------------------------------

struct StateName { uint32_t obj; char name[56]; };
static const int STATE_TABLE = 2048;
static StateName g_names[STATE_TABLE];
static CRITICAL_SECTION g_nameCs;

static bool ReadStateName(uint32_t obj, char* out, int cap) {
    uint32_t vt, fn, table, str;
    uint8_t code[6];
    if (!ReadDword(obj, &vt) || !ReadDword(vt + 0x10, &fn) || !MemcpyTry(code, fn, 6)) return false;
    if (code[0] != 0xA1 || code[5] != 0xC3) return false;
    memcpy(&table, code + 1, 4);
    if (!ReadDword(table, &str) || !MemcpyTry(out, str, cap - 1)) return false;
    out[cap - 1] = 0;
    for (int i = 0; i < cap - 1 && out[i]; i++) {
        char c = out[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || (c >= 'a' && c <= 'z'))) return false;
    }
    return out[0] != 0;
}

static void NameOf(uint32_t obj, char* out, int cap) {
    if (!obj) { snprintf(out, cap, "-"); return; }
    uint32_t h = ((obj >> 2) * 2654435761u) & (STATE_TABLE - 1);
    EnterCriticalSection(&g_nameCs);
    for (int i = 0; i < 32; i++) {
        StateName& s = g_names[(h + i) & (STATE_TABLE - 1)];
        if (s.obj == obj) { snprintf(out, cap, "%s", s.name); LeaveCriticalSection(&g_nameCs); return; }
        if (s.obj == 0) {
            s.obj = obj;
            if (!ReadStateName(obj, s.name, sizeof(s.name))) snprintf(s.name, sizeof(s.name), "?%X", Rva(Rd(obj)));
            snprintf(out, cap, "%s", s.name);
            LeaveCriticalSection(&g_nameCs);
            return;
        }
    }
    LeaveCriticalSection(&g_nameCs);
    snprintf(out, cap, "?");
}

// "vtRVA:STATE" of a task (its machine context at +0x18, the state at context +4).
static void DescTask(uint32_t task, char* out, int cap) {
    if (!PtrPlausible(task)) { snprintf(out, cap, "-"); return; }
    char st[64];
    NameOf(Rd(task + TASK_CTX_OFF + CTX_OFF_STATE), st, sizeof(st));
    snprintf(out, cap, "%X:%s", Rva(Rd(task)), st);
}

static bool DescStack(uint32_t group, char* out, int cap) {
    out[0] = 0;
    uint32_t owner = group + GRP_OFF_OWNER;
    if (!PtrPlausible(group)) return false;
    uint32_t n = Rd(owner + OWN_OFF_COUNT), cur = Rd(owner + OWN_OFF_CUR);
    if (n > 8) return false;
    int k = snprintf(out, cap, "%u/%u", cur, n);
    for (uint32_t i = 0; i < n && k < cap - 80; i++) {
        char t[128];
        DescTask(Rd(owner + OWN_OFF_STACK + 8 * i), t, sizeof(t));
        k += snprintf(out + k, cap - k, "|%s", t);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Unit registry (probe): every battle unit met through a task transition or a redirected call
// ---------------------------------------------------------------------------

struct UnitRec {
    uint32_t u;
    uint32_t lastPos;
    char order[2 * U_Q_STRIDE + 32];   // last order queue head text
};
static const int MAX_UNITS = 128;
static UnitRec g_units[MAX_UNITS];
static volatile LONG g_nUnits = 0;
static CRITICAL_SECTION g_unitCs;
static const int KNOWN_TABLE = 1024;
static volatile LONG g_known[KNOWN_TABLE];

static bool KnownUnit(uint32_t u, bool add) {
    uint32_t h = ((u >> 3) * 2654435761u) & (KNOWN_TABLE - 1);
    for (int i = 0; i < 64; i++) {
        volatile LONG* slot = &g_known[(h + i) & (KNOWN_TABLE - 1)];
        LONG v = *slot;
        if ((uint32_t)v == u) return true;
        if (v == 0) {
            if (!add) return false;
            LONG prev = InterlockedCompareExchange(slot, (LONG)u, 0);
            if (prev == 0 || (uint32_t)prev == u) return false;
        }
    }
    return false;
}

static void NoteUnit(uint32_t u) {
    if (!g_probe || !u || KnownUnit(u, false) || !IsBattleUnit(u)) return;
    EnterCriticalSection(&g_unitCs);
    bool found = false;
    for (int i = 0; i < g_nUnits; i++) if (g_units[i].u == u) found = true;
    if (!found && g_nUnits < MAX_UNITS) {
        UnitRec& r = g_units[g_nUnits];
        memset(&r, 0, sizeof(r));
        r.u = u;
        InterlockedIncrement(&g_nUnits);
    }
    LeaveCriticalSection(&g_unitCs);
    KnownUnit(u, true);
}

// ---------------------------------------------------------------------------
// Transitions (probe)
// ---------------------------------------------------------------------------

static bool TaskOfInterest(uint32_t vtRva) {
    return vtRva == RVA_MISSILE_TASK_VT || vtRva == RVA_INTERCEPT_TASK_VT || vtRva == RVA_GROUPFIRE_TASK_VT ||
           vtRva == RVA_ROWFIRE_TASK_VT || vtRva == RVA_MELEE_TASK_VT || vtRva == RVA_YARI_TASK_VT;
}

// Per machine context: transitions seen (sampling of the tasks not of interest).
struct CtxCount { uint32_t ctx, n; };
static const int CTX_TABLE = 1 << 15;
static CtxCount* g_ctx = nullptr;
static CRITICAL_SECTION g_ctxCs;

static uint32_t CountCtx(uint32_t ctx) {
    uint32_t h = ((ctx >> 2) * 2654435761u) & (CTX_TABLE - 1);
    EnterCriticalSection(&g_ctxCs);
    for (int i = 0; i < 64; i++) {
        CtxCount& c = g_ctx[(h + i) & (CTX_TABLE - 1)];
        if (c.ctx == ctx || c.ctx == 0) {
            c.ctx = ctx;
            uint32_t n = ++c.n;
            LeaveCriticalSection(&g_ctxCs);
            return n;
        }
    }
    LeaveCriticalSection(&g_ctxCs);
    return 0;
}

// FUN_11374cf0+0x1B: ESI = machine context, EDI = the next state; [ctx+4] still the old one.
// Tasks: context at task+0x18, owner at task+4 (a group for squad tasks, a unit for unit tasks).
static void HandleTransition(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cTrans);
    uint32_t mctx = R32(ctx + GUARD_OFF_ESI), next = R32(ctx + GUARD_OFF_EDI);
    uint32_t task = mctx - TASK_CTX_OFF, own = Rd(task + TASK_OFF_OWNER);
    uint32_t vt = Rva(Rd(task));
    char kind;
    uint32_t unit = 0, group = 0;
    if (IsGroup(own)) {
        kind = 'T';
        group = own;
        unit = UnitOfGroup(own);
    } else if (IsBattleUnit(own)) {
        kind = 'V';
        unit = own;
    } else {
        return;
    }
    NoteUnit(unit);
    bool interest = kind == 'V' || TaskOfInterest(vt);
    uint32_t n = CountCtx(mctx);
    if (!interest && n > 20 && n % 50 != 0) return;
    if (!RowBudget()) return;
    uint32_t old = Rd(mctx + CTX_OFF_STATE);
    char so[64], sn[64];
    NameOf(old, so, sizeof(so));
    NameOf(next, sn, sizeof(sn));
    uint32_t target = 0, b2c = 0, b2d = 0;
    float dist = -1.0f;
    if (vt == RVA_MISSILE_TASK_VT) {
        target = Rd(task + MTASK_OFF_TARGET);
        b2c = RdB(task + MTASK_OFF_KEEPSTATE);
        b2d = RdB(task + MTASK_OFF_NOFOLLOW);
        NoteUnit(target);
        dist = UnitDistance(unit, target);
    }
    InterlockedIncrement(&g_cTransLogged);
    EnterCriticalSection(&g_cs);
    if (g_fStates)
        fprintf(g_fStates, "%u,%u,%c,%X,%X,%X,%X,%d,%u,%u,%X,%s,%X,%s,%X,%u,%u,%u,%u,%.1f\n", Ms(), Tick(), kind, vt, mctx,
                unit, group, (int)RdB(unit + U_OFF_GUARD), Rd(unit + U_OFF_LEVEL), Rd(unit + U_OFF_ROUT), Rva(Rd(old)), so,
                Rva(Rd(next)), sn, target, target ? Rd(target + U_OFF_LEVEL) : 0, target ? Rd(target + U_OFF_ROUT) : 0, b2c,
                b2d, dist);
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// The redirected calls (game thread)
// ---------------------------------------------------------------------------

typedef uint32_t (__fastcall* TestFn)(uint32_t task);

// Last logged answer per (task, site): probe rows only on changes.
struct CallSeen { uint32_t task; uint8_t game[SITE_COUNT], answer[SITE_COUNT]; };
static const int SEEN_TABLE = 1024;
static CallSeen g_seen[SEEN_TABLE];

static void CallRow(int site, uint32_t task, uint32_t unit, int guard, uint32_t target, uint32_t game, uint32_t answer,
                    bool marked) {
    if (!g_probe) return;
    uint32_t h = ((task >> 3) * 2654435761u) & (SEEN_TABLE - 1);
    CallSeen& s = g_seen[h];
    if (s.task != task) {
        memset(&s, 0xFF, sizeof(s));
        s.task = task;
    }
    if (s.game[site] == game && s.answer[site] == answer && game == answer) return;
    s.game[site] = (uint8_t)game;
    s.answer[site] = (uint8_t)answer;
    if (!RowBudget()) return;
    NoteUnit(unit);
    NoteUnit(target);
    float dist = UnitDistance(unit, target);
    InterlockedIncrement(&g_cCallRows);
    EnterCriticalSection(&g_cs);
    if (g_fCalls)
        fprintf(g_fCalls, "%u,%u,%d,%X,%X,%d,%X,%u,%u,%u,%u,%u,%d,%.1f\n", Ms(), Tick(), site, task, unit, guard, target,
                target ? Rd(target + U_OFF_LEVEL) : 0, target ? Rd(target + U_OFF_ROUT) : 0,
                RdB(task + MTASK_OFF_NOFOLLOW), game, answer, marked ? 1 : 0, dist);
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

static uint32_t HandleSite(int site, uint32_t task) {
    InterlockedIncrement(&g_cCalls[site]);
    const CallSite& cs = g_sites[site];
    uint32_t game = ((TestFn)(uintptr_t)(g_base + cs.calleeRva))(task) & 0xFF;
    // the INTERCEPT update is shared with the naval missile task: only the land missile task
    if (Rva(Rd(task)) != RVA_MISSILE_TASK_VT) return game;
    uint32_t unit = Rd(Rd(task + TASK_OFF_OWNER) + GRP_OFF_UNIT);
    uint32_t target = Rd(task + MTASK_OFF_TARGET);
    if (!PtrPlausible(unit) || !PtrPlausible(target)) return game;
    int guard = GuardOf(unit);
    BattleCheck();
    NoteMissileUnit(unit);
    if (cs.firing && guard == 1) MarkInRange(unit, target);
    uint32_t answer = game;
    bool marked = false;
    if (guard == 1 && FixOn()) {
        if (cs.kind == K_OOR && game) {
            InterlockedIncrement(&g_cGuardYes);
            marked = cs.firing || Marked(unit, target);
            if (marked) { answer = 0; InterlockedIncrement(&g_fNoChase); }
            else InterlockedIncrement(&g_fFresh);
        } else if (cs.kind == K_TGT && !game) {
            if (Rd(target + U_OFF_LEVEL) == LEVEL_SHATTERED) {
                answer = 1;
                InterlockedIncrement(&g_fShattered);
            } else if (site == S_TGT_INTERCEPT && Marked(unit, target)) {
                marked = true;
                answer = 1;
                InterlockedIncrement(&g_fStopped);
            }
        }
    } else if (guard == 1 && g_probe) {
        marked = Marked(unit, target);
    }
    CallRow(site, task, unit, guard, target, game, answer, marked);
    return answer;
}

static uint32_t __fastcall Site0(uint32_t task) { return HandleSite(0, task); }
static uint32_t __fastcall Site1(uint32_t task) { return HandleSite(1, task); }
static uint32_t __fastcall Site2(uint32_t task) { return HandleSite(2, task); }
static uint32_t __fastcall Site3(uint32_t task) { return HandleSite(3, task); }
static uint32_t __fastcall Site4(uint32_t task) { return HandleSite(4, task); }
static uint32_t __fastcall Site5(uint32_t task) { return HandleSite(5, task); }
static uint32_t __fastcall Site6(uint32_t task) { return HandleSite(6, task); }
static TestFn const g_wrappers[SITE_COUNT] = { Site0, Site1, Site2, Site3, Site4, Site5, Site6 };

// ---------------------------------------------------------------------------
// Fire at will's choice (game thread): FUN_101752d0 (cdecl: the unit, a list of enemy units in
// missile range with its count at +0x2800, the current target(s), the range, ...) scores every
// enemy in the list (nearer is better, the current target gets a bonus, a routing enemy only a
// penalty) and returns the best. The unit interrupt machine's FIRE_AT_WILL_INTERRUPT_STATE starts
// a unit attack task against that choice (FUN_1016aa00 -> context +0x1C, enter FUN_10170990).
// For a missile unit in guard mode the shattered enemies (level 7) are taken out of the list
// before the game scores it, so fire at will takes another enemy in range or holds fire. The
// callers rebuild the list for every call (a stack buffer in FUN_1016aa00 / FUN_10178300, a
// global whose count FUN_10177e30 resets afterwards), so the list is changed in place.
// ---------------------------------------------------------------------------

static const uint32_t FAW_OFF_COUNT = 0x2800, FAW_MAX = 0xA00;

static void HandleFaw(const uint8_t* ctx) {
    InterlockedIncrement(&g_cFaw);
    if (!FixOn()) return;
    uint32_t unit = R32(ctx + GUARD_OFF_ARG1), list = R32(ctx + GUARD_OFF_ARG2), n;
    if (!PtrPlausible(unit) || !PtrPlausible(list) || !ReadDword(list + FAW_OFF_COUNT, &n) || n == 0 || n > FAW_MAX)
        return;
    BattleCheck();
    if (!IsMissileUnit(unit) || GuardOf(unit) != 1) return;
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t e = Rd(list + 4 * i);
        if (PtrPlausible(e) && Rd(e + U_OFF_LEVEL) == LEVEL_SHATTERED) { InterlockedIncrement(&g_fIgnored); continue; }
        if (k != i) WriteDwordTry(list + 4 * k, e);
        k++;
    }
    if (k != n) WriteDwordTry(list + FAW_OFF_COUNT, k);
}

// FUN_103daa80 (probe): who posts attack commands, with what target. c1..c4: the first code
// addresses of the game module found on the stack above the return address (a rough backtrace).
static void PostRow(const uint8_t* ctx, const char* ev, uint32_t g, uint32_t u, uint32_t cmd, uint32_t nargs);

static void HandlePost(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t g = R32(ctx + GUARD_OFF_ECX), cmd = R32(ctx + GUARD_OFF_ARG1);
    if (!IsGroup(g)) return;
    PostRow(ctx, "post", g, UnitOfGroup(g), cmd, 1);
}

// FUN_1012d660 (probe): a unit attack task built (arg 2 = the unit, arg 3 = {target, ...}).
static void HandleUnitTask(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t u = R32(ctx + GUARD_OFF_ARG2), cmd = R32(ctx + GUARD_OFF_ARG3);
    if (!IsBattleUnit(u)) {
        uint32_t a1 = R32(ctx + GUARD_OFF_ARG1);
        if (IsBattleUnit(a1)) u = a1;
        else return;
    }
    PostRow(ctx, "utask", 0, u, cmd, 3);
}

static void PostRow(const uint8_t* ctx, const char* ev, uint32_t g, uint32_t u, uint32_t cmd, uint32_t nargs) {
    uint32_t target = Rd(cmd);
    uint32_t c[4] = {}, k = 0, sp = R32(ctx + GUARD_OFF_ESP_BEFORE_PUSHAD) + 4 + 4 * nargs;   // above ret and args
    for (uint32_t i = 0; i < 96 && k < 4; i++) {
        uint32_t v = Rd(sp + 4 * i), r = Rva(v);
        if (r >= 0x1000 && r < 0x1700000) c[k++] = r;
    }
    NoteUnit(u);
    NoteUnit(target);
    if (!RowBudget()) return;
    InterlockedIncrement(&g_cPosts);
    EnterCriticalSection(&g_cs);
    if (g_fPosts)
        fprintf(g_fPosts, "%u,%u,%s,%X,%X,%X,%X,%X,%X,%X,%d,%X,%u,%u,%.1f,%X,%X,%X\n", Ms(), Tick(), ev,
                Rva(R32(ctx + GUARD_OFF_RET)), c[0], c[1], c[2], c[3], g, u, (int)RdB(u + U_OFF_GUARD), target,
                PtrPlausible(target) ? Rd(target + U_OFF_LEVEL) : 0, PtrPlausible(target) ? Rd(target + U_OFF_ROUT) : 0,
                UnitDistance(u, target), Rd(cmd + 4), Rd(cmd + 8), Rd(cmd + 12));
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// Sampler thread (probe): positions, order queue heads
// ---------------------------------------------------------------------------

static HANDLE g_sampler = nullptr;
static volatile LONG g_stop = 0;

static void SampleUnit(UnitRec& r, uint32_t tick) {
    uint32_t u = r.u;
    uint32_t qn = Rd(u + U_OFF_Q_N), qc = Rd(u + U_OFF_Q_CUR);
    uint32_t head = qn && qc < 16 ? u + U_OFF_Q + qc * U_Q_STRIDE : 0;
    char order[sizeof(r.order)];
    if (head) {
        uint8_t b[U_Q_STRIDE] = {};
        CopyGuarded(head, b, U_Q_STRIDE);
        int k = snprintf(order, sizeof(order), "%u,%u,", qn, qc);
        Hex(b, U_Q_STRIDE, order + k);
    } else {
        snprintf(order, sizeof(order), "%u,%u,", qn, qc);
    }
    if (strcmp(order, r.order) != 0) {
        memcpy(r.order, order, sizeof(order));
        if (RowBudget()) {
            EnterCriticalSection(&g_cs);
            if (g_fOrders) fprintf(g_fOrders, "%u,%X,%s\n", tick, u, order);
            InterlockedIncrement(&g_cOrders);
            LeaveCriticalSection(&g_cs);
        }
    }
    if (tick >= r.lastPos + 5 || tick < r.lastPos) {
        r.lastPos = tick;
        uint32_t main = Rd(u + U_OFF_MAIN), alive = 0;
        float x = 0, z = 0;
        GroupCentre(main, &alive, &x, &z);
        char stack[384];
        DescStack(main, stack, sizeof(stack));
        if (RowBudget()) {
            EnterCriticalSection(&g_cs);
            if (g_fPos)
                fprintf(g_fPos, "%u,%X,%X,%u,%u,%.2f,%.2f,%u,%u,%u,%u,%X,%s\n", tick, u, main, RdB(u + U_OFF_GUARD), alive,
                        x, z, Rd(u + U_OFF_LEVEL), Rd(u + U_OFF_ROUT), qn, qc, head ? Rva(Rd(head)) : 0, stack);
            InterlockedIncrement(&g_cPos);
            LeaveCriticalSection(&g_cs);
        }
    }
}

static DWORD WINAPI SamplerThread(LPVOID) {
    while (!InterlockedCompareExchange(&g_stop, 0, 0)) {
        Sleep(50);
        uint32_t tick = Tick();
        if (!tick) continue;
        int n = g_nUnits;
        for (int i = 0; i < n; i++) {
            EnterCriticalSection(&g_unitCs);
            SampleUnit(g_units[i], tick);
            LeaveCriticalSection(&g_unitCs);
        }
        EnterCriticalSection(&g_cs);
        MaybeFlush();
        LeaveCriticalSection(&g_cs);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Key, self-check, detours, harness exports
// ---------------------------------------------------------------------------

static void SelfCheckFailed() {
    InterlockedExchange(&g_selfOff, 1);
    InterlockedExchange(&g_fixOn, 0);
    Log("guardfix: SELF-CHECK, %ld guard bytes that are not 0 or 1: fix switched off for this session", g_fBadGuard);
    Tone_Play(g_toneRefused);
}

// I toggles the fix (dev build; focus only through proxy/keys). Harness: GuardFixTestKeys bit 0.
extern "C" __declspec(dllexport) void __stdcall GuardFixTestKeys(LONG bits) { InterlockedExchange(&g_testKeys, bits); }

static void ToggleKey() {
    if (InterlockedCompareExchange(&g_selfOff, 0, 0)) {
        Log("guardfix: I, refused (switched off by the self-check)");
        Tone_Play(g_toneRefused);
        return;
    }
    bool on = !FixOn();
    InterlockedExchange(&g_fixOn, on ? 1 : 0);
    Log("guardfix: I, fix %s (so far: chases refused %ld, shattered targets dropped %ld, intercepts stopped %ld)",
        on ? "ON" : "OFF", g_fNoChase, g_fShattered, g_fStopped);
    Tone_Play(on ? g_toneOn : g_toneOff);
}

extern "C" void __stdcall GuardHandler0(const uint8_t* ctx) { HandleTransition(ctx); }
extern "C" void __stdcall GuardHandler1(const uint8_t* ctx) { HandleFaw(ctx); }
extern "C" void __stdcall GuardHandler2(const uint8_t* ctx) { HandlePost(ctx); }
extern "C" void __stdcall GuardHandler3(const uint8_t* ctx) { HandleUnitTask(ctx); }
HOOK_DETOUR(GuardDetour0, GuardHandler0, g_guardTrampolines, 0)
HOOK_DETOUR(GuardDetour1, GuardHandler1, g_guardTrampolines, 1)
HOOK_DETOUR(GuardDetour2, GuardHandler2, g_guardTrampolines, 2)
HOOK_DETOUR(GuardDetour3, GuardHandler3, g_guardTrampolines, 3)
static const HookDetour g_detours[HOOK_COUNT] = { GuardDetour0, GuardDetour1, GuardDetour2, GuardDetour3 };

// Harness only (fixes/guard-mode-ranged-fix/tests): fix and probe counters.
extern "C" __declspec(dllexport) void __stdcall GuardFixTestCounters(long* out) {
    out[0] = g_probe ? 1 : 0;
    out[1] = g_fixOn;
    out[2] = g_base ? 1 : 0;
    out[3] = g_cGuardYes;
    out[4] = g_fNoChase;
    out[5] = g_fFresh;
    out[6] = g_fShattered;
    out[7] = g_fStopped;
    out[8] = g_fBadGuard;
    out[9] = g_selfOff;
    out[10] = g_fMarks;
    long calls = 0;
    for (int i = 0; i < SITE_COUNT; i++) calls += g_cCalls[i];
    out[11] = calls;
    out[12] = g_cTrans;
    out[13] = g_cTransLogged;
    out[14] = g_cCallRows;
    out[15] = g_nUnits;
    int patched = 0;
    for (int i = 0; i < SITE_COUNT; i++) patched += g_sitePatched[i] ? 1 : 0;
    out[16] = patched;
    out[17] = g_cFaw;
    out[18] = g_fIgnored;
    out[19] = g_cPosts;
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
        WriteRel(i, g_base + g_sites[i].calleeRva);
        g_sitePatched[i] = false;
    }
}

void GuardFix_Install() {
    Log("guardfix: guard mode ranged fix (dev build)");
    Mem_Init();
    uint32_t size = 0;
    uint8_t* base = Hook_GameModule(false, &size);
    if (!base) { Log("guardfix: REFUSED, empire.retail.dll is not loaded"); return; }
    if (size != HOOK_ANALYSED_IMAGE_SIZE) { Log("guardfix: REFUSED, SizeOfImage 0x%08X is not the analysed build", size); return; }
    for (int i = 0; i < SITE_COUNT; i++) {
        HookDef d = SiteDef(i);
        if (!Hook_SiteMatches(base, d)) {
            Log("guardfix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", d.rva, d.func);
            return;
        }
    }
    bool probeWanted = EnvIs("GUARDFIX_PROBE", '1');
    for (int h = 0; h < HOOK_COUNT; h++) {
        if (g_hookDefs[h].probeOnly && !probeWanted) continue;
        if (!Hook_SiteMatches(base, g_hookDefs[h])) {
            Log("guardfix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary (another probe on?)",
                g_hookDefs[h].rva, g_hookDefs[h].func);
            return;
        }
    }
    g_base = (uint32_t)(uintptr_t)base;
    InitializeCriticalSection(&g_cs);
    InitializeCriticalSection(&g_nameCs);
    InitializeCriticalSection(&g_unitCs);
    InitializeCriticalSection(&g_ctxCs);
    QueryPerformanceFrequency(&g_qpf);
    static const ToneSeg on[] = { { 880, 160 } };
    static const ToneSeg off[] = { { 330, 300 } };
    static const ToneSeg refused[] = { { 200, 450 }, { 0, 150 }, { 200, 450 }, { 0, 150 }, { 200, 450 } };
    g_toneOn = Tone_Make(on, 1);
    g_toneOff = Tone_Make(off, 1);
    g_toneRefused = Tone_Make(refused, 5);
#ifdef GUARDFIX_START_OFF
    bool fixOff = !EnvIs("GUARDFIX_ON", '1');      // live-check build: off at launch, I turns it on
    const char* offWhy = "OFF at launch (build option GUARDFIX_START_OFF)";
#else
    bool fixOff = EnvIs("GUARDFIX_OFF", '1');
    const char* offWhy = "OFF (GUARDFIX_OFF=1)";
#endif
    if (probeWanted) {
        g_ctx = (CtxCount*)VirtualAlloc(nullptr, sizeof(CtxCount) * CTX_TABLE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (g_ctx && OpenProbe()) g_probe = true;
        else Log("guardfix: probe not started (no run folder or no memory)");
    }
    bool use[HOOK_COUNT];
    for (int h = 0; h < HOOK_COUNT; h++) use[h] = !g_hookDefs[h].probeOnly || g_probe;
    if (!Hook_Prepare(base, g_hookDefs, HOOK_COUNT, use, g_guardTrampolines, g_slots)) {
        Log("guardfix: REFUSED, no trampoline memory");
        g_probe = false;
        g_base = 0;
        return;
    }
    InterlockedExchange(&g_fixOn, fixOff ? 0 : 1);
    for (int i = 0; i < SITE_COUNT; i++) {
        if (!WriteRel(i, (uint32_t)(uintptr_t)g_wrappers[i])) {
            Log("guardfix: REFUSED, cannot write the call at RVA 0x%06X; restoring", g_sites[i].rva);
            UnpatchSites();
            g_probe = false;
            g_base = 0;
            return;
        }
        g_sitePatched[i] = true;
        Log("guardfix: call %s (%s) at RVA 0x%06X redirected", g_sites[i].name, g_sites[i].role, g_sites[i].rva);
    }
    if (g_probe) g_sampler = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    Hook_Patch(g_detours, g_slots, HOOK_COUNT);
    for (int h = 0; h < HOOK_COUNT; h++)
        if (g_slots[h].active)
            Log("guardfix: hook %s (%s) installed at RVA 0x%06X", g_hookDefs[h].func, g_hookDefs[h].role, g_hookDefs[h].rva);
    Log("guardfix: fix %s: a missile unit in guard mode lets a target go once it leaves its range and drops shattered "
        "targets", fixOff ? offWhy : "ON");
    g_keyWatched = Keys_Watch('I', &g_testKeys, 1, ToggleKey);
    Log("guardfix: I toggles the fix on / off%s", g_keyWatched ? "" : " (ERROR: key poll not started)");
}

void GuardFix_Remove() {
    if (g_keyWatched) { Keys_Unwatch(ToggleKey); g_keyWatched = false; }
    Hook_Remove(g_slots, HOOK_COUNT);
    if (!g_base) return;
    UnpatchSites();
    long calls = 0;
    for (int i = 0; i < SITE_COUNT; i++) calls += g_cCalls[i];
    Log("guardfix: missile task tests %ld; guard units asking to intercept %ld: refused %ld, left to the game (target "
        "never in range) %ld; shattered targets dropped %ld; intercepts stopped %ld; fire at will choices %ld, shattered "
        "enemies left out of guard missile units' fire at will %ld; guard bytes not 0 / 1 %ld%s",
        calls, g_cGuardYes, g_fNoChase, g_fFresh, g_fShattered, g_fStopped, g_cFaw, g_fIgnored, g_fBadGuard,
        g_selfOff ? " (fix switched off by the self-check)" : "");
    if (g_sampler) {
        InterlockedExchange(&g_stop, 1);
        WaitForSingleObject(g_sampler, 2000);
        CloseHandle(g_sampler);
        g_sampler = nullptr;
    }
    if (g_probe) {
        EnterCriticalSection(&g_cs);
        FILE** fs[] = { &g_fStates, &g_fCalls, &g_fPos, &g_fOrders, &g_fPosts };
        for (FILE** f : fs) if (*f) { fclose(*f); *f = nullptr; }
        g_probe = false;
        LeaveCriticalSection(&g_cs);
        Log("guardfix: probe closed: transitions %ld (logged %ld), call rows %ld, units %ld, position rows %ld, order rows "
            "%ld, rows %ld", g_cTrans, g_cTransLogged, g_cCallRows, g_nUnits, g_cPos, g_cOrders, g_rows);
    }
}
