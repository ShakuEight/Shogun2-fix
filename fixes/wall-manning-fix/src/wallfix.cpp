// wallfix.cpp -- wall manning fix for Total War: Shogun 2 (original x86 Empire.Retail.dll).
// Folder fixes/wall-manning-fix/, code name wallfix.
//
// Dev build. Fault: in a siege defence, a unit ordered to man a wall section often marches to an
// unrelated point C on open ground first, stops, and only then walks onto the wall at B, instead
// of going from A to B at once.
//
// Engine decode (fixes/wall-manning-fix/ARCHITECTURE.md): the order runs a squad task (vtable
// 0x115BF91C, constructor FUN_1010d810) with three steps. Its first state's enter (FUN_101b0b60)
// takes the bounding box of the wall positions the unit will hold and asks FUN_1016af90 whether
// the unit stands outside a circle around it (half the box diagonal plus the unit's arrival
// radius). If so it computes a staging point C with FUN_10175f10: for a building, on the line from
// the box centre towards a reference point of the building (inside the castle in the measured
// replay), half a diagonal plus half a formation depth out; else the first spot towards the unit
// where the whole formation rectangle fits. The second state marches the unit there in formation
// (an ordinary unit move, FUN_103db410 from FUN_1017e5a0) and waits until it stands in its
// rectangle; only the third state (FUN_101b2010) hands every man his wall position, and the men
// walk there one by one. A unit that does not come from C's side walks to C and back.
//
// The fix, on from launch (WALLFIX_OFF=1 starts off, I toggles): FUN_1016af90 called from the wall
// task (return address 0x101B0CAD) answers "close enough", so the task skips the staging march
// and the men go straight from where they stand to their wall positions, at the order's pace.
// WALLFIX_MAX_DIST=<m> (default 0 = no limit) keeps the staging march for a unit whose centre is
// farther than that from the centre of its wall positions.
//
// The research probe (WALLFIX_PROBE=1) writes <game>\wallfix_runs\<stamp>\:
//   wall_events.csv   the defend order, every unit move action created (FUN_103db410, with its
//                     caller and destination), the move controller of such a move (FUN_103e1250),
//                     the form-up actions (FUN_101a53f0 / 5500 / 5600 / 56e0, FUN_103be5d0) and
//                     their go-to-slot step (FUN_103e1780); each row names the group's
//                     behaviour-stack top task and that task's current state
//   wall_slots.csv    the per-man slots of every form-up go-to step (the wall positions)
//   wall_stacks.csv   the behaviour stack (task vtables and state names) of every group seen in
//                     an event, one row whenever it changes (sampled every 50 ms)
//   wall_astar.csv    every soldier A* search (FUN_10386d90): start, end, width, mode, layer,
//                     result, nodes, portals, time
//   (wall_events.csv also gets a "staging" row per decision of the wall task: the game's answer,
//   the fix's answer, the distance)
//   wall_men.csv      every man of every unit with a staging decision, every 200 ms: position
//                     (for the time to man the wall and the route length, fix off and on)

#include "log.h"
#include "hook.h"
#include "gamemem.h"
#include "keys.h"
#include "stack_offsets_wall.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

static uint32_t g_base = 0;

// ---------------------------------------------------------------------------
// Hooks (stolen bytes: docs/re/prologues_wallfix*.txt from tools/ghidra/dump_prologue.py; whole
// instructions, no branch into the range)
// ---------------------------------------------------------------------------

enum {
    H_ORDER = 0,       // FUN_10254e80 entry: defend-building order constructor (CANDIDATE)
    H_MOVE,            // FUN_103db410 entry: a unit move action is created (ECX = unit, arg 1 = parameters)
    H_MOVECTL,         // FUN_103e1250 entry: the move action builds its move controller (ECX = action)
    H_FORMUP_A,        // FUN_101a53f0 entry: form-up creator (ECX = task)
    H_FORMUP_B,        // FUN_101a5500 entry
    H_FORMUP_C,        // FUN_101a5600 entry (re-form)
    H_FORMUP_D,        // FUN_101a56e0 entry
    H_FORMUP_ACT,      // FUN_103be5d0 entry: form-up action constructor
    H_FORMUP_GO,       // FUN_103e1780 entry: form-up action sends each man to his slot (ECX = action)
    H_ASTAR,           // FUN_10386d90 entry: soldier A* (ECX = search, arg 1 = request); wrapped
    H_STAGE,           // FUN_1016af90 entry: "unit far from its wall positions" (ECX = task, arg 1 = box); the fix
    HOOK_COUNT
};

static const HookDef g_hookDefs[HOOK_COUNT] = {
    { "FUN_10254e80", "defend order constructor", 0x254E80, 5,
      { 0x56, 0x8B, 0x74, 0x24, 0x08 }, -1, true },                        // PUSH ESI; MOV ESI,[ESP+8]
    { "FUN_103db410", "unit move action", 0x3DB410, 6,
      { 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x04 }, -1, true },                  // PUSH ESI; MOV ESI,ECX; MOV EAX,[ESI+4]
    { "FUN_103e1250", "move controller of a move action", 0x3E1250, 6,
      { 0x83, 0xEC, 0x18, 0x57, 0x8B, 0xF9 }, -1, true },                  // SUB ESP,0x18; PUSH EDI; MOV EDI,ECX
    { "FUN_101a53f0", "form-up creator A", 0x1A53F0, 6,
      { 0x83, 0xEC, 0x14, 0x57, 0x8B, 0xF9 }, -1, true },                  // SUB ESP,0x14; PUSH EDI; MOV EDI,ECX
    { "FUN_101a5500", "form-up creator B", 0x1A5500, 5,
      { 0x83, 0xEC, 0x18, 0x53, 0x55 }, -1, true },                        // SUB ESP,0x18; PUSH EBX; PUSH EBP
    { "FUN_101a5600", "form-up creator C (re-form)", 0x1A5600, 5,
      { 0x83, 0xEC, 0x14, 0x53, 0x56 }, -1, true },                        // SUB ESP,0x14; PUSH EBX; PUSH ESI
    { "FUN_101a56e0", "form-up creator D", 0x1A56E0, 5,
      { 0x83, 0xEC, 0x18, 0x53, 0x56 }, -1, true },                        // SUB ESP,0x18; PUSH EBX; PUSH ESI
    { "FUN_103be5d0", "form-up action constructor", 0x3BE5D0, 6,
      { 0x53, 0x56, 0x8B, 0x74, 0x24, 0x0C }, -1, true },                  // PUSH EBX; PUSH ESI; MOV ESI,[ESP+0xC]
    { "FUN_103e1780", "form-up go to slots", 0x3E1780, 5,
      { 0x83, 0xEC, 0x08, 0x56, 0x57 }, -1, true },                        // SUB ESP,8; PUSH ESI; PUSH EDI
    { "FUN_10386d90", "soldier A*", 0x386D90, 8,
      { 0x83, 0xEC, 0x20, 0x53, 0x8B, 0x5C, 0x24, 0x30 }, -1, true },      // SUB ESP,0x20; PUSH EBX; MOV EBX,[ESP+0x30]
    { "FUN_1016af90", "staging test of the wall task (the fix)", 0x16AF90, 6,
      { 0x83, 0xEC, 0x18, 0x57, 0x8B, 0xF9 }, -1, false },                 // SUB ESP,0x18; PUSH EDI; MOV EDI,ECX
};

static HookSlot g_slots[HOOK_COUNT] = {};
extern "C" uint8_t* g_wallTrampolines[HOOK_COUNT] = {};

// ---------------------------------------------------------------------------
// Engine layout (fixes/wall-manning-fix/ARCHITECTURE.md; docs/re/unit_finish_x86.md,
// docs/re/missile_fire_x86.md, docs/re/request_radius_x86.md)
// ---------------------------------------------------------------------------

static const uint32_t OWN_OFF_STACK = 0x400, OWN_OFF_COUNT = 0x440, OWN_OFF_CUR = 0x454;   // behaviour stack
static const uint32_t TASK_OFF_UNIT = 0x04, TASK_OFF_OWNER = 0x08, TASK_OFF_CTX = 0x18;     // squad task
static const uint32_t CTX_OFF_STATE = 0x04;                                                 // machine context
static const uint32_t UNIT_OFF_COUNT = 0x18, UNIT_OFF_MEN = 0x1C;                           // soldiers
static const uint32_t FORM_OFF_SLOTS = 0x44, FORM_SLOT = 0x24;                              // form-up action
static const uint32_t MOVE_OFF_R = 0x28, MOVE_OFF_DEST = 0x2C, MOVE_OFF_R2 = 0x50;          // move action
static const uint32_t SEARCH_OFF_NODES = 0xC8, SEARCH_OFF_NPORT = 0xD4;                     // A* object (P+0x36C)
static const uint32_t RVA_RET_STAGING = 0x1B0CAD;   // return of the staging test call in FUN_101b0b60
static const uint32_t WTASK_OFF_UNIT = 0x7C;        // wall task: its unit
static const uint32_t Q_OFF_START = 0x04, Q_OFF_END = 0x18, Q_OFF_W = 0x2C, Q_OFF_MODE = 0x30, Q_OFF_LAYER = 0x38;

static inline uint32_t R32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint32_t Rd(uint32_t addr) { uint32_t v = 0; ReadDword(addr, &v); return v; }
static inline float RdF(uint32_t addr) { uint32_t v = Rd(addr); float f; memcpy(&f, &v, 4); return f; }
static inline uint8_t RdB(uint32_t addr) { uint8_t v = 0; ReadByte(addr, &v); return v; }
static inline uint32_t Rva(uint32_t a) { return a >= g_base && a < g_base + HOOK_ANALYSED_IMAGE_SIZE ? a - g_base : 0; }

// ---------------------------------------------------------------------------
// Fix state (the fix proper is not written yet)
// ---------------------------------------------------------------------------

static volatile LONG g_fixOn = 0;
static float g_maxDist = 0;                        // WALLFIX_MAX_DIST, 0 = no limit
static volatile LONG g_fSkipped = 0, g_fKept = 0, g_fNear = 0, g_fOther = 0;
static bool g_keyWatched = false;
static volatile LONG g_testKeys = 0;
static uint8_t* g_toneOn = nullptr;
static uint8_t* g_toneOff = nullptr;

// ---------------------------------------------------------------------------
// State names: slot +0x10 of a state's vtable is "MOV EAX,[table]; RET"; [table] is the name
// (docs/re/missile_fire_x86.md). Cached per state object.
// ---------------------------------------------------------------------------

struct StateName { uint32_t obj; char name[56]; };
static const int STATE_TABLE = 1024;
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

// The name of a state object ("-" for none, "?" when it has no readable name).
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
    NameOf(Rd(task + TASK_OFF_CTX + CTX_OFF_STATE), st, sizeof(st));
    snprintf(out, cap, "%X:%s", Rva(Rd(task)), st);
}

// The behaviour stack of an owner: "cur/count|task|task|..."; false when it does not look like one.
static bool DescStack(uint32_t owner, char* out, int cap) {
    out[0] = 0;
    if (!PtrPlausible(owner)) return false;
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
static CRITICAL_SECTION g_cs;
static char g_dir[MAX_PATH] = "";
static FILE* g_fEvents = nullptr;
static FILE* g_fSlots = nullptr;
static FILE* g_fStacks = nullptr;
static FILE* g_fAstar = nullptr;
static FILE* g_fMen = nullptr;
static LARGE_INTEGER g_qpf = {};
static volatile LONG g_rows = 0, g_seq = 0;
static const LONG MAX_ROWS = 3000000;
static volatile LONG g_cEvents = 0, g_cAstar = 0, g_cSlots = 0, g_cStacks = 0, g_cMen = 0;
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
    snprintf(root, sizeof(root), "%swallfix_runs\\", exe);
    CreateDirectoryA(root, nullptr);
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_dir, sizeof(g_dir), "%s%04d%02d%02d_%02d%02d%02d\\", root, st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute, st.wSecond);
    if (!CreateDirectoryA(g_dir, nullptr)) { Log("wallfix: cannot create %s", g_dir); return false; }
    g_fEvents = OpenCsv("wall_events.csv",
                        "seq,ms,tick,thread,event,ret_rva,obj,obj_vt,unit,owner,x,y,z,a,b,c,d,stack,hex\n");
    g_fSlots = OpenCsv("wall_slots.csv", "seq,tick,action,unit,i,soldier,x,y,z,heading,type\n");
    g_fStacks = OpenCsv("wall_stacks.csv", "ms,tick,owner,unit_x,unit_z,stack\n");
    g_fAstar = OpenCsv("wall_astar.csv",
                       "ms,tick,thread,ret_rva,q,sx,sy,sz,sreg,ex,ey,ez,ereg,w,mode,layer,result,nodes,portals,us\n");
    g_fMen = OpenCsv("wall_men.csv", "ms,tick,unit,i,soldier,x,y,z\n");
    if (!g_fEvents || !g_fSlots || !g_fStacks || !g_fAstar || !g_fMen) { Log("wallfix: cannot open the probe files in %s", g_dir); return false; }
    Log("wallfix: probe on, writing %s", g_dir);
    return true;
}

static void MaybeFlush() {
    DWORD now = GetTickCount();
    if (now - g_lastFlush < 2000) return;
    g_lastFlush = now;
    if (g_fEvents) fflush(g_fEvents);
    if (g_fSlots) fflush(g_fSlots);
    if (g_fStacks) fflush(g_fStacks);
    if (g_fAstar) fflush(g_fAstar);
    if (g_fMen) fflush(g_fMen);
}

static void Hex(const uint8_t* b, uint32_t n, char* out) {
    static const char* hx = "0123456789ABCDEF";
    for (uint32_t i = 0; i < n; i++) { out[2 * i] = hx[b[i] >> 4]; out[2 * i + 1] = hx[b[i] & 15]; }
    out[2 * n] = 0;
}

// ---------------------------------------------------------------------------
// Watched behaviour-stack owners: every owner seen in an event; a thread logs their stacks
// ---------------------------------------------------------------------------

struct Watched { uint32_t owner, unit; char last[512]; };
static const int WATCH_MAX = 64;
static Watched g_watch[WATCH_MAX];
static volatile LONG g_nWatch = 0;
static CRITICAL_SECTION g_watchCs;
static HANDLE g_sampler = nullptr;
static volatile LONG g_stop = 0;

static void Watch(uint32_t owner, uint32_t unit) {
    if (!g_probe || !PtrPlausible(owner)) return;
    EnterCriticalSection(&g_watchCs);
    bool seen = false;
    for (int i = 0; i < g_nWatch && !seen; i++) seen = g_watch[i].owner == owner;
    if (!seen && g_nWatch < WATCH_MAX) {
        g_watch[g_nWatch].owner = owner;
        g_watch[g_nWatch].unit = unit;
        g_watch[g_nWatch].last[0] = 0;
        g_nWatch++;
    }
    LeaveCriticalSection(&g_watchCs);
}

// Mean position of a unit's soldiers (soldier position e+0x524.. per docs/re/request_radius_x86.md,
// FUN_103df250); false when the list does not look like one.
static bool UnitCentre(uint32_t unit, float* x, float* z) {
    uint32_t n = Rd(unit + UNIT_OFF_COUNT), men = Rd(unit + UNIT_OFF_MEN);
    if (n == 0 || n > 400 || !PtrPlausible(men)) return false;
    double sx = 0, sz = 0;
    int k = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t e = Rd(men + 4 * i);
        if (!PtrPlausible(e)) continue;
        sx += RdF(e + 0x524);
        sz += RdF(e + 0x52C);
        k++;
    }
    if (!k) return false;
    *x = (float)(sx / k);
    *z = (float)(sz / k);
    return true;
}

// Units whose men are sampled: every unit with a staging decision of the wall task.
static uint32_t g_menUnits[16];
static volatile LONG g_nMenUnits = 0;

static void WatchMen(uint32_t unit) {
    if (!g_probe || !PtrPlausible(unit)) return;
    EnterCriticalSection(&g_watchCs);
    bool seen = false;
    for (int i = 0; i < g_nMenUnits && !seen; i++) seen = g_menUnits[i] == unit;
    if (!seen && g_nMenUnits < 16) g_menUnits[g_nMenUnits++] = unit;
    LeaveCriticalSection(&g_watchCs);
}

// One row per man of every sampled unit (soldier position e+0x524 / +0x528 / +0x52C). Under g_watchCs.
static void SampleMen(uint32_t tick) {
    for (int k = 0; k < g_nMenUnits; k++) {
        uint32_t unit = g_menUnits[k];
        uint32_t n = Rd(unit + UNIT_OFF_COUNT), men = Rd(unit + UNIT_OFF_MEN);
        if (n == 0 || n > 400 || !PtrPlausible(men)) continue;
        EnterCriticalSection(&g_cs);
        for (uint32_t i = 0; i < n && g_fMen && RowBudget(); i++) {
            uint32_t e = Rd(men + 4 * i);
            if (!PtrPlausible(e)) continue;
            fprintf(g_fMen, "%u,%u,%X,%u,%X,%.2f,%.2f,%.2f\n", Ms(), tick, unit, i, e, RdF(e + 0x524), RdF(e + 0x528),
                    RdF(e + 0x52C));
            InterlockedIncrement(&g_cMen);
        }
        LeaveCriticalSection(&g_cs);
    }
}

static DWORD WINAPI SamplerThread(LPVOID) {
    uint32_t lastMenTick = 0;
    while (!InterlockedCompareExchange(&g_stop, 0, 0)) {
        Sleep(50);
        uint32_t tick = GameTickAt(g_base);
        if (!tick) continue;
        if (tick >= lastMenTick + 2 || tick < lastMenTick) {
            lastMenTick = tick;
            EnterCriticalSection(&g_watchCs);
            SampleMen(tick);
            LeaveCriticalSection(&g_watchCs);
        }
        EnterCriticalSection(&g_watchCs);
        int n = g_nWatch;
        for (int i = 0; i < n; i++) {
            char s[512];
            if (!DescStack(g_watch[i].owner, s, sizeof(s))) continue;
            if (strcmp(s, g_watch[i].last) == 0) continue;
            memcpy(g_watch[i].last, s, sizeof(s));
            float x = 0, z = 0;
            UnitCentre(g_watch[i].unit, &x, &z);
            if (!RowBudget()) continue;
            EnterCriticalSection(&g_cs);
            if (g_fStacks) fprintf(g_fStacks, "%u,%u,%X,%.1f,%.1f,%s\n", Ms(), tick, g_watch[i].owner, x, z, s);
            InterlockedIncrement(&g_cStacks);
            LeaveCriticalSection(&g_cs);
        }
        LeaveCriticalSection(&g_watchCs);
        EnterCriticalSection(&g_cs);
        MaybeFlush();
        LeaveCriticalSection(&g_cs);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Event rows
// ---------------------------------------------------------------------------

struct Ev {
    const char* event;
    uint32_t ret, obj, unit, owner;
    float x, y, z;
    uint32_t a, b, c, d;
    const uint8_t* raw;
    uint32_t rawLen;
};

static LONG EventRow(const Ev& v) {
    if (!g_probe || !RowBudget()) return 0;
    LONG seq = InterlockedIncrement(&g_seq);
    char stack[512] = "";
    DescStack(v.owner, stack, sizeof(stack));
    char hex[2 * 0x80 + 1] = "";
    if (v.raw && v.rawLen) Hex(v.raw, v.rawLen > 0x80 ? 0x80 : v.rawLen, hex);
    EnterCriticalSection(&g_cs);
    if (g_fEvents)
        fprintf(g_fEvents, "%ld,%u,%u,%lu,%s,%X,%X,%X,%X,%X,%.2f,%.2f,%.2f,%X,%X,%X,%X,%s,%s\n", seq, Ms(),
                GameTickAt(g_base), GetCurrentThreadId(), v.event, Rva(v.ret), v.obj, Rva(Rd(v.obj)), v.unit, v.owner,
                v.x, v.y, v.z, v.a, v.b, v.c, v.d, stack, hex);
    InterlockedIncrement(&g_cEvents);
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
    Watch(v.owner, v.unit);
    return seq;
}

// FUN_10254e80(ECX = order, arg 1 = source record, arg 2 = target, arg 3 / arg 4 = flag bytes)
static void HandleOrder(const uint8_t* ctx) {
    uint8_t raw[0x40] = {};
    uint32_t src = R32(ctx + WALL_OFF_ARG1);
    CopyGuarded(src, raw, sizeof(raw));
    Ev v = { "order", R32(ctx + WALL_OFF_RET), R32(ctx + WALL_OFF_ECX), 0, 0, 0, 0, 0,
             src, R32(ctx + WALL_OFF_ARG2), R32(ctx + WALL_OFF_ARG3) & 0xFF, R32(ctx + WALL_OFF_ARG4) & 0xFF,
             raw, sizeof(raw) };
    EventRow(v);
}

// FUN_103db410(ECX = unit, arg 1 = P): P+0 path result, P+4 destination {x, y, z, region, aux},
// P+0x18 heading given, P+0x1A heading, P+0x20 width.
static void HandleMove(const uint8_t* ctx) {
    uint32_t unit = R32(ctx + WALL_OFF_ECX), p = R32(ctx + WALL_OFF_ARG1);
    uint8_t raw[0x28] = {};
    CopyGuarded(p, raw, sizeof(raw));
    float d[3];
    memcpy(d, raw + 4, 12);
    uint32_t flags = raw[0x18] | (raw[0x1A] << 8) | (raw[0x1B] << 16);
    uint32_t width;
    memcpy(&width, raw + 0x20, 4);
    Ev v = { "move", R32(ctx + WALL_OFF_RET), p, unit, unit, d[0], d[1], d[2], R32(raw), flags, width,
             (uint32_t)(raw[0x24] | (raw[0x25] << 8)), raw, sizeof(raw) };
    EventRow(v);
}

// FUN_103e1250(ECX = move action): +0x28 path result, +0x2C destination, +0x40 / +0x42 heading
// mode / heading, +0x50 the action's own unit path search; [[action+8]] = the unit.
static void HandleMoveCtl(const uint8_t* ctx) {
    uint32_t act = R32(ctx + WALL_OFF_ECX);
    uint8_t raw[0x60] = {};
    CopyGuarded(act, raw, sizeof(raw));
    float d[3];
    memcpy(d, raw + MOVE_OFF_DEST, 12);
    uint32_t unit = Rd(Rd(act + 8));
    Ev v = { "movectl", R32(ctx + WALL_OFF_RET), act, unit, unit, d[0], d[1], d[2], R32(raw + MOVE_OFF_R),
             R32(raw + MOVE_OFF_R2), (uint32_t)(raw[0x40] | (raw[0x42] << 8) | (raw[0x43] << 16)), R32(raw + 0x44), raw,
             sizeof(raw) };
    EventRow(v);
}

static void HandleFormupCreator(const uint8_t* ctx, const char* which) {
    uint32_t task = R32(ctx + WALL_OFF_ECX);
    uint32_t unit = Rd(task + TASK_OFF_UNIT), owner = Rd(task + TASK_OFF_OWNER);
    char st[64];
    NameOf(Rd(task + TASK_OFF_CTX + CTX_OFF_STATE), st, sizeof(st));
    uint8_t raw[0x40] = {};
    CopyGuarded(task, raw, sizeof(raw));
    float x = 0, z = 0;
    UnitCentre(unit, &x, &z);
    Ev v = { which, R32(ctx + WALL_OFF_RET), task, unit, owner, x, 0, z, 0, 0, 0, 0, raw, sizeof(raw) };
    EventRow(v);
}

// FUN_103be5d0(ECX = action, arg 1 = owner, arg 2 = parameters)
static void HandleFormupAct(const uint8_t* ctx) {
    uint32_t p = R32(ctx + WALL_OFF_ARG2);
    uint8_t raw[0x18] = {};
    CopyGuarded(p, raw, sizeof(raw));
    Ev v = { "formup_act", R32(ctx + WALL_OFF_RET), R32(ctx + WALL_OFF_ECX), 0, R32(ctx + WALL_OFF_ARG1), 0, 0, 0,
             R32(raw), R32(raw + 4), R32(raw + 8), R32(raw + 12), raw, sizeof(raw) };
    EventRow(v);
}

// FUN_103e1780(ECX = form-up action): [action+4] = unit, slots at [action+0x44], 0x24 bytes each:
// +4 position, +0x18 heading, +0x20 type; one per soldier.
static void HandleFormupGo(const uint8_t* ctx) {
    uint32_t act = R32(ctx + WALL_OFF_ECX);
    uint32_t unit = Rd(act + 4), slots = Rd(act + FORM_OFF_SLOTS);
    uint32_t n = Rd(unit + UNIT_OFF_COUNT), men = Rd(unit + UNIT_OFF_MEN);
    float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
    uint32_t types = 0;
    LONG seq = 0;
    if (n <= 400 && PtrPlausible(slots)) {
        for (uint32_t i = 0; i < n; i++) {
            uint8_t s[FORM_SLOT];
            if (!MemcpyTry(s, slots + FORM_SLOT * i, FORM_SLOT)) break;
            float p[3];
            memcpy(p, s + 4, 12);
            for (int k = 0; k < 3; k++) { if (p[k] < lo[k]) lo[k] = p[k]; if (p[k] > hi[k]) hi[k] = p[k]; }
            uint32_t t = R32(s + 0x20);
            if (t < 32) types |= 1u << t;
        }
    }
    Ev v = { "formup_go", R32(ctx + WALL_OFF_RET), act, unit, Rd(act + 8), (lo[0] + hi[0]) / 2, lo[1],
             (lo[2] + hi[2]) / 2, n, types, 0, 0, nullptr, 0 };
    uint32_t box[4];
    memcpy(&box[0], &lo[0], 4); memcpy(&box[1], &hi[0], 4); memcpy(&box[2], &lo[2], 4); memcpy(&box[3], &hi[2], 4);
    v.c = box[0];
    v.d = box[3];
    seq = EventRow(v);
    if (!seq || n > 400 || !PtrPlausible(slots)) return;
    EnterCriticalSection(&g_cs);
    for (uint32_t i = 0; i < n && g_fSlots; i++) {
        uint8_t s[FORM_SLOT];
        if (!MemcpyTry(s, slots + FORM_SLOT * i, FORM_SLOT)) break;
        float p[3];
        memcpy(p, s + 4, 12);
        uint16_t hd;
        memcpy(&hd, s + 0x18, 2);
        fprintf(g_fSlots, "%ld,%u,%X,%X,%u,%X,%.2f,%.2f,%.2f,%u,%u\n", seq, GameTickAt(g_base), act, unit, i,
                PtrPlausible(men) ? Rd(men + 4 * i) : 0, p[0], p[1], p[2], hd, R32(s + 0x20));
        InterlockedIncrement(&g_cSlots);
    }
    LeaveCriticalSection(&g_cs);
}

// FUN_10386d90 wrapped: the original runs through the trampoline, then the search is logged.
typedef uint32_t (__fastcall* AstarFn)(uint32_t self, uint32_t edx, uint32_t q, uint32_t s, uint32_t e);

static uint32_t HandleAstar(const uint8_t* ctx) {
    uint32_t self = R32(ctx + WALL_OFF_ECX), q = R32(ctx + WALL_OFF_ARG1);
    uint32_t sr = R32(ctx + WALL_OFF_ARG2), er = R32(ctx + WALL_OFF_ARG3);
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    uint32_t r = ((AstarFn)g_wallTrampolines[H_ASTAR])(self, 0, q, sr, er);
    QueryPerformanceCounter(&t1);
    uint32_t tick = GameTickAt(g_base);
    if (tick && g_probe && RowBudget()) {
        uint8_t qb[0x48] = {};
        CopyGuarded(q, qb, sizeof(qb));
        float s[3], e[3], w;
        memcpy(s, qb + Q_OFF_START, 12);
        memcpy(e, qb + Q_OFF_END, 12);
        memcpy(&w, qb + Q_OFF_W, 4);
        int64_t us = g_qpf.QuadPart ? (t1.QuadPart - t0.QuadPart) * 1000000 / g_qpf.QuadPart : 0;
        EnterCriticalSection(&g_cs);
        if (g_fAstar)
            fprintf(g_fAstar, "%u,%u,%lu,%X,%X,%.2f,%.2f,%.2f,%X,%.2f,%.2f,%.2f,%X,%.2f,%u,%u,%u,%u,%u,%lld\n", Ms(),
                    tick, GetCurrentThreadId(), Rva(R32(ctx + WALL_OFF_RET)), q, s[0], s[1], s[2],
                    R32(qb + Q_OFF_START + 12), e[0], e[1], e[2], R32(qb + Q_OFF_END + 12), w, R32(qb + Q_OFF_MODE),
                    R32(qb + Q_OFF_LAYER), r & 0xFF, Rd(self + SEARCH_OFF_NODES), Rd(self + SEARCH_OFF_NPORT), us);
        InterlockedIncrement(&g_cAstar);
        MaybeFlush();
        LeaveCriticalSection(&g_cs);
    }
    return HOOK_RETURN(r);
}

// FUN_1016af90 wrapped (ECX = task, arg 1 = box {min x, min z, max x, max z}; RET 4). The game's
// answer comes from the trampoline; for the wall task's call the fix may answer 0 instead.
typedef uint32_t (__fastcall* StageFn)(uint32_t self, uint32_t edx, uint32_t box);

static uint32_t HandleStage(const uint8_t* ctx) {
    uint32_t ret = R32(ctx + WALL_OFF_RET);
    bool on = InterlockedCompareExchange(&g_fixOn, 0, 0) != 0;
    if (ret != g_base + RVA_RET_STAGING) {
        InterlockedIncrement(&g_fOther);
        return HOOK_CONTINUE;
    }
    if (!on && !g_probe) return HOOK_CONTINUE;
    uint32_t task = R32(ctx + WALL_OFF_ECX), boxp = R32(ctx + WALL_OFF_ARG1);
    uint32_t game = ((StageFn)g_wallTrampolines[H_STAGE])(task, 0, boxp);
    float box[4] = {};
    MemcpyTry(box, boxp, sizeof(box));
    float bx = (box[0] + box[2]) * 0.5f, bz = (box[1] + box[3]) * 0.5f, ux = 0, uz = 0;
    uint32_t unit = Rd(task + WTASK_OFF_UNIT);
    bool haveUnit = UnitCentre(unit, &ux, &uz);
    float dist = haveUnit ? sqrtf((ux - bx) * (ux - bx) + (uz - bz) * (uz - bz)) : -1.0f;
    uint32_t answer = game;
    if ((game & 0xFF) == 0) {
        InterlockedIncrement(&g_fNear);
    } else if (on && (g_maxDist <= 0 || (haveUnit && dist <= g_maxDist))) {
        answer = game & 0xFFFFFF00u;
        InterlockedIncrement(&g_fSkipped);
    } else if (on) {
        InterlockedIncrement(&g_fKept);
    }
    if (g_probe) {
        Ev v = { "staging", ret, task, unit, unit + 0x20, ux, dist, uz, game & 0xFF, answer & 0xFF, 0, 0,
                 (const uint8_t*)box, sizeof(box) };
        memcpy(&v.c, &bx, 4);
        memcpy(&v.d, &bz, 4);
        EventRow(v);
        WatchMen(unit);
    }
    return HOOK_RETURN(answer);
}

// ---------------------------------------------------------------------------
// Key, detours, harness exports
// ---------------------------------------------------------------------------

// I toggles the fix (dev build; focus only through proxy/keys). Harness: WallFixTestKeys bit 0.
extern "C" __declspec(dllexport) void __stdcall WallFixTestKeys(LONG bits) { InterlockedExchange(&g_testKeys, bits); }

static void ToggleKey() {
    bool on = InterlockedCompareExchange(&g_fixOn, 0, 0) == 0;
    InterlockedExchange(&g_fixOn, on ? 1 : 0);
    Log("wallfix: I, fix %s", on ? "ON" : "OFF");
    Tone_Play(on ? g_toneOn : g_toneOff);
}

extern "C" void __stdcall WallHandler0(const uint8_t* ctx) { HandleOrder(ctx); }
extern "C" void __stdcall WallHandler1(const uint8_t* ctx) { HandleMove(ctx); }
extern "C" void __stdcall WallHandler2(const uint8_t* ctx) { HandleMoveCtl(ctx); }
extern "C" void __stdcall WallHandler3(const uint8_t* ctx) { HandleFormupCreator(ctx, "formup_a"); }
extern "C" void __stdcall WallHandler4(const uint8_t* ctx) { HandleFormupCreator(ctx, "formup_b"); }
extern "C" void __stdcall WallHandler5(const uint8_t* ctx) { HandleFormupCreator(ctx, "formup_c"); }
extern "C" void __stdcall WallHandler6(const uint8_t* ctx) { HandleFormupCreator(ctx, "formup_d"); }
extern "C" void __stdcall WallHandler7(const uint8_t* ctx) { HandleFormupAct(ctx); }
extern "C" void __stdcall WallHandler8(const uint8_t* ctx) { HandleFormupGo(ctx); }
extern "C" uint32_t __stdcall WallHandler9(const uint8_t* ctx) { return HandleAstar(ctx); }
extern "C" uint32_t __stdcall WallHandler10(const uint8_t* ctx) { return HandleStage(ctx); }

HOOK_DETOUR(WallDetour0, WallHandler0, g_wallTrampolines, 0)
HOOK_DETOUR(WallDetour1, WallHandler1, g_wallTrampolines, 1)
HOOK_DETOUR(WallDetour2, WallHandler2, g_wallTrampolines, 2)
HOOK_DETOUR(WallDetour3, WallHandler3, g_wallTrampolines, 3)
HOOK_DETOUR(WallDetour4, WallHandler4, g_wallTrampolines, 4)
HOOK_DETOUR(WallDetour5, WallHandler5, g_wallTrampolines, 5)
HOOK_DETOUR(WallDetour6, WallHandler6, g_wallTrampolines, 6)
HOOK_DETOUR(WallDetour7, WallHandler7, g_wallTrampolines, 7)
HOOK_DETOUR(WallDetour8, WallHandler8, g_wallTrampolines, 8)
HOOK_DETOUR_RET(WallDetour9, WallHandler9, g_wallTrampolines, 9, 0xC)   // FUN_10386d90: __thiscall, 3 stack args
HOOK_DETOUR_RET(WallDetour10, WallHandler10, g_wallTrampolines, 10, 4)   // FUN_1016af90: __thiscall, 1 stack arg

static const HookDetour g_detours[HOOK_COUNT] = { WallDetour0, WallDetour1, WallDetour2, WallDetour3, WallDetour4,
                                                  WallDetour5, WallDetour6, WallDetour7, WallDetour8, WallDetour9,
                                                  WallDetour10 };

// Harness only (fixes/wall-manning-fix/tests): fix and probe counters.
extern "C" __declspec(dllexport) void __stdcall WallFixTestCounters(long* out) {
    out[0] = g_probe ? 1 : 0;
    out[1] = g_fixOn;
    out[2] = g_cEvents;
    out[3] = g_cAstar;
    out[4] = g_cSlots;
    out[5] = g_cStacks;
    out[6] = g_base ? 1 : 0;
    out[7] = g_nWatch;
    out[8] = g_fSkipped;
    out[9] = g_fKept;
    out[10] = g_fNear;
    out[11] = g_fOther;
    out[12] = (long)g_maxDist;
    out[13] = g_cMen;
}

// ---------------------------------------------------------------------------
// Install / remove
// ---------------------------------------------------------------------------

static float EnvFloat(const char* name, float def, float lo, float hi) {
    char v[16] = "";
    if (GetEnvironmentVariableA(name, v, sizeof(v)) == 0) return def;
    float x = (float)atof(v);
    if (x < lo || x > hi) {
        Log("wallfix: %s=%s out of range %.0f..%.0f, using %.0f", name, v, lo, hi, def);
        return def;
    }
    return x;
}

static bool EnvIs(const char* name, char c) {
    char v[8] = "";
    return GetEnvironmentVariableA(name, v, sizeof(v)) > 0 && v[0] == c;
}

void WallFix_Install() {
    Log("wallfix: wall manning fix (dev build)");
    Mem_Init();
    uint32_t size = 0;
    uint8_t* base = Hook_GameModule(false, &size);
    if (!base) { Log("wallfix: REFUSED, empire.retail.dll is not loaded"); return; }
    if (size != HOOK_ANALYSED_IMAGE_SIZE) { Log("wallfix: REFUSED, SizeOfImage 0x%08X is not the analysed build", size); return; }
    for (int h = 0; h < HOOK_COUNT; h++) {
        if (!Hook_SiteMatches(base, g_hookDefs[h])) {
            Log("wallfix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", g_hookDefs[h].rva,
                g_hookDefs[h].func);
            return;
        }
    }
    g_base = (uint32_t)(uintptr_t)base;
    InitializeCriticalSection(&g_cs);
    InitializeCriticalSection(&g_nameCs);
    InitializeCriticalSection(&g_watchCs);
    QueryPerformanceFrequency(&g_qpf);
    static const ToneSeg on[] = { { 880, 160 } };
    static const ToneSeg off[] = { { 330, 300 } };
    g_toneOn = Tone_Make(on, 1);
    g_toneOff = Tone_Make(off, 1);
    bool fixOff = EnvIs("WALLFIX_OFF", '1');
    g_maxDist = EnvFloat("WALLFIX_MAX_DIST", 0, 0, 5000);
    if (EnvIs("WALLFIX_PROBE", '1')) {
        if (OpenProbe()) g_probe = true;
        else Log("wallfix: probe not started (no run folder)");
    }
    bool use[HOOK_COUNT];
    for (int h = 0; h < HOOK_COUNT; h++) use[h] = !g_hookDefs[h].probeOnly || g_probe;
    if (!Hook_Prepare(base, g_hookDefs, HOOK_COUNT, use, g_wallTrampolines, g_slots)) {
        Log("wallfix: REFUSED, no trampoline memory");
        g_probe = false;
        g_base = 0;
        return;
    }
    InterlockedExchange(&g_fixOn, fixOff ? 0 : 1);
    if (g_probe) g_sampler = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    Hook_Patch(g_detours, g_slots, HOOK_COUNT);
    for (int h = 0; h < HOOK_COUNT; h++)
        if (g_slots[h].active)
            Log("wallfix: hook %s (%s) installed at RVA 0x%06X", g_hookDefs[h].func, g_hookDefs[h].role, g_hookDefs[h].rva);
    if (g_maxDist > 0)
        Log("wallfix: fix %s: a unit ordered onto a wall skips the staging march when within %.0f m of its wall positions",
            fixOff ? "OFF (WALLFIX_OFF=1)" : "ON", g_maxDist);
    else
        Log("wallfix: fix %s: a unit ordered onto a wall skips the staging march and goes straight to its wall positions",
            fixOff ? "OFF (WALLFIX_OFF=1)" : "ON");
    g_keyWatched = Keys_Watch('I', &g_testKeys, 1, ToggleKey);
    Log("wallfix: I toggles the fix on / off%s", g_keyWatched ? "" : " (ERROR: key poll not started)");
}

void WallFix_Remove() {
    if (g_keyWatched) { Keys_Unwatch(ToggleKey); g_keyWatched = false; }
    Hook_Remove(g_slots, HOOK_COUNT);
    if (!g_base) return;
    Log("wallfix: staging marches skipped %ld, kept beyond WALLFIX_MAX_DIST %ld, not needed (unit close) %ld, other "
        "callers of the test %ld", g_fSkipped, g_fKept, g_fNear, g_fOther);
    if (g_sampler) {
        InterlockedExchange(&g_stop, 1);
        WaitForSingleObject(g_sampler, 1000);
        CloseHandle(g_sampler);
        g_sampler = nullptr;
    }
    if (g_probe) {
        EnterCriticalSection(&g_cs);
        if (g_fEvents) fclose(g_fEvents);
        if (g_fSlots) fclose(g_fSlots);
        if (g_fStacks) fclose(g_fStacks);
        if (g_fAstar) fclose(g_fAstar);
        if (g_fMen) fclose(g_fMen);
        g_fEvents = g_fSlots = g_fStacks = g_fAstar = g_fMen = nullptr;
        g_probe = false;
        LeaveCriticalSection(&g_cs);
        Log("wallfix: probe closed: events %ld, slots %ld, stack rows %ld, A* searches %ld, man samples %ld, watched "
            "owners %ld, rows %ld", g_cEvents, g_cSlots, g_cStacks, g_cAstar, g_cMen, g_nWatch, g_rows);
    }
}
