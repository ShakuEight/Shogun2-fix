// zombiefix.cpp -- zombie yari wall fix for Total War: Shogun 2 (original x86 Empire.Retail.dll).
// Folder fixes/zombie-yari-wall-fix/, code name zombiefix.
//
// Dev build. Fault: yari ashigaru in yari wall break and shatter, but now and then a unit does
// not run: it stands on the spot, out of the fight for the morale system, and its yari wall
// keeps killing whatever touches it.
//
// Engine decode (fixes/zombie-yari-wall-fix/ARCHITECTURE.md, docs/re/rout_x86.md):
// - A battle unit (the object a soldier points at with +0x214) holds the morale block at +0xCC4:
//   morale level +0xCE8 (5 wavering, 6 broken, 7 shattered), rout state +0xCEC (2 or 3 = routing
//   for the script condition "routing"), rally byte +0xD10; its kill count at +0xD88. Its soldier
//   groups are listed at +0xB90 (count +0xB8C); +0xB9C is the main group (most men).
// - A group (squad tasks: task+4; a soldier's +0x3B4) has its men at +0x1C (count +0x18) and its
//   behaviour stack at group+0x20+0x400 (8-byte entries, count +0x440, current +0x454).
// - The unit's interrupt machine lives at unit +0xE78 (its context's +0x10 = the unit). It enters
//   ROUT_INTERRUPT_STATE when the unit counts as routing; the enter (FUN_10170cc0) clears the
//   unit's order queue and its own task stack; every tick (FUN_101bd3b0) sets up each group whose
//   byte +0x2781 is 0 (FUN_101a9bf0: byte = 1, move cancelled, behaviour stack cleared, the rout
//   task, vtable 0x115E01F4, pushed). The exit clears the byte again.
//
// Cause: a group is set up once. When a unit breaks in the middle of a charge order, a melee
// attack task (0x115C0DFC) starts on the group in the same tick, and when it ends the stack falls
// back to the group's default re-form task (0x115C0644: REFORM, HALT, ADOPT_STANCES, IDLE, READY)
// or later to intercept tasks (0x115C0A48). The rout task is gone, the byte still says "set up",
// so nothing pushes it again: the unit stays routing for the morale system but stands, re-forms
// into its yari wall stance and keeps killing what touches it (replay zombie_yariwall: two units,
// 34 and 40 kills after they broke).
//
// The fix (on from launch; ZOMBIEFIX_OFF=1 starts off, I toggles; a build with the CMake option
// ZOMBIEFIX_START_OFF starts off unless ZOMBIEFIX_ON=1): an entry hook on the rout tick
// clears byte +0x2781 of every live group of the unit that is marked set up but has no rout task
// on its stack, so the game's own tick sets the group up again in the same call (its own rout
// task, its own move). Nothing else is changed; a unit that rallies leaves the rout state first.
//
// The research probe (ZOMBIEFIX_PROBE=1) writes <game>\zombiefix_runs\<stamp>\:
//   zombie_states.csv  state machine transitions (FUN_11374cf0+0x1B), with the machine's owner
//                      (squad task, soldier, group, unit) and its unit; every transition of a
//                      unit that is routing, else the first 200 per machine and then 1 in 20
//   zombie_morale.csv  every change of a unit's morale level / rout state / rally byte (50 ms
//                      sampler), with the morale block as hex
//   zombie_groups.csv  every change of a unit's group lists, main group, order queue top, and
//                      of each group's behaviour stack
//   zombie_pos.csv     every unit's groups twice per game second: men alive, centre, the unit's kills
//   zombie_men.csv     every 4th man of every group once per game second: position
//   zombie_raw.csv     hex dumps of the unit (0x2010), its groups (0x1000) and two men (0x600)
//                      of each group every 5 game seconds and at each morale change
//   zombie_kills.csv   every pike / knockdown kill (outcome 5) and every pike / knockdown call
//                      whose attacker's unit is routing
//   zombie_tasks.csv   every re-form / intercept / melee attack task built for a routing unit's
//                      group, with the constructor's caller
//   zombie_fix.csv     every group the fix re-armed (or, fix off, found without its rout task)
//   zombie_setup.csv   every group rout set-up (FUN_101a9bf0, stack before and after) and every
//                      behaviour stack pop of a routing unit's group (FUN_1019e480, lock byte, caller),
//                      every group stop of a routing unit (FUN_101604a0, and FUN_101605f0 for all its groups)
//                      and attack command post (FUN_103daa80), with the game's yari wall test

#include "log.h"
#include "hook.h"
#include "gamemem.h"
#include "keys.h"
#include "stack_offsets_zombie.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint32_t g_base = 0;

// ---------------------------------------------------------------------------
// Hooks (stolen bytes: docs/re/prologues_fbrfix_mid.txt for the transition site,
// docs/re/prologues_zombiefix.txt for the kill functions, prologues_zombiefix_tasks.txt for the task constructors,
// docs/re/prologues_zombiefix_fix.txt for the rout tick, prologues_zombiefix_rout.txt for the rout
// set-up, the stack pop and the yari wall attack task, prologues_zombiefix_stop.txt for the group stop
// and the attack command post; whole instructions, no branch into the range)
// ---------------------------------------------------------------------------

enum {
    H_TRANSITION = 0,  // FUN_11374cf0+0x1B: after the old state's exit, before the new is stored (ESI = ctx, EDI = next)
    H_PIKE,            // FUN_10435410 entry: pike contact outcome (ECX = resolver, result*, attacker, defender); wrapped
    H_KNOCK,           // FUN_10430cf0 entry: knockdown outcome (ECX = resolver, attacker, defender, outcome, flag); wrapped
    H_TASK_REFORM,     // FUN_101283a0 entry: re-form / idle squad task constructor (ECX = task, group, owner, params)
    H_TASK_INTERCEPT,  // FUN_101283f0 entry: direct intercept squad task constructor
    H_TASK_ATTACK,     // FUN_101273f0 entry: melee attack squad task constructor
    H_ROUT_TICK,       // FUN_101bd3b0 entry: tick of the rout interrupt state (ECX = interrupt context); the fix
    H_SETUP,           // FUN_101a9bf0 entry: a group's rout set-up (ECX = group); wrapped (probe)
    H_CLEAR,           // FUN_1019e480 entry: behaviour stack pop down to arg 1 entries (ECX = owner) (probe)
    H_TASK_YARI,       // FUN_101ceb40 entry: the yari wall attack task 0x115C82B4 constructor (probe)
    H_STOP,            // FUN_101604a0 entry: group stop: current action cancelled, stack cleared (ECX = group) (probe)
    H_POST,            // FUN_103daa80 entry: group attack command posted into its action slot (ECX = group) (probe)
    H_STOPALL,         // FUN_101605f0 entry: every group of a unit stopped (ECX = unit +0xB80) (probe)
    HOOK_COUNT
};

static const HookDef g_hookDefs[HOOK_COUNT] = {
    { "FUN_11374cf0+0x1B", "state transition", 0x1374D0B, 5,
      { 0x89, 0x7E, 0x04, 0x8B, 0xCF }, -1, true },                        // MOV [ESI+4],EDI; MOV ECX,EDI
    { "FUN_10435410", "pike kill outcome", 0x435410, 6,
      { 0x64, 0xA1, 0x2C, 0x00, 0x00, 0x00 }, -1, true },                  // MOV EAX,FS:[0x2c]
    { "FUN_10430cf0", "knockdown kill outcome", 0x430CF0, 6,
      { 0x81, 0xEC, 0x98, 0x00, 0x00, 0x00 }, -1, true },                  // SUB ESP,0x98
    { "FUN_101283a0", "re-form task constructor", 0x1283A0, 6,
      { 0x56, 0x57, 0xFF, 0x74, 0x24, 0x10 }, -1, true },                  // PUSH ESI; PUSH EDI; PUSH [ESP+0x10]
    { "FUN_101283f0", "direct intercept task constructor", 0x1283F0, 6,
      { 0x56, 0x57, 0xFF, 0x74, 0x24, 0x10 }, -1, true },
    { "FUN_101273f0", "melee attack task constructor", 0x1273F0, 6,
      { 0x56, 0x57, 0xFF, 0x74, 0x24, 0x10 }, -1, true },
    { "FUN_101bd3b0", "rout interrupt state tick (the fix)", 0x1BD3B0, 7,
      { 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0x47, 0x10 }, -1, false },           // PUSH ESI; PUSH EDI; MOV EDI,ECX; MOV EAX,[EDI+0x10]
    // docs/re/prologues_zombiefix_rout.txt
    { "FUN_101a9bf0", "group rout set-up", 0x1A9BF0, 5,
      { 0x51, 0x56, 0x8B, 0xF1, 0x57 }, -1, true },                        // PUSH ECX; PUSH ESI; MOV ESI,ECX; PUSH EDI
    { "FUN_1019e480", "behaviour stack pop", 0x19E480, 10,
      { 0x56, 0x8B, 0xF1, 0x80, 0xBE, 0x50, 0x04, 0x00, 0x00, 0x00 }, -1, true },   // PUSH ESI; MOV ESI,ECX; CMP byte [ESI+0x450],0
    { "FUN_101ceb40", "yari wall attack task constructor", 0x1CEB40, 6,
      { 0x56, 0x57, 0xFF, 0x74, 0x24, 0x10 }, -1, true },                  // PUSH ESI; PUSH EDI; PUSH [ESP+0x10]
    // docs/re/prologues_zombiefix_stop.txt
    { "FUN_101604a0", "group stop", 0x1604A0, 10,
      { 0x56, 0x8B, 0xF1, 0x80, 0xBE, 0x08, 0x05, 0x00, 0x00, 0x01 }, -1, true },   // PUSH ESI; MOV ESI,ECX; CMP byte [ESI+0x508],1
    { "FUN_103daa80", "group attack command post", 0x3DAA80, 6,
      { 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x04 }, -1, true },                  // PUSH ESI; MOV ESI,ECX; MOV EAX,[ESI+4]
    // docs/re/prologues_zombiefix_stop2.txt
    { "FUN_101605f0", "stop every group of a unit", 0x1605F0, 7,
      { 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0x77, 0x10 }, -1, true },           // PUSH ESI; PUSH EDI; MOV EDI,ECX; MOV ESI,[EDI+0x10]
};

static HookSlot g_slots[HOOK_COUNT] = {};
extern "C" uint8_t* g_zombieTrampolines[HOOK_COUNT] = {};

// ---------------------------------------------------------------------------
// Engine layout (see the header)
// ---------------------------------------------------------------------------

static const uint32_t CTX_OFF_STATE = 0x04;                                  // machine context: current state
static const uint32_t TASK_CTX_OFF = 0x18, TASK_OFF_UNIT = 0x04;             // squad task: context, group
static const uint32_t GRP_OFF_COUNT = 0x18, GRP_OFF_MEN = 0x1C;              // group: men
static const uint32_t GRP_OFF_OWNER = 0x20;                                  // group: behaviour stack owner
static const uint32_t OWN_OFF_STACK = 0x400, OWN_OFF_COUNT = 0x440, OWN_OFF_CUR = 0x454;
static const uint32_t OWN_OFF_UNLOCKED = 0x450;                              // stack: 1 = no pop running (FUN_1019e480)
static const uint32_t SOL_OFF_GROUP = 0x3B4, SOL_OFF_UNIT = 0x214, SOL_OFF_POS = 0x48;
static const uint32_t U_OFF_MORALE = 0xCC4, U_OFF_LEVEL = 0xCE8, U_OFF_ROUT = 0xCEC, U_OFF_RALLY = 0xD10;
static const uint32_t U_OFF_GA_N = 0xB8C, U_OFF_GA = 0xB90, U_OFF_GB_N = 0xB70, U_OFF_GB = 0xB74, U_OFF_MAIN = 0xB9C;
static const uint32_t U_OFF_KILLS = 0xD88;                                   // unit: kill count
static const uint32_t U_OFF_Q = 0x454, U_Q_STRIDE = 0x90, U_OFF_Q_N = 0x9EC, U_OFF_Q_CUR = 0x9F0;
static const uint32_t MAX_MEN = 400;
static const uint32_t ICTX_OFF_UNIT = 0x10;                                  // interrupt context (unit +0xE78): its unit
static const uint32_t GRP_OFF_ROUTED = 0x2781;                               // group: rout set up (FUN_101a9bf0)
static const uint32_t RVA_ROUT_TASK_VT = 0x15E01F4, RVA_ROUT_TASK_VT2 = 0x15EDFA8;   // the rout task (FUN_103be670 / FUN_10529820)
static const uint32_t RAW_U = 0x2010, RAW_G = 0x1000, RAW_S = 0x600;   // units lie 0x2010 apart

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

// ---------------------------------------------------------------------------
// Fix state (the fix proper is not written yet)
// ---------------------------------------------------------------------------

static volatile LONG g_fixOn = 0;
static volatile LONG g_fRearmed = 0, g_fMissing = 0, g_fTicks = 0;
static bool g_keyWatched = false;
static volatile LONG g_testKeys = 0;
static uint8_t* g_toneOn = nullptr;
static uint8_t* g_toneOff = nullptr;

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

// The name of a state object ("-" for none, "?vtRVA" when it has no readable name).
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

// "vtRVA:STATE" of a squad task (its machine context at +0x18, the state at context +4).
static void DescTask(uint32_t task, char* out, int cap) {
    if (!PtrPlausible(task)) { snprintf(out, cap, "-"); return; }
    char st[64];
    NameOf(Rd(task + TASK_CTX_OFF + CTX_OFF_STATE), st, sizeof(st));
    snprintf(out, cap, "%X:%s", Rva(Rd(task)), st);
}

// The behaviour stack of a group: "cur/count|task|task|..."; false when it does not look like one.
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
// Probe files
// ---------------------------------------------------------------------------

static bool g_probe = false;
static CRITICAL_SECTION g_cs;      // files
static char g_dir[MAX_PATH] = "";
static FILE* g_fStates = nullptr;
static FILE* g_fMorale = nullptr;
static FILE* g_fGroups = nullptr;
static FILE* g_fPos = nullptr;
static FILE* g_fMen = nullptr;
static FILE* g_fRaw = nullptr;
static FILE* g_fKills = nullptr;
static FILE* g_fTasks = nullptr;
static FILE* g_fFix = nullptr;
static FILE* g_fSetup = nullptr;
static LARGE_INTEGER g_qpf = {};
static volatile LONG g_rows = 0, g_rawRows = 0;
static const LONG MAX_ROWS = 6000000, MAX_RAW_ROWS = 60000;
static volatile LONG g_cTrans = 0, g_cTransLogged = 0, g_cMorale = 0, g_cGroups = 0, g_cPos = 0, g_cMen = 0, g_cRaw = 0;
static volatile LONG g_cPike = 0, g_cPikeKill = 0, g_cPikeHigh = 0, g_cKnock = 0, g_cKnockKill = 0, g_cKillRows = 0;
static volatile LONG g_cRoutKills = 0, g_cTaskRows = 0, g_cFixRows = 0, g_cSetupRows = 0;
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
    snprintf(root, sizeof(root), "%szombiefix_runs\\", exe);
    CreateDirectoryA(root, nullptr);
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_dir, sizeof(g_dir), "%s%04d%02d%02d_%02d%02d%02d\\", root, st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute, st.wSecond);
    if (!CreateDirectoryA(g_dir, nullptr)) { Log("zombiefix: cannot create %s", g_dir); return false; }
    g_fStates = OpenCsv("zombie_states.csv", "ms,tick,thread,ctx,kind,owner,off,unit,group,rout,old,old_state,new,new_state\n");
    g_fMorale = OpenCsv("zombie_morale.csv", "ms,tick,unit,level,rout,rally,main,men,x,z,hex\n");
    g_fGroups = OpenCsv("zombie_groups.csv", "ms,tick,unit,what,group,text\n");
    g_fPos = OpenCsv("zombie_pos.csv", "tick,unit,group,main,men,alive,x,z,level,rout,rally,kills\n");
    g_fMen = OpenCsv("zombie_men.csv", "tick,unit,group,i,soldier,x,y,z\n");
    g_fRaw = OpenCsv("zombie_raw.csv", "tick,reason,kind,unit,addr,hex\n");
    g_fKills = OpenCsv("zombie_kills.csv",
                       "ms,tick,kind,ret_rva,attacker,att_unit,att_group,att_level,att_rout,defender,def_unit,def_group,"
                       "def_level,def_rout,outcome_in,outcome,who,ax,az,dx,dz\n");
    g_fTasks = OpenCsv("zombie_tasks.csv", "ms,tick,kind,ret_rva,task,group,unit,level,rout,routed,stack\n");
    g_fFix = OpenCsv("zombie_fix.csv", "ms,tick,action,unit,group,men,level,rout,stack\n");
    g_fSetup = OpenCsv("zombie_setup.csv", "ms,tick,thread,event,ret_rva,unit,group,unlocked,count,keep,yari,stack\n");
    if (!g_fStates || !g_fMorale || !g_fGroups || !g_fPos || !g_fMen || !g_fRaw || !g_fKills || !g_fTasks || !g_fFix ||
        !g_fSetup) {
        Log("zombiefix: cannot open the probe files in %s", g_dir);
        return false;
    }
    Log("zombiefix: probe on, writing %s", g_dir);
    return true;
}

static void FlushAll() {
    FILE* fs[] = { g_fStates, g_fMorale, g_fGroups, g_fPos, g_fMen, g_fRaw, g_fKills, g_fTasks, g_fFix, g_fSetup };
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
// Unit registry: every battle unit met through a squad task or a kill
// ---------------------------------------------------------------------------

static const int MAX_GROUPS = 8;
struct GroupRec { uint32_t g; char stack[384]; };
struct UnitRec {
    uint32_t u;
    uint32_t level, rout, rally;      // last seen morale fields
    bool seen;
    char lists[256];                  // last group list text
    char queue[64];                   // last order queue text
    GroupRec groups[MAX_GROUPS];
    int nGroups;
    uint32_t lastRaw, lastPos, lastMen;
};
static const int MAX_UNITS = 96;
static UnitRec g_units[MAX_UNITS];
static volatile LONG g_nUnits = 0;
static CRITICAL_SECTION g_unitCs;

static UnitRec* FindUnitLocked(uint32_t u) {
    for (int i = 0; i < g_nUnits; i++) if (g_units[i].u == u) return &g_units[i];
    return nullptr;
}

// Plausible battle unit: its main group or one of its listed groups is a group.
static bool IsBattleUnit(uint32_t u) {
    if (!PtrPlausible(u)) return false;
    if (IsGroup(Rd(u + U_OFF_MAIN))) return true;
    uint32_t n = Rd(u + U_OFF_GA_N), a = Rd(u + U_OFF_GA);
    return n > 0 && n <= 16 && PtrPlausible(a) && IsGroup(Rd(a));
}

static void AddGroupLocked(UnitRec* r, uint32_t g) {
    if (!g) return;
    for (int i = 0; i < r->nGroups; i++) if (r->groups[i].g == g) return;
    if (r->nGroups < MAX_GROUPS) { r->groups[r->nGroups].g = g; r->groups[r->nGroups].stack[0] = 0; r->nGroups++; }
}

// Groups already registered: lock-free lookup, so that the game thread does not wait for the
// sampler on every squad task transition.
static const int KNOWN_TABLE = 4096;
static volatile LONG g_known[KNOWN_TABLE];

static bool KnownGroup(uint32_t g, bool add) {
    uint32_t h = ((g >> 3) * 2654435761u) & (KNOWN_TABLE - 1);
    for (int i = 0; i < 64; i++) {
        volatile LONG* slot = &g_known[(h + i) & (KNOWN_TABLE - 1)];
        LONG v = *slot;
        if ((uint32_t)v == g) return true;
        if (v == 0) {
            if (!add) return false;
            LONG prev = InterlockedCompareExchange(slot, (LONG)g, 0);
            if (prev == 0 || (uint32_t)prev == g) return false;
        }
    }
    return false;
}

// Registers the unit of a group (through its first man) or of a soldier.
static void NoteGroup(uint32_t g) {
    if (!g_probe || !g || KnownGroup(g, false) || !IsGroup(g)) return;
    uint32_t s0 = Rd(Rd(g + GRP_OFF_MEN));
    uint32_t u = Rd(s0 + SOL_OFF_UNIT);
    if (!IsBattleUnit(u)) return;
    EnterCriticalSection(&g_unitCs);
    UnitRec* r = FindUnitLocked(u);
    if (!r && g_nUnits < MAX_UNITS) {
        r = &g_units[g_nUnits];
        memset(r, 0, sizeof(*r));
        r->u = u;
        InterlockedIncrement(&g_nUnits);
    }
    if (r) AddGroupLocked(r, g);
    LeaveCriticalSection(&g_unitCs);
    if (r) KnownGroup(g, true);
}

static uint32_t RoutOf(uint32_t u) { return PtrPlausible(u) ? Rd(u + U_OFF_ROUT) : 0xFFFFFFFFu; }

// ---------------------------------------------------------------------------
// Machine attribution: squad task, else the soldier / group / unit object it lies in
// ---------------------------------------------------------------------------

struct CtxRec { uint32_t ctx; char kind; uint32_t owner, off, unit, group; uint32_t n; };
static const int CTX_TABLE = 1 << 16;
static CtxRec* g_ctx = nullptr;
static CRITICAL_SECTION g_ctxCs;

// Under g_unitCs: which known object holds ctx.
static void Attribute(uint32_t ctx, CtxRec* c) {
    c->kind = '?';
    for (int i = 0; i < g_nUnits; i++) {
        UnitRec& r = g_units[i];
        if (ctx >= r.u && ctx < r.u + 0x2000) { c->kind = 'U'; c->owner = r.u; c->off = ctx - r.u; c->unit = r.u; return; }
        for (int k = 0; k < r.nGroups; k++) {
            uint32_t g = r.groups[k].g;
            if (ctx >= g && ctx < g + 0x2800) {
                c->kind = 'G'; c->owner = g; c->off = ctx - g; c->unit = r.u; c->group = g; return;
            }
            uint32_t n = Rd(g + GRP_OFF_COUNT), men = Rd(g + GRP_OFF_MEN);
            if (n > MAX_MEN || !PtrPlausible(men)) continue;
            for (uint32_t j = 0; j < n; j++) {
                uint32_t s = Rd(men + 4 * j);
                if (s && ctx >= s && ctx < s + 0x800) {
                    c->kind = 'S'; c->owner = s; c->off = ctx - s; c->unit = r.u; c->group = g; return;
                }
            }
        }
    }
}

static CtxRec* LookupCtx(uint32_t ctx) {
    uint32_t h = ((ctx >> 2) * 2654435761u) & (CTX_TABLE - 1);
    for (int i = 0; i < 64; i++) {
        CtxRec& c = g_ctx[(h + i) & (CTX_TABLE - 1)];
        if (c.ctx == ctx) return &c;
        if (c.ctx == 0) {
            memset(&c, 0, sizeof(c));
            c.ctx = ctx;
            EnterCriticalSection(&g_unitCs);
            Attribute(ctx, &c);
            LeaveCriticalSection(&g_unitCs);
            return &c;
        }
    }
    return nullptr;
}

// FUN_11374cf0+0x1B: ESI = machine context, EDI = the next state; [ctx+4] still the old one.
static void HandleTransition(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cTrans);
    uint32_t mctx = R32(ctx + ZOMBIE_OFF_ESI), next = R32(ctx + ZOMBIE_OFF_EDI);
    uint32_t old = Rd(mctx + CTX_OFF_STATE);
    // squad task: context at task+0x18, group at task+4
    uint32_t task = mctx - TASK_CTX_OFF, grp = Rd(task + TASK_OFF_UNIT);
    char kind;
    uint32_t owner, off = 0, unit = 0, group = 0, n;
    if (IsGroup(grp)) {
        NoteGroup(grp);
        kind = 'T';
        owner = Rva(Rd(task));
        group = grp;
        unit = Rd(Rd(Rd(grp + GRP_OFF_MEN)) + SOL_OFF_UNIT);
        EnterCriticalSection(&g_ctxCs);
        CtxRec* c = LookupCtx(mctx);
        if (c && c->kind == '?') { c->kind = 'T'; c->unit = unit; c->group = grp; }
        n = c ? ++c->n : 0;
        LeaveCriticalSection(&g_ctxCs);
    } else {
        EnterCriticalSection(&g_ctxCs);
        CtxRec* c = LookupCtx(mctx);
        if (c && c->kind == '?' && (c->n & 63) == 63) {   // objects met later: try again now and then
            EnterCriticalSection(&g_unitCs);
            Attribute(mctx, c);
            LeaveCriticalSection(&g_unitCs);
        }
        kind = c ? c->kind : '?';
        owner = c ? c->owner : 0;
        off = c ? c->off : 0;
        unit = c ? c->unit : 0;
        group = c ? c->group : 0;
        n = c ? ++c->n : 0;
        LeaveCriticalSection(&g_ctxCs);
    }
    uint32_t rout = unit ? RoutOf(unit) : 0xFFFFFFFFu;
    bool routing = rout != 0 && rout != 0xFFFFFFFFu;
    if (!routing && n > 200 && n % 20 != 0) return;
    if (!RowBudget()) return;
    char so[64], sn[64];
    NameOf(old, so, sizeof(so));
    NameOf(next, sn, sizeof(sn));
    InterlockedIncrement(&g_cTransLogged);
    EnterCriticalSection(&g_cs);
    if (g_fStates)
        fprintf(g_fStates, "%u,%u,%lu,%X,%c,%X,%X,%X,%X,%d,%X,%s,%X,%s\n", Ms(), Tick(), GetCurrentThreadId(), mctx, kind,
                owner, off, unit, group, (int)rout, Rva(Rd(old)), so, Rva(Rd(next)), sn);
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// Kills
// ---------------------------------------------------------------------------

static void KillRow(const char* kind, uint32_t ret, uint32_t att, uint32_t def, uint32_t in, uint32_t out, uint32_t who) {
    uint32_t au = Rd(att + SOL_OFF_UNIT), du = Rd(def + SOL_OFF_UNIT);
    uint32_t ag = Rd(att + SOL_OFF_GROUP), dg = Rd(def + SOL_OFF_GROUP);
    uint32_t ar = RoutOf(au), dr = RoutOf(du);
    bool attRouting = ar != 0 && ar != 0xFFFFFFFFu;
    if (out == 5 && attRouting) InterlockedIncrement(&g_cRoutKills);
    if (out != 5 && !attRouting) return;
    NoteGroup(ag);
    NoteGroup(dg);
    if (!RowBudget()) return;
    InterlockedIncrement(&g_cKillRows);
    EnterCriticalSection(&g_cs);
    if (g_fKills)
        fprintf(g_fKills, "%u,%u,%s,%X,%X,%X,%X,%d,%d,%X,%X,%X,%d,%d,%u,%u,%X,%.2f,%.2f,%.2f,%.2f\n", Ms(), Tick(), kind,
                Rva(ret), att, au, ag, (int)Rd(au + U_OFF_LEVEL), (int)ar, def, du, dg, (int)Rd(du + U_OFF_LEVEL),
                (int)dr, in, out, who, RdF(att + SOL_OFF_POS), RdF(att + SOL_OFF_POS + 8), RdF(def + SOL_OFF_POS),
                RdF(def + SOL_OFF_POS + 8));
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

// FUN_10435410 wrapped (ECX = resolver; arg 1 = result {outcome, who}, arg 2 = attacker, arg 3 =
// defender; RET 0xC; returns arg 1). HOOK_RETURN keeps 31 bits: a result buffer above 2 GB is
// left to the game unwrapped (counted).
typedef uint32_t (__fastcall* PikeFn)(uint32_t self, uint32_t edx, uint32_t res, uint32_t att, uint32_t def);

static uint32_t HandlePike(const uint8_t* ctx) {
    if (!g_probe) return HOOK_CONTINUE;
    uint32_t self = R32(ctx + ZOMBIE_OFF_ECX), res = R32(ctx + ZOMBIE_OFF_ARG1);
    uint32_t att = R32(ctx + ZOMBIE_OFF_ARG2), def = R32(ctx + ZOMBIE_OFF_ARG3);
    if (res & 0x80000000u) { InterlockedIncrement(&g_cPikeHigh); return HOOK_CONTINUE; }
    InterlockedIncrement(&g_cPike);
    uint32_t in = Rd(res);
    uint32_t r = ((PikeFn)g_zombieTrampolines[H_PIKE])(self, 0, res, att, def);
    uint32_t out = Rd(res), who = Rd(res + 4);
    if (out == 5) InterlockedIncrement(&g_cPikeKill);
    KillRow("pike", R32(ctx + ZOMBIE_OFF_RET), att, def, in, out, who);
    return HOOK_RETURN(r);
}

// FUN_10430cf0 wrapped (ECX = resolver; attacker, defender, outcome, flag byte; RET 0x10; returns
// the outcome, 5 = kill).
typedef uint32_t (__fastcall* KnockFn)(uint32_t self, uint32_t edx, uint32_t att, uint32_t def, uint32_t in, uint32_t flag);

static uint32_t HandleKnock(const uint8_t* ctx) {
    if (!g_probe) return HOOK_CONTINUE;
    uint32_t self = R32(ctx + ZOMBIE_OFF_ECX), att = R32(ctx + ZOMBIE_OFF_ARG1), def = R32(ctx + ZOMBIE_OFF_ARG2);
    uint32_t in = R32(ctx + ZOMBIE_OFF_ARG3), flag = R32(ctx + ZOMBIE_OFF_ARG4);
    InterlockedIncrement(&g_cKnock);
    uint32_t out = ((KnockFn)g_zombieTrampolines[H_KNOCK])(self, 0, att, def, in, flag);
    if (out == 5) InterlockedIncrement(&g_cKnockKill);
    if (att && def) KillRow("knock", R32(ctx + ZOMBIE_OFF_RET), att, def, in, out, flag & 0xFF);
    return HOOK_RETURN(out);
}

// ---------------------------------------------------------------------------
// Squad tasks pushed onto a routing unit's group (probe)
// ---------------------------------------------------------------------------

static uint32_t UnitOfGroup(uint32_t g) { return IsGroup(g) ? Rd(Rd(Rd(g + GRP_OFF_MEN)) + SOL_OFF_UNIT) : 0; }

// FUN_101283a0 / 101283f0 / 101273f0 (ECX = task, arg 1 = group, arg 2 = owner, arg 3 = parameters)
static void HandleTaskCtor(const uint8_t* ctx, const char* kind) {
    if (!g_probe) return;
    uint32_t g = R32(ctx + ZOMBIE_OFF_ARG1), u = UnitOfGroup(g);
    uint32_t rout = RoutOf(u);
    if (rout == 0 || rout == 0xFFFFFFFFu || !RowBudget()) return;
    char stack[384];
    DescStack(g, stack, sizeof(stack));
    InterlockedIncrement(&g_cTaskRows);
    EnterCriticalSection(&g_cs);
    if (g_fTasks)
        fprintf(g_fTasks, "%u,%u,%s,%X,%X,%X,%X,%u,%u,%u,%s\n", Ms(), Tick(), kind, Rva(R32(ctx + ZOMBIE_OFF_RET)),
                R32(ctx + ZOMBIE_OFF_ECX), g, u, Rd(u + U_OFF_LEVEL), rout, RdB(g + GRP_OFF_ROUTED), stack);
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// The rout set-up and the stack pops of a routing unit's groups (probe): who empties the stack
// ---------------------------------------------------------------------------

// Yari wall active for the group's unit: the game's own test (the unit's ability set at +0xED4 of
// [group +0x510], vtable slot +4, ability 6), as FUN_103d0470 asks it. Game thread only; -1 when
// the objects do not look right (harness).
typedef uint32_t (__fastcall* AbilityFn)(uint32_t self, uint32_t edx, uint32_t id);

static int YariOn(uint32_t g) {
    uint32_t owner = Rd(g + 0x510);
    if (!PtrPlausible(owner)) return -1;
    uint32_t set = owner + 0xED4, fn = Rd(Rd(set) + 4);
    if (!Rva(fn)) return -1;
    return (int)(((AbilityFn)fn)(set, 0, 6) & 0xFF);
}

static void SetupRow(const char* ev, uint32_t ret, uint32_t u, uint32_t g, uint32_t keep, int yari = -2) {
    if (!RowBudget()) return;
    char stack[384];
    DescStack(g, stack, sizeof(stack));
    uint32_t owner = g + GRP_OFF_OWNER;
    InterlockedIncrement(&g_cSetupRows);
    EnterCriticalSection(&g_cs);
    if (g_fSetup)
        fprintf(g_fSetup, "%u,%u,%lu,%s,%X,%X,%X,%u,%u,%u,%d,%s\n", Ms(), Tick(), GetCurrentThreadId(), ev, Rva(ret), u, g,
                RdB(owner + OWN_OFF_UNLOCKED), Rd(owner + OWN_OFF_COUNT), keep, yari, stack);
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

// FUN_101a9bf0 wrapped (ECX = group, no stack arguments, plain RET): the stack before and after.
typedef void (__fastcall* SetupFn)(uint32_t self, uint32_t edx);

static uint32_t HandleSetup(const uint8_t* ctx) {
    if (!g_probe) return HOOK_CONTINUE;
    uint32_t g = R32(ctx + ZOMBIE_OFF_ECX), ret = R32(ctx + ZOMBIE_OFF_RET), u = UnitOfGroup(g);
    SetupRow("setup_in", ret, u, g, 0, YariOn(g));
    ((SetupFn)g_zombieTrampolines[H_SETUP])(g, 0);
    SetupRow("setup_out", ret, u, g, 0, YariOn(g));
    return HOOK_RETURN(0);
}

// FUN_1019e480 (ECX = stack owner = group +0x20, arg 1 = entries to keep): only for routing units.
static void HandleClear(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t g = R32(ctx + ZOMBIE_OFF_ECX) - GRP_OFF_OWNER;
    if (!IsGroup(g)) return;
    uint32_t u = UnitOfGroup(g), rout = RoutOf(u);
    if (rout == 0 || rout == 0xFFFFFFFFu) return;
    SetupRow("pop", R32(ctx + ZOMBIE_OFF_RET), u, g, R32(ctx + ZOMBIE_OFF_ARG1));
}

// FUN_101604a0 (ECX = group): only for routing units. FUN_103daa80 (ECX = group): every post,
// "post" for a routing unit, "post_ok" otherwise.
static void HandleStop(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t g = R32(ctx + ZOMBIE_OFF_ECX);
    if (!IsGroup(g)) return;
    uint32_t u = UnitOfGroup(g), rout = RoutOf(u);
    if (rout == 0 || rout == 0xFFFFFFFFu) return;
    SetupRow("stop", R32(ctx + ZOMBIE_OFF_RET), u, g, RdB(g + 0x508));
}

static void HandlePost(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t g = R32(ctx + ZOMBIE_OFF_ECX);
    if (!IsGroup(g)) return;
    uint32_t u = UnitOfGroup(g), rout = RoutOf(u);
    SetupRow(rout != 0 && rout != 0xFFFFFFFFu ? "post" : "post_ok", R32(ctx + ZOMBIE_OFF_RET), u, g, RdB(g + 0x508),
             YariOn(g));
}

// FUN_101605f0 (ECX = unit +0xB80, its group list): only for routing units; one row per group.
static void HandleStopAll(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t u = R32(ctx + ZOMBIE_OFF_ECX) - (U_OFF_GA - 0x10);
    if (!IsBattleUnit(u)) return;
    uint32_t rout = RoutOf(u);
    if (rout == 0 || rout == 0xFFFFFFFFu) return;
    uint32_t n = Rd(u + U_OFF_GA_N), arr = Rd(u + U_OFF_GA);
    for (uint32_t i = 0; i < n && i < 16 && PtrPlausible(arr); i++) {
        uint32_t g = Rd(arr + 4 * i);
        if (IsGroup(g)) SetupRow("stopall", R32(ctx + ZOMBIE_OFF_RET), u, g, RdB(g + 0x508), YariOn(g));
    }
}

// ---------------------------------------------------------------------------
// The fix: the rout interrupt state's tick (FUN_101bd3b0) sets up every group of the unit whose
// byte +0x2781 is still 0 (FUN_101a9bf0: byte = 1, the group's behaviour stack cleared, the rout
// task pushed) and never looks at the group again. A task pushed onto the group afterwards (an
// order of the same tick, the group's default re-form task, an intercept) replaces the rout task
// for good: the unit stays "routing" for the morale system but stands, re-forms, adopts its
// stances (yari wall: braced spears) and kills whatever runs into it. The fix clears the byte of
// a live group that is marked set up but has no rout task on its stack, so the game's own tick
// sets it up again in the same call.
// ---------------------------------------------------------------------------

static bool HasRoutTask(uint32_t g) {
    uint32_t owner = g + GRP_OFF_OWNER, n = Rd(owner + OWN_OFF_COUNT);
    if (n > 8) return true;                       // not readable as a stack: leave the group alone
    for (uint32_t i = 0; i < n; i++) {
        uint32_t vt = Rd(Rd(owner + OWN_OFF_STACK + 8 * i));
        if (vt == g_base + RVA_ROUT_TASK_VT || vt == g_base + RVA_ROUT_TASK_VT2) return true;
    }
    return false;
}

static void FixRow(const char* action, uint32_t u, uint32_t g) {
    if (!g_probe || !RowBudget()) return;
    char stack[384];
    DescStack(g, stack, sizeof(stack));
    InterlockedIncrement(&g_cFixRows);
    EnterCriticalSection(&g_cs);
    if (g_fFix)
        fprintf(g_fFix, "%u,%u,%s,%X,%X,%u,%u,%u,%s\n", Ms(), Tick(), action, u, g, Rd(g + GRP_OFF_COUNT),
                Rd(u + U_OFF_LEVEL), Rd(u + U_OFF_ROUT), stack);
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

static void HandleRoutTick(const uint8_t* ctx) {
    InterlockedIncrement(&g_fTicks);
    uint32_t u = Rd(R32(ctx + ZOMBIE_OFF_ECX) + ICTX_OFF_UNIT);
    if (!PtrPlausible(u)) return;
    uint32_t n = Rd(u + U_OFF_GA_N), arr = Rd(u + U_OFF_GA);
    if (n == 0 || n > 16 || !PtrPlausible(arr)) return;
    bool on = InterlockedCompareExchange(&g_fixOn, 0, 0) != 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t g = Rd(arr + 4 * i);
        if (!IsGroup(g) || RdB(g + GRP_OFF_ROUTED) != 1 || HasRoutTask(g)) continue;
        InterlockedIncrement(&g_fMissing);
        if (on && WriteByteTry(g + GRP_OFF_ROUTED, 0)) {
            InterlockedIncrement(&g_fRearmed);
            FixRow("rearm", u, g);
        } else {
            FixRow("missing", u, g);
        }
    }
}

// ---------------------------------------------------------------------------
// Sampler thread: morale changes, group lists, stacks, positions, raw dumps
// ---------------------------------------------------------------------------

static HANDLE g_sampler = nullptr;
static volatile LONG g_stop = 0;

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

static void RawRow(uint32_t tick, const char* reason, char kind, uint32_t u, uint32_t addr, uint32_t len) {
    static uint8_t buf[RAW_U];
    static char hex[2 * RAW_U + 1];
    if (InterlockedIncrement(&g_rawRows) >= MAX_RAW_ROWS) return;
    uint32_t got = CopyGuarded(addr, buf, len);
    if (got < len) memset(buf + got, 0, len - got);
    Hex(buf, len, hex);
    EnterCriticalSection(&g_cs);
    if (g_fRaw) fprintf(g_fRaw, "%u,%s,%c,%X,%X,%s\n", tick, reason, kind, u, addr, hex);
    InterlockedIncrement(&g_cRaw);
    LeaveCriticalSection(&g_cs);
}

// Sampler thread only (RawRow's buffers are static). Under g_unitCs.
static void DumpUnit(UnitRec& r, uint32_t tick, const char* reason) {
    RawRow(tick, reason, 'U', r.u, r.u, RAW_U);
    for (int k = 0; k < r.nGroups; k++) {
        uint32_t g = r.groups[k].g;
        RawRow(tick, reason, 'G', r.u, g, RAW_G);
        uint32_t n = Rd(g + GRP_OFF_COUNT), men = Rd(g + GRP_OFF_MEN);
        if (n == 0 || n > MAX_MEN || !PtrPlausible(men)) continue;
        RawRow(tick, reason, 'S', r.u, Rd(men), RAW_S);
        if (n > 1) RawRow(tick, reason, 'S', r.u, Rd(men + 4 * (n / 2)), RAW_S);
    }
}

static void SampleUnit(UnitRec& r, uint32_t tick) {
    uint32_t u = r.u;
    // groups listed by the unit (they may split off and join)
    char lists[256];
    int k = snprintf(lists, sizeof(lists), "main=%X A", Rd(u + U_OFF_MAIN));
    uint32_t na = Rd(u + U_OFF_GA_N), a = Rd(u + U_OFF_GA), nb = Rd(u + U_OFF_GB_N), b = Rd(u + U_OFF_GB);
    for (uint32_t i = 0; i < na && i < 8 && PtrPlausible(a); i++) {
        uint32_t g = Rd(a + 4 * i);
        k += snprintf(lists + k, sizeof(lists) - k, ":%X/%u", g, Rd(g + GRP_OFF_COUNT));
        if (IsGroup(g)) AddGroupLocked(&r, g);
    }
    k += snprintf(lists + k, sizeof(lists) - k, " B");
    for (uint32_t i = 0; i < nb && i < 8 && PtrPlausible(b) && k < (int)sizeof(lists) - 24; i++) {
        uint32_t g = Rd(b + 4 * i);
        k += snprintf(lists + k, sizeof(lists) - k, ":%X/%u", g, Rd(g + GRP_OFF_COUNT));
        if (IsGroup(g)) AddGroupLocked(&r, g);
    }
    if (IsGroup(Rd(u + U_OFF_MAIN))) AddGroupLocked(&r, Rd(u + U_OFF_MAIN));
    char queue[64];
    uint32_t qn = Rd(u + U_OFF_Q_N), qc = Rd(u + U_OFF_Q_CUR);
    snprintf(queue, sizeof(queue), "%u/%u:%X", qc, qn, qn && qc < 16 ? Rva(Rd(u + U_OFF_Q + qc * U_Q_STRIDE)) : 0);
    DWORD ms = Ms();
    if (strcmp(lists, r.lists) != 0 || strcmp(queue, r.queue) != 0) {
        memcpy(r.lists, lists, sizeof(lists));
        memcpy(r.queue, queue, sizeof(queue));
        if (RowBudget()) {
            EnterCriticalSection(&g_cs);
            if (g_fGroups) fprintf(g_fGroups, "%lu,%u,%X,lists,0,%s queue=%s\n", ms, tick, u, lists, queue);
            InterlockedIncrement(&g_cGroups);
            LeaveCriticalSection(&g_cs);
        }
    }
    for (int i = 0; i < r.nGroups; i++) {
        char s[384];
        if (!DescStack(r.groups[i].g, s, sizeof(s)) || strcmp(s, r.groups[i].stack) == 0) continue;
        memcpy(r.groups[i].stack, s, sizeof(s));
        if (!RowBudget()) continue;
        EnterCriticalSection(&g_cs);
        if (g_fGroups) fprintf(g_fGroups, "%lu,%u,%X,stack,%X,%s\n", ms, tick, u, r.groups[i].g, s);
        InterlockedIncrement(&g_cGroups);
        LeaveCriticalSection(&g_cs);
    }
    // morale
    uint32_t level = Rd(u + U_OFF_LEVEL), rout = Rd(u + U_OFF_ROUT), rally = RdB(u + U_OFF_RALLY);
    bool changed = !r.seen || level != r.level || rout != r.rout || rally != r.rally;
    if (changed) {
        r.seen = true;
        r.level = level; r.rout = rout; r.rally = rally;
        uint8_t mb[0x80] = {};
        CopyGuarded(u + U_OFF_MORALE, mb, sizeof(mb));
        char hex[2 * 0x80 + 1];
        Hex(mb, sizeof(mb), hex);
        uint32_t main = Rd(u + U_OFF_MAIN), alive = 0;
        float x = 0, z = 0;
        GroupCentre(main, &alive, &x, &z);
        if (RowBudget()) {
            EnterCriticalSection(&g_cs);
            if (g_fMorale) fprintf(g_fMorale, "%lu,%u,%X,%u,%u,%u,%X,%u,%.2f,%.2f,%s\n", ms, tick, u, level, rout, rally,
                                   main, alive, x, z, hex);
            InterlockedIncrement(&g_cMorale);
            LeaveCriticalSection(&g_cs);
        }
        DumpUnit(r, tick, "morale");
        r.lastRaw = tick;
    } else if (tick >= r.lastRaw + 50 || tick < r.lastRaw) {
        DumpUnit(r, tick, "periodic");
        r.lastRaw = tick;
    }
    // positions twice per game second, men once per second
    if (tick >= r.lastPos + 5 || tick < r.lastPos) {
        r.lastPos = tick;
        uint32_t main = Rd(u + U_OFF_MAIN);
        EnterCriticalSection(&g_cs);
        for (int i = 0; i < r.nGroups && g_fPos; i++) {
            uint32_t g = r.groups[i].g, alive = 0;
            float x = 0, z = 0;
            GroupCentre(g, &alive, &x, &z);
            if (!RowBudget()) break;
            fprintf(g_fPos, "%u,%X,%X,%d,%u,%u,%.2f,%.2f,%u,%u,%u,%u\n", tick, u, g, g == main ? 1 : 0,
                    Rd(g + GRP_OFF_COUNT), alive, x, z, level, rout, rally, Rd(u + U_OFF_KILLS));
            InterlockedIncrement(&g_cPos);
        }
        LeaveCriticalSection(&g_cs);
    }
    if (tick >= r.lastMen + 10 || tick < r.lastMen) {
        r.lastMen = tick;
        EnterCriticalSection(&g_cs);
        for (int i = 0; i < r.nGroups && g_fMen; i++) {
            uint32_t g = r.groups[i].g, n = Rd(g + GRP_OFF_COUNT), men = Rd(g + GRP_OFF_MEN);
            if (n == 0 || n > MAX_MEN || !PtrPlausible(men)) continue;
            for (uint32_t j = 0; j < n; j += 4) {
                uint32_t s = Rd(men + 4 * j);
                if (!PtrPlausible(s) || !RowBudget()) continue;
                fprintf(g_fMen, "%u,%X,%X,%u,%X,%.2f,%.2f,%.2f\n", tick, u, g, j, s, RdF(s + SOL_OFF_POS),
                        RdF(s + SOL_OFF_POS + 4), RdF(s + SOL_OFF_POS + 8));
                InterlockedIncrement(&g_cMen);
            }
        }
        LeaveCriticalSection(&g_cs);
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
// Key, detours, harness exports
// ---------------------------------------------------------------------------

// I toggles the fix (dev build; focus only through proxy/keys). Harness: ZombieFixTestKeys bit 0.
extern "C" __declspec(dllexport) void __stdcall ZombieFixTestKeys(LONG bits) { InterlockedExchange(&g_testKeys, bits); }

static void ToggleKey() {
    bool on = InterlockedCompareExchange(&g_fixOn, 0, 0) == 0;
    InterlockedExchange(&g_fixOn, on ? 1 : 0);
    Log("zombiefix: I, fix %s", on ? "ON" : "OFF");
    Tone_Play(on ? g_toneOn : g_toneOff);
}

extern "C" void __stdcall ZombieHandler0(const uint8_t* ctx) { HandleTransition(ctx); }
extern "C" uint32_t __stdcall ZombieHandler1(const uint8_t* ctx) { return HandlePike(ctx); }
extern "C" uint32_t __stdcall ZombieHandler2(const uint8_t* ctx) { return HandleKnock(ctx); }
extern "C" void __stdcall ZombieHandler3(const uint8_t* ctx) { HandleTaskCtor(ctx, "reform"); }
extern "C" void __stdcall ZombieHandler4(const uint8_t* ctx) { HandleTaskCtor(ctx, "intercept"); }
extern "C" void __stdcall ZombieHandler5(const uint8_t* ctx) { HandleTaskCtor(ctx, "attack"); }
extern "C" void __stdcall ZombieHandler6(const uint8_t* ctx) { HandleRoutTick(ctx); }
extern "C" uint32_t __stdcall ZombieHandler7(const uint8_t* ctx) { return HandleSetup(ctx); }
extern "C" void __stdcall ZombieHandler8(const uint8_t* ctx) { HandleClear(ctx); }
extern "C" void __stdcall ZombieHandler9(const uint8_t* ctx) { HandleTaskCtor(ctx, "yari_attack"); }
extern "C" void __stdcall ZombieHandler10(const uint8_t* ctx) { HandleStop(ctx); }
extern "C" void __stdcall ZombieHandler11(const uint8_t* ctx) { HandlePost(ctx); }
extern "C" void __stdcall ZombieHandler12(const uint8_t* ctx) { HandleStopAll(ctx); }

HOOK_DETOUR(ZombieDetour0, ZombieHandler0, g_zombieTrampolines, 0)
HOOK_DETOUR_RET(ZombieDetour1, ZombieHandler1, g_zombieTrampolines, 1, 0xC)    // FUN_10435410: __thiscall, 3 stack args
HOOK_DETOUR_RET(ZombieDetour2, ZombieHandler2, g_zombieTrampolines, 2, 0x10)   // FUN_10430cf0: __thiscall, 4 stack args
HOOK_DETOUR(ZombieDetour3, ZombieHandler3, g_zombieTrampolines, 3)
HOOK_DETOUR(ZombieDetour4, ZombieHandler4, g_zombieTrampolines, 4)
HOOK_DETOUR(ZombieDetour5, ZombieHandler5, g_zombieTrampolines, 5)
HOOK_DETOUR(ZombieDetour6, ZombieHandler6, g_zombieTrampolines, 6)
HOOK_DETOUR_RET(ZombieDetour7, ZombieHandler7, g_zombieTrampolines, 7, 0)    // FUN_101a9bf0: __thiscall, no stack args
HOOK_DETOUR(ZombieDetour8, ZombieHandler8, g_zombieTrampolines, 8)
HOOK_DETOUR(ZombieDetour9, ZombieHandler9, g_zombieTrampolines, 9)
HOOK_DETOUR(ZombieDetour10, ZombieHandler10, g_zombieTrampolines, 10)
HOOK_DETOUR(ZombieDetour11, ZombieHandler11, g_zombieTrampolines, 11)
HOOK_DETOUR(ZombieDetour12, ZombieHandler12, g_zombieTrampolines, 12)

static const HookDetour g_detours[HOOK_COUNT] = { ZombieDetour0, ZombieDetour1, ZombieDetour2, ZombieDetour3,
                                                  ZombieDetour4, ZombieDetour5, ZombieDetour6, ZombieDetour7,
                                                  ZombieDetour8, ZombieDetour9, ZombieDetour10, ZombieDetour11,
                                                  ZombieDetour12 };

// Harness only (fixes/zombie-yari-wall-fix/tests): fix and probe counters.
extern "C" __declspec(dllexport) void __stdcall ZombieFixTestCounters(long* out) {
    out[0] = g_probe ? 1 : 0;
    out[1] = g_fixOn;
    out[2] = g_base ? 1 : 0;
    out[3] = g_cTrans;
    out[4] = g_cTransLogged;
    out[5] = g_nUnits;
    out[6] = g_cPike;
    out[7] = g_cPikeKill;
    out[8] = g_cKnock;
    out[9] = g_cKnockKill;
    out[10] = g_cKillRows;
    out[11] = g_cRoutKills;
    out[12] = g_cPikeHigh;
    out[13] = g_cMorale;
    out[14] = g_fRearmed;
    out[15] = g_fMissing;
    out[16] = g_fTicks;
    out[17] = g_cTaskRows;
    out[18] = g_cFixRows;
    out[19] = g_cSetupRows;
}

// ---------------------------------------------------------------------------
// Install / remove
// ---------------------------------------------------------------------------

static bool EnvIs(const char* name, char c) {
    char v[8] = "";
    return GetEnvironmentVariableA(name, v, sizeof(v)) > 0 && v[0] == c;
}

void ZombieFix_Install() {
    Log("zombiefix: zombie yari wall fix (dev build)");
    Mem_Init();
    uint32_t size = 0;
    uint8_t* base = Hook_GameModule(false, &size);
    if (!base) { Log("zombiefix: REFUSED, empire.retail.dll is not loaded"); return; }
    if (size != HOOK_ANALYSED_IMAGE_SIZE) { Log("zombiefix: REFUSED, SizeOfImage 0x%08X is not the analysed build", size); return; }
    for (int h = 0; h < HOOK_COUNT; h++) {
        if (!Hook_SiteMatches(base, g_hookDefs[h])) {
            Log("zombiefix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", g_hookDefs[h].rva,
                g_hookDefs[h].func);
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
    g_toneOn = Tone_Make(on, 1);
    g_toneOff = Tone_Make(off, 1);
#ifdef ZOMBIEFIX_START_OFF
    bool fixOff = !EnvIs("ZOMBIEFIX_ON", '1');      // live-check build: off at launch, I turns it on
    const char* offWhy = "OFF at launch (build option ZOMBIEFIX_START_OFF)";
#else
    bool fixOff = EnvIs("ZOMBIEFIX_OFF", '1');
    const char* offWhy = "OFF (ZOMBIEFIX_OFF=1)";
#endif
    if (EnvIs("ZOMBIEFIX_PROBE", '1')) {
        g_ctx = (CtxRec*)VirtualAlloc(nullptr, sizeof(CtxRec) * CTX_TABLE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (g_ctx && OpenProbe()) g_probe = true;
        else Log("zombiefix: probe not started (no run folder or no memory)");
    }
    bool use[HOOK_COUNT];
    for (int h = 0; h < HOOK_COUNT; h++) use[h] = !g_hookDefs[h].probeOnly || g_probe;
    if (!Hook_Prepare(base, g_hookDefs, HOOK_COUNT, use, g_zombieTrampolines, g_slots)) {
        Log("zombiefix: REFUSED, no trampoline memory");
        g_probe = false;
        g_base = 0;
        return;
    }
    InterlockedExchange(&g_fixOn, fixOff ? 0 : 1);
    if (g_probe) g_sampler = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    Hook_Patch(g_detours, g_slots, HOOK_COUNT);
    for (int h = 0; h < HOOK_COUNT; h++)
        if (g_slots[h].active)
            Log("zombiefix: hook %s (%s) installed at RVA 0x%06X", g_hookDefs[h].func, g_hookDefs[h].role, g_hookDefs[h].rva);
    Log("zombiefix: fix %s: a routing unit's group whose rout task was replaced is set up to rout again",
        fixOff ? offWhy : "ON");
    g_keyWatched = Keys_Watch('I', &g_testKeys, 1, ToggleKey);
    Log("zombiefix: I toggles the fix on / off%s", g_keyWatched ? "" : " (ERROR: key poll not started)");
}

void ZombieFix_Remove() {
    if (g_keyWatched) { Keys_Unwatch(ToggleKey); g_keyWatched = false; }
    Hook_Remove(g_slots, HOOK_COUNT);
    if (!g_base) return;
    Log("zombiefix: rout ticks %ld; groups found without their rout task %ld, set up to rout again %ld", g_fTicks,
        g_fMissing, g_fRearmed);
    if (g_sampler) {
        InterlockedExchange(&g_stop, 1);
        WaitForSingleObject(g_sampler, 2000);
        CloseHandle(g_sampler);
        g_sampler = nullptr;
    }
    if (g_probe) {
        EnterCriticalSection(&g_cs);
        FILE** fs[] = { &g_fStates, &g_fMorale, &g_fGroups, &g_fPos, &g_fMen, &g_fRaw, &g_fKills, &g_fTasks, &g_fFix,
                        &g_fSetup };
        for (FILE** f : fs) if (*f) { fclose(*f); *f = nullptr; }
        g_probe = false;
        LeaveCriticalSection(&g_cs);
        Log("zombiefix: probe closed: transitions %ld (logged %ld), units %ld, morale rows %ld, group rows %ld, raw rows %ld, "
            "rows %ld", g_cTrans, g_cTransLogged, g_nUnits, g_cMorale, g_cGroups, g_cRaw, g_rows);
        Log("zombiefix: pike calls %ld (kills %ld, result above 2 GB not wrapped %ld), knockdown calls %ld (kills %ld), "
            "kills by routing units %ld; tasks pushed onto routing groups %ld", g_cPike, g_cPikeKill, g_cPikeHigh, g_cKnock,
            g_cKnockKill, g_cRoutKills, g_cTaskRows);
    }
}
