// fbrfix.cpp -- fire-by-rank fix for Total War: Shogun 2 (original x86 Empire.Retail.dll).
// Folder fixes/fire-by-rank-fix/, code name fbrfix.
//
// Dev build. Shogun 2's fire by rank is the group fire task (vtable 0x115C1048): the men are put
// in up to 3 groups; the task waits until every man of the current group is "loaded and ready",
// orders the group to fire, waits until every man has fired, gives the group reload orders, and
// moves to the next group. Two faults, both fixed here:
//
//   1. Hattori / Ikko Ikki: their gun units carry a sword officer (officer_katana: katana
//      animations, no gun). After the first volley his group's reload order (kneeling) never
//      completes for him, so the wait for his group never ends and the whole unit stops firing
//      and reloading for good.
//   2. Every unit: the wait before the first volley lasts about 10 s, because it waits for the
//      last man to finish his ready pose; also later waits hang on single stragglers.
//
// The fix (three entry hooks, on from launch; FBRFIX_OFF=1 starts it off, Y toggles it):
//   - FUN_10188290 (the group task's order to get ready / reload): skipped for a man who is not
//     of his unit's shooter class (soldier +0x48C differs from the unit's most common value;
//     the sword officer is the only such man seen), so he never gets an order he cannot finish.
//   - FUN_1017b980 (the "group loaded and ready" test): the group may fire when every shooter of
//     it is loaded and either ready or the wait has lasted FBRFIX_GRACE game ticks (default 15
//     = 1.5 s); men outside the shooter class are not waited for.
//   - FUN_1017b9d0 (the "group fire completed" test; part 2, early reload): a man's reload time is
//     set at his shot but only counts down once he has a reload order, and the game gives the
//     group's reload orders when every man of the group has finished his fire order, so the men
//     who fired wait (2 to 9 s measured) for the men who cannot get a shot. The hook gives each
//     shooter his kneeling reload order (the game's own FUN_10188290) as soon as he has fired and
//     his fire order is done, and answers the test for those men (FBRFIX_EARLY_RELOAD=0 turns
//     it off). FBRFIX_FIRE_GRACE=<ticks> (default 0 = off) also ends the turn that many ticks
//     after the group's last shot; the men who did not fire keep their load.
//   - Option FBRFIX_ALL_RANKS=1 (off by default): the game caps the task at 3 groups, so in a
//     formation deeper than 3 ranks the rear ranks never fire; with the option every rank fires
//     in turn (the group count is raised before the first volley).
//
// The probe (FBRFIX_PROBE=1, research) hooks the game's generic state-machine runner and the two
// soldier order functions and writes <game>\fbrfix_runs\<stamp>\:
//
//   fbr_states.csv    every state change of every machine that ever showed a shooting state
//                     (machine context, owner class, unit, old and new state names, task row)
//   fbr_rowdump.csv   on entering a row or group state of a fire-by-rank task (the group fire
//                     task Shogun 2 uses, or the row task), and every game second while it
//                     waits: every soldier of the unit (row or group, reload time, ready bytes)
//   fbr_reload.csv    per soldier: reload time leaving 0 (fired: a shot), first going down after
//                     that (counting: the reload has started) and reaching 0 (loaded again),
//                     sampled every tick for units with a shooting state
//   fbr_orders.csv    orders given to soldiers of those units (which order function, caller,
//                     first words of the order; the target of fire orders)
//   fbr_units.csv     every unit's soldier positions once per game second
//   fbr_unitinfo.csv  once per unit: soldier count, record, key-like strings in its record
//
// Engine decode (docs/re/missile_fire_x86.md): a state machine context holds its current state
// at +4; the runner FUN_11374cf0 (this = context) calls state->update (slot 0); a non-null
// result is the next state: the old state's exit (slot +0xC) runs, the new state is stored and
// its enter (slot +4) runs. Slot +0x10 returns the state's name. For squad tasks the context is
// the task + 0x18 and the task's unit is at task + 4.

#include "log.h"
#include "hook.h"
#include "gamemem.h"
#include "keys.h"
#include "stack_offsets_fbr.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

static uint32_t g_base = 0;

// ---------------------------------------------------------------------------
// Hooks (stolen bytes: docs/re/prologues_fbrfix.txt, prologues_fbrfix_mid.txt from
// tools/ghidra/dump_prologue.py; all relocatable, no branch into the range)
// ---------------------------------------------------------------------------

enum {
    H_RUNNER = 0,      // FUN_11374cf0 entry: state machine tick (ECX = context)
    H_TRANSITION,      // FUN_11374cf0+0x1B: after the old state's exit, before the new is stored
    H_ORDER_A,         // FUN_103db8f0 entry: soldier takes an order (reload, fire, ...)
    H_ORDER_B,         // FUN_103db740 entry: soldier takes an order (aim / ready)
    H_GROUP_READY,     // FUN_1017b980 entry: group task "current group loaded and ready" (the fix)
    H_GROUP_ORDER,     // FUN_10188290 entry: group task gives one man his ready / reload order (the fix)
    H_GROUP_DONE,      // FUN_1017b9d0 entry: group task "current group's fire orders done" (the fix, early reload)
    HOOK_COUNT
};

static const HookDef g_hookDefs[HOOK_COUNT] = {
    { "FUN_11374cf0", "state machine runner", 0x1374CF0, 5,
      { 0x56, 0x8B, 0xF1, 0x57, 0x56 }, -1, true },              // PUSH ESI; MOV ESI,ECX; PUSH EDI; PUSH ESI
    { "FUN_11374cf0+0x1B", "state transition", 0x1374D0B, 5,
      { 0x89, 0x7E, 0x04, 0x8B, 0xCF }, -1, true },              // MOV [ESI+4],EDI; MOV ECX,EDI
    { "FUN_103db8f0", "soldier order A", 0x3DB8F0, 5,
      { 0x53, 0x56, 0x57, 0x8B, 0xF9 }, -1, true },              // PUSH EBX; PUSH ESI; PUSH EDI; MOV EDI,ECX
    { "FUN_103db740", "soldier order B", 0x3DB740, 5,
      { 0x53, 0x56, 0x57, 0x8B, 0xF9 }, -1, true },              // same prologue
    // docs/re/prologues_fbrfix_fix.txt
    { "FUN_1017b980", "group loaded and ready test", 0x17B980, 5,
      { 0x53, 0x56, 0x57, 0x8B, 0xF9 }, -1, false },             // PUSH EBX; PUSH ESI; PUSH EDI; MOV EDI,ECX
    { "FUN_10188290", "group ready / reload order", 0x188290, 5,
      { 0x83, 0xEC, 0x50, 0x8B, 0xC1 }, -1, false },             // SUB ESP,0x50; MOV EAX,ECX
    // docs/re/prologues_fbrfix_done.txt
    { "FUN_1017b9d0", "group fire completed test", 0x17B9D0, 5,
      { 0x53, 0x56, 0x57, 0x8B, 0xF1 }, -1, false },             // PUSH EBX; PUSH ESI; PUSH EDI; MOV ESI,ECX
};

static HookSlot g_slots[HOOK_COUNT] = {};
extern "C" uint8_t* g_fbrTrampolines[HOOK_COUNT] = {};

// ---------------------------------------------------------------------------
// Engine layout (decoded statically, see the header)
// ---------------------------------------------------------------------------

static const uint32_t CTX_OFF_STATE = 0x04;          // machine context: current state object
static const uint32_t TASK_CTX_OFF = 0x18;           // squad task: machine context at +0x18
static const uint32_t TASK_OFF_UNIT = 0x04;          // squad task: unit
static const uint32_t RVA_VOLLEY_TASK_VT = 0x15C0564;   // fire-by-rank (volley) task vtable
static const uint32_t VOL_OFF_ROW = 0x30, VOL_OFF_ROWS = 0x34, VOL_OFF_FILES = 0x38;
static const uint32_t VOL_OFF_TABLE = 0x58, VOL_OFF_TABLE_N = 0x5C;   // (row, column) per soldier index
// Group fire task (the one Shogun 2's fire by rank runs; constructor FUN_101295d0): groups capped at 3.
static const uint32_t RVA_GROUP_TASK_VT = 0x15C1048;
static const uint32_t GRP_OFF_COUNT = 0x38, GRP_OFF_CUR = 0x40, GRP_OFF_FIRED = 0x44, GRP_OFF_WRAPPED = 0x45;
static const uint32_t GRP_OFF_TABLE = 0x48;          // dword group per soldier index
static const uint32_t UNIT_OFF_COUNT = 0x18, UNIT_OFF_SOLDIERS = 0x1C, UNIT_OFF_RECORD = 0x510;
static const uint32_t SOL_OFF_UNIT = 0x3B4, SOL_OFF_INDEX = 0x218, SOL_OFF_RELOAD = 0x390;
static const uint32_t SOL_OFF_ORDERED = 0x384, SOL_OFF_DONE = 0x308, SOL_OFF_ACTION = 0x248;
static const uint32_t SOL_OFF_STATE7C = 0x1F0;       // dword index 0x7C (checked by the wait state)
static const uint32_t MAX_MEN = 512;

struct Vec3 { float x, y, z; };

static uint32_t Tick() { return GameTickAt(g_base); }

static bool Rd32(uint32_t a, uint32_t* v) { return MemcpyTry(v, a, 4); }

// A unit: plausible soldier array whose first soldier links back to it.
static bool IsUnit(uint32_t unit, uint32_t* count, uint32_t* arr) {
    uint32_t n, a, s0, back;
    if (!PtrPlausible(unit) || !Rd32(unit + UNIT_OFF_COUNT, &n) || !Rd32(unit + UNIT_OFF_SOLDIERS, &a)) return false;
    if (n == 0 || n > MAX_MEN || !PtrPlausible(a) || !Rd32(a, &s0) || !PtrPlausible(s0)) return false;
    if (!Rd32(s0 + SOL_OFF_UNIT, &back) || back != unit) return false;
    *count = n;
    *arr = a;
    return true;
}

// ---------------------------------------------------------------------------
// State names: slot +0x10 of a state's vtable is "MOV EAX,[table]; RET"; [table] is the name.
// ---------------------------------------------------------------------------

struct StateInfo { uint32_t obj; char name[56]; uint8_t interest; };
static const int STATE_TABLE = 1024;   // power of two
static StateInfo g_states[STATE_TABLE];
static volatile LONG g_stateLock = 0;

static const char* const kInterest[] = {
    "ROW", "CAN_SHOOT", "FIRE", "FACE_TARGET", "RELOAD", "MISSILE", "SPINE", "AIM", "VOLLEY", "GROUP",
    "SHOOT", "LOADED",
};

static bool NameOfState(uint32_t obj, char* out, int cap) {
    uint32_t vt, fn, table, str;
    uint8_t code[6];
    if (!Rd32(obj, &vt) || !Rd32(vt + 0x10, &fn) || !MemcpyTry(code, fn, 6)) return false;
    if (code[0] != 0xA1 || code[5] != 0xC3) return false;
    memcpy(&table, code + 1, 4);
    if (!Rd32(table, &str) || !MemcpyTry(out, str, cap - 1)) return false;
    out[cap - 1] = 0;
    for (int i = 0; i < cap - 1; i++) {
        char c = out[i];
        if (!c) break;
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || (c >= 'a' && c <= 'z'))) return false;
    }
    return out[0] != 0;
}

// Cached lookup; null name = not a named state.
static const StateInfo* State(uint32_t obj) {
    if (!obj) return nullptr;
    uint32_t h = ((obj >> 2) * 2654435761u) & (STATE_TABLE - 1);
    for (int i = 0; i < 16; i++) {
        StateInfo& s = g_states[(h + i) & (STATE_TABLE - 1)];
        if (s.obj == obj) return &s;
        if (s.obj == 0) {
            while (InterlockedCompareExchange(&g_stateLock, 1, 0) != 0) {}
            if (s.obj == 0) {
                StateInfo tmp = {};
                tmp.obj = obj;
                if (!NameOfState(obj, tmp.name, sizeof(tmp.name))) strcpy(tmp.name, "?");
                for (const char* k : kInterest) if (strstr(tmp.name, k)) tmp.interest = 1;
                s.interest = tmp.interest;
                memcpy(s.name, tmp.name, sizeof(s.name));
                InterlockedExchange((volatile LONG*)&s.obj, (LONG)obj);
            }
            InterlockedExchange(&g_stateLock, 0);
            if (s.obj == obj) return &s;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Watched machines and units
// ---------------------------------------------------------------------------

struct Machine { uint32_t ctx; uint32_t unit; uint32_t lastDump; };
static const int MACHINE_TABLE = 512;
static Machine g_machines[MACHINE_TABLE];
static volatile LONG g_machineLock = 0;

static Machine* FindMachine(uint32_t ctx, bool add) {
    uint32_t h = ((ctx >> 2) * 2654435761u) & (MACHINE_TABLE - 1);
    for (int i = 0; i < 32; i++) {
        Machine& m = g_machines[(h + i) & (MACHINE_TABLE - 1)];
        if (m.ctx == ctx) return &m;
        if (m.ctx == 0) {
            if (!add) return nullptr;
            while (InterlockedCompareExchange(&g_machineLock, 1, 0) != 0) {}
            if (m.ctx == 0) InterlockedExchange((volatile LONG*)&m.ctx, (LONG)ctx);
            InterlockedExchange(&g_machineLock, 0);
            if (m.ctx == ctx) return &m;
        }
    }
    return nullptr;
}

struct UnitRec {
    uint32_t unit;
    uint32_t lastPos;          // tick of the last position sample
    uint32_t lastReload;       // tick of the last reload sample
    uint8_t watched;           // a machine of this unit showed a shooting state
    uint8_t described;
    float reload[MAX_MEN];     // last reload time per soldier slot
    uint8_t counting[MAX_MEN]; // the reload since the last shot has started counting down
    uint32_t who[MAX_MEN];     // the soldier last seen in the slot (casualties shift the array)
};
static const int UNIT_TABLE = 128;
static UnitRec* g_units = nullptr;   // VirtualAlloc'd
static volatile LONG g_unitLock = 0;

static UnitRec* FindUnit(uint32_t unit, bool add) {
    if (!g_units || !unit) return nullptr;
    uint32_t h = ((unit >> 2) * 2654435761u) & (UNIT_TABLE - 1);
    for (int i = 0; i < UNIT_TABLE; i++) {
        UnitRec& u = g_units[(h + i) & (UNIT_TABLE - 1)];
        if (u.unit == unit) return &u;
        if (u.unit == 0) {
            if (!add) return nullptr;
            while (InterlockedCompareExchange(&g_unitLock, 1, 0) != 0) {}
            if (u.unit == 0) {
                for (uint32_t k = 0; k < MAX_MEN; k++) u.reload[k] = -1.0f;
                u.lastPos = u.lastReload = 0xFFFFFFFF;
                InterlockedExchange((volatile LONG*)&u.unit, (LONG)unit);
            }
            InterlockedExchange(&g_unitLock, 0);
            if (u.unit == unit) return &u;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Probe output
// ---------------------------------------------------------------------------

static CRITICAL_SECTION g_cs;
static bool g_probe = false;
static char g_dir[MAX_PATH] = "";
static FILE* g_fStates = nullptr;
static FILE* g_fRows = nullptr;
static FILE* g_fReload = nullptr;
static FILE* g_fOrders = nullptr;
static FILE* g_fUnits = nullptr;
static FILE* g_fInfo = nullptr;
static FILE* g_fRaw = nullptr;
static volatile LONG g_rows = 0;
static const LONG MAX_ROWS = 6000000;
static DWORD g_t0 = 0, g_lastFlush = 0;
static volatile LONG g_cTransitions = 0, g_cLogged = 0, g_cOrders = 0, g_cShots = 0, g_cLoaded = 0, g_cDumps = 0;

static bool EnvIs1(const char* name) {
    char v[8] = "";
    return GetEnvironmentVariableA(name, v, sizeof(v)) > 0 && v[0] == '1';
}

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
    snprintf(root, sizeof(root), "%sfbrfix_runs\\", exe);
    CreateDirectoryA(root, nullptr);
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_dir, sizeof(g_dir), "%s%04d%02d%02d_%02d%02d%02d\\", root, st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute, st.wSecond);
    if (!CreateDirectoryA(g_dir, nullptr)) { Log("fbrfix: cannot create %s", g_dir); return false; }
    g_fStates = OpenCsv("fbr_states.csv", "ms,tick,thread,ctx,owner_vt,unit,old_state,old,new_state,new,row,rows,files\n");
    g_fRows = OpenCsv("fbr_rowdump.csv", "ms,tick,why,task,unit,row,rows,i,soldier,vt,index,trow,tcol,reload,ordered,done,"
                                         "action,action94,state7c,x,z\n");
    g_fReload = OpenCsv("fbr_reload.csv", "ms,tick,unit,i,soldier,event,reload\n");
    g_fOrders = OpenCsv("fbr_orders.csv", "ms,tick,kind,ret_rva,soldier,unit,index,w0,w1,w2,w3,w4,w5,w6,w7,w8,w9,"
                                          "w10,w11,target,target_unit\n");
    g_fUnits = OpenCsv("fbr_units.csv", "ms,tick,unit,count,i,soldier,x,y,z\n");
    g_fInfo = OpenCsv("fbr_unitinfo.csv", "ms,tick,unit,count,record,soldier0_vt,strings\n");
    g_fRaw = OpenCsv("fbr_soldier_raw.csv", "ms,tick,task,unit,i,soldier,block,off,value\n");
    if (!g_fStates || !g_fRows || !g_fReload || !g_fOrders || !g_fUnits || !g_fInfo) {
        Log("fbrfix: cannot open the probe files in %s", g_dir);
        return false;
    }
    Log("fbrfix: probe on, writing %s", g_dir);
    return true;
}

static void MaybeFlush() {
    DWORD now = GetTickCount();
    if (now - g_lastFlush < 2000) return;
    g_lastFlush = now;
    fflush(g_fStates); fflush(g_fRows); fflush(g_fReload); fflush(g_fOrders); fflush(g_fUnits); fflush(g_fInfo);
}

static bool RowBudget(LONG n) { return InterlockedExchangeAdd(&g_rows, n) + n < MAX_ROWS; }

// Key-like strings (6..80 chars of [A-Za-z0-9_]) pointed at by the first 0x800 bytes of the unit
// record, and one level further through the first 0x80 bytes of the blocks the record's first
// 0x100 bytes point to. Strings are copied with CopyGuarded (every page checked first), so a
// stray pointer never touches a guard page.
static int KeyStrings(uint32_t rec, char* out, int cap) {
    int len = 0, found = 0;
    out[0] = 0;
    if (!PtrPlausible(rec)) return 0;
    for (int depth = 0; depth < 2 && found < 8; depth++) {
        uint32_t span = depth == 0 ? 0x800 : 0x80;
        uint32_t bases[64];
        int nb = 0;
        if (depth == 0) bases[nb++] = rec;
        else {
            for (uint32_t k = 0; k < 0x100 && nb < 64; k += 4) {
                uint32_t p;
                if (Rd32(rec + k, &p) && PtrPlausible(p)) bases[nb++] = p;
            }
        }
        for (int b = 0; b < nb && found < 8; b++) {
            if (!RangeReadable(bases[b], span)) continue;
            for (uint32_t k = 0; k < span && found < 8; k += 4) {
                uint32_t p;
                char s[84];
                if (!Rd32(bases[b] + k, &p) || !PtrInUser(p) || CopyGuarded(p, (uint8_t*)s, sizeof(s) - 1) == 0)
                    continue;
                s[sizeof(s) - 1] = 0;
                int n = 0;
                while (n < 80 && ((s[n] >= 'A' && s[n] <= 'Z') || (s[n] >= 'a' && s[n] <= 'z') ||
                                  (s[n] >= '0' && s[n] <= '9') || s[n] == '_')) n++;
                if (n < 6 || s[n] != 0) continue;
                bool hasUnder = false, hasUpper = false;
                for (int q = 0; q < n; q++) { hasUnder |= s[q] == '_'; hasUpper |= s[q] >= 'A' && s[q] <= 'Z'; }
                if (!hasUnder || !hasUpper) continue;
                len += snprintf(out + len, cap - len, "%s%d:%X:%s", found ? " " : "", depth, k, s);
                found++;
            }
        }
    }
    return found;
}

static void DescribeUnit(UnitRec* u, uint32_t count, uint32_t arr, DWORD ms, uint32_t tick) {
    if (u->described) return;
    u->described = 1;
    uint32_t rec = 0, s0 = 0, vt = 0;
    Rd32(u->unit + UNIT_OFF_RECORD, &rec);
    Rd32(arr, &s0);
    Rd32(s0, &vt);
    char strs[1024];
    KeyStrings(rec, strs, sizeof(strs));
    EnterCriticalSection(&g_cs);
    fprintf(g_fInfo, "%lu,%u,%08X,%u,%08X,%06X,%s\n", ms, tick, u->unit, count, rec, vt ? vt - g_base : 0, strs);
    LeaveCriticalSection(&g_cs);
}

static void SamplePositions(UnitRec* u, uint32_t count, uint32_t arr, DWORD ms, uint32_t tick) {
    if (u->lastPos != 0xFFFFFFFF && tick - u->lastPos < 10) return;   // once per game second
    u->lastPos = tick;
    if (!RowBudget((LONG)count)) return;
    EnterCriticalSection(&g_cs);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t s;
        Vec3 p;
        if (!Rd32(arr + 4 * i, &s) || !MemcpyTry(&p, s + 0x48, 12)) continue;
        fprintf(g_fUnits, "%lu,%u,%08X,%u,%u,%08X,%.2f,%.2f,%.2f\n", ms, tick, u->unit, count, i, s, p.x, p.y, p.z);
    }
    LeaveCriticalSection(&g_cs);
}

// Every tick for watched units: a soldier's reload time leaving 0 = he fired; first going down
// after that = his reload has started (the timer only runs under a reload order); reaching 0 =
// loaded.
static void SampleReload(UnitRec* u, uint32_t count, uint32_t arr, DWORD ms, uint32_t tick) {
    if (u->lastReload == tick) return;
    u->lastReload = tick;
    uint32_t n = count < MAX_MEN ? count : MAX_MEN;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t s;
        float r;
        if (!Rd32(arr + 4 * i, &s) || !MemcpyTry(&r, s + SOL_OFF_RELOAD, 4)) continue;
        float prev = u->reload[i];
        u->reload[i] = r;
        if (u->who[i] != s) { u->who[i] = s; u->counting[i] = 1; continue; }   // first sample of this man here
        if (prev < -0.5f) continue;   // first sample
        int ev = 0;
        if (prev <= 0.0f && r > 0.0f) ev = 1;        // fired: reload time set
        else if (prev > 0.0f && r <= 0.0f) ev = 2;   // loaded
        else if (r > prev + 0.001f) ev = 3;          // reload time went up without reaching 0 first
        else if (r < prev - 0.001f && !u->counting[i]) ev = 4;   // reload started counting down
        if (ev == 1 || ev == 3) u->counting[i] = 0;
        if (ev == 4) u->counting[i] = 1;
        if (!ev || !RowBudget(1)) continue;
        if (ev == 1) InterlockedIncrement(&g_cShots);
        if (ev == 2) InterlockedIncrement(&g_cLoaded);
        static const char* const kEvent[] = { "", "fired", "loaded", "up", "counting" };
        EnterCriticalSection(&g_cs);
        fprintf(g_fReload, "%lu,%u,%08X,%u,%08X,%s,%.3f\n", ms, tick, u->unit, i, s, kEvent[ev], r);
        LeaveCriticalSection(&g_cs);
    }
}

// All soldiers of a fire-by-rank task's unit with their row (row task) or group (group task)
// and their ready bytes. For the group task trow = group, tcol = -1, row = current group,
// rows = group count.
static void DumpRows(uint32_t task, uint32_t unit, const char* why, DWORD ms, uint32_t tick) {
    uint32_t count, arr, row = 0, rows = 0, table = 0, tableN = 0, vt = 0;
    if (!IsUnit(unit, &count, &arr)) return;
    Rd32(task, &vt);
    bool group = vt == g_base + RVA_GROUP_TASK_VT;
    if (group) {
        Rd32(task + GRP_OFF_CUR, &row);
        Rd32(task + GRP_OFF_COUNT, &rows);
        Rd32(task + GRP_OFF_TABLE, &table);
        tableN = MAX_MEN;
    } else {
        Rd32(task + VOL_OFF_ROW, &row);
        Rd32(task + VOL_OFF_ROWS, &rows);
        Rd32(task + VOL_OFF_TABLE, &table);
        Rd32(task + VOL_OFF_TABLE_N, &tableN);
    }
    if (!RowBudget((LONG)count)) return;
    InterlockedIncrement(&g_cDumps);
    EnterCriticalSection(&g_cs);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t s = 0, vt = 0, idx = 0xFFFFFFFF, act = 0, st7c = 0;
        int32_t trow = -9, tcol = -9;
        float rl = 0;
        uint8_t ord = 0xFF, done = 0xFF, a94 = 0xFF;
        Vec3 p = {0, 0, 0};
        if (!Rd32(arr + 4 * i, &s)) continue;
        Rd32(s, &vt);
        Rd32(s + SOL_OFF_INDEX, &idx);
        if (table && idx < tableN) {
            if (group) { MemcpyTry(&trow, table + idx * 4, 4); tcol = -1; }
            else { MemcpyTry(&trow, table + idx * 8, 4); MemcpyTry(&tcol, table + idx * 8 + 4, 4); }
        }
        MemcpyTry(&rl, s + SOL_OFF_RELOAD, 4);
        MemcpyTry(&ord, s + SOL_OFF_ORDERED, 1);
        MemcpyTry(&done, s + SOL_OFF_DONE, 1);
        Rd32(s + SOL_OFF_ACTION, &act);
        if (PtrPlausible(act)) MemcpyTry(&a94, act + 0x94, 1);
        Rd32(s + SOL_OFF_STATE7C, &st7c);
        MemcpyTry(&p, s + 0x48, 12);
        fprintf(g_fRows, "%lu,%u,%s,%08X,%08X,%u,%u,%u,%08X,%06X,%d,%d,%d,%.3f,%u,%u,%08X,%u,%u,%.2f,%.2f\n", ms, tick,
                why, task, unit, row, rows, i, s, vt ? vt - g_base : 0, (int)idx, trow, tcol, rl, ord, done, act, a94,
                st7c, p.x, p.z);
    }
    LeaveCriticalSection(&g_cs);
}

// Once per group task: the raw first 0x800 bytes of soldier 0 (the officer in every unit seen),
// soldier 1 and soldier 2, and 0x100 bytes of the block each one's +0x20C points to (the fire
// orders read [[s+0x20C]+0x60]). For an offline diff of a gun officer against a sword officer.
static void DumpRaw(uint32_t task, uint32_t unit, DWORD ms, uint32_t tick) {
    static uint32_t s_done[32];
    static int s_n = 0;
    for (int i = 0; i < s_n; i++) if (s_done[i] == task) return;
    if (s_n >= 32 || !g_fRaw) return;
    s_done[s_n++] = task;
    uint32_t count, arr;
    if (!IsUnit(unit, &count, &arr)) return;
    EnterCriticalSection(&g_cs);
    for (uint32_t i = 0; i < 3 && i < count; i++) {
        uint32_t s = 0, blk = 0;
        if (!Rd32(arr + 4 * i, &s)) continue;
        Rd32(s + 0x20C, &blk);
        for (uint32_t k = 0; k < 0x800; k += 4) {
            uint32_t v = 0;
            Rd32(s + k, &v);
            fprintf(g_fRaw, "%lu,%u,%08X,%08X,%u,%08X,soldier,%04X,%08X\n", ms, tick, task, unit, i, s, k, v);
        }
        if (PtrPlausible(blk) && RangeReadable(blk, 0x100)) {
            for (uint32_t k = 0; k < 0x100; k += 4) {
                uint32_t v = 0;
                Rd32(blk + k, &v);
                fprintf(g_fRaw, "%lu,%u,%08X,%08X,%u,%08X,b20c,%04X,%08X\n", ms, tick, task, unit, i, s, k, v);
            }
        }
    }
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

// The row task (FotS-style rank fire) or the group task (Shogun 2 fire by rank).
static bool IsVolleyTask(uint32_t task) {
    uint32_t vt;
    return PtrPlausible(task) && Rd32(task, &vt) &&
           (vt == g_base + RVA_VOLLEY_TASK_VT || vt == g_base + RVA_GROUP_TASK_VT);
}

static bool IsGroupTask(uint32_t task) {
    uint32_t vt;
    return PtrPlausible(task) && Rd32(task, &vt) && vt == g_base + RVA_GROUP_TASK_VT;
}

// Runner entry: every tick of every machine. Units of watched machines are sampled.
static void HandleRunner(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t mctx = *(const uint32_t*)(ctx + FBR_OFF_ECX);
    Machine* m = FindMachine(mctx, false);
    DWORD ms = GetTickCount() - g_t0;
    uint32_t tick = Tick();
    uint32_t count, arr;
    if (!m) {
        // positions of every unit, once per game second, from whatever task of it ticks here
        uint32_t unit;
        if (Rd32(mctx - TASK_CTX_OFF + TASK_OFF_UNIT, &unit) && IsUnit(unit, &count, &arr)) {
            UnitRec* u = FindUnit(unit, true);
            if (u) { DescribeUnit(u, count, arr, ms, tick); SamplePositions(u, count, arr, ms, tick); }
        }
        return;
    }
    if (!m->unit || !IsUnit(m->unit, &count, &arr)) return;
    UnitRec* u = FindUnit(m->unit, true);
    if (!u) return;
    DescribeUnit(u, count, arr, ms, tick);
    SamplePositions(u, count, arr, ms, tick);
    SampleReload(u, count, arr, ms, tick);
    uint32_t task = mctx - TASK_CTX_OFF, cur = 0;
    if (IsVolleyTask(task) && Rd32(mctx + CTX_OFF_STATE, &cur)) {
        const StateInfo* s = State(cur);
        if (s && strncmp(s->name, "WAIT_FOR", 8) == 0 && (m->lastDump == 0 || tick - m->lastDump >= 10)) {
            m->lastDump = tick;
            DumpRows(task, m->unit, "waiting", ms, tick);
        }
    }
    MaybeFlush();
}

static void HandleTransition(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cTransitions);
    uint32_t mctx = *(const uint32_t*)(ctx + FBR_OFF_ESI);
    uint32_t next = *(const uint32_t*)(ctx + FBR_OFF_EDI);
    uint32_t old = 0;
    Rd32(mctx + CTX_OFF_STATE, &old);
    const StateInfo* so = State(old);
    const StateInfo* sn = State(next);
    Machine* m = FindMachine(mctx, false);
    if (!m) {
        if (!((so && so->interest) || (sn && sn->interest))) return;
        m = FindMachine(mctx, true);
        if (!m) return;
        uint32_t unit, count, arr;
        if (Rd32(mctx - TASK_CTX_OFF + TASK_OFF_UNIT, &unit) && IsUnit(unit, &count, &arr)) {
            m->unit = unit;
            UnitRec* u = FindUnit(unit, true);
            if (u) u->watched = 1;
        }
    }
    if (!RowBudget(1)) return;
    InterlockedIncrement(&g_cLogged);
    uint32_t task = mctx - TASK_CTX_OFF, ovt = 0, row = 0, rows = 0, files = 0;
    Rd32(task, &ovt);
    bool volley = IsVolleyTask(task);
    if (IsGroupTask(task)) { Rd32(task + GRP_OFF_CUR, &row); Rd32(task + GRP_OFF_COUNT, &rows); files = 0; }
    else if (volley) { Rd32(task + VOL_OFF_ROW, &row); Rd32(task + VOL_OFF_ROWS, &rows); Rd32(task + VOL_OFF_FILES, &files); }
    DWORD ms = GetTickCount() - g_t0;
    uint32_t tick = Tick();
    EnterCriticalSection(&g_cs);
    fprintf(g_fStates, "%lu,%u,%lu,%08X,%06X,%08X,%08X,%s,%08X,%s,%d,%d,%d\n", ms, tick, GetCurrentThreadId(), mctx,
            ovt ? ovt - g_base : 0, m->unit, old, so ? so->name : "", next, sn ? sn->name : "",
            volley ? (int)row : -1, volley ? (int)rows : -1, volley ? (int)files : -1);
    LeaveCriticalSection(&g_cs);
    if (volley && sn && (strstr(sn->name, "ROW") || strstr(sn->name, "GROUP") || strstr(sn->name, "START"))) {
        m->lastDump = tick;
        DumpRows(task, m->unit, sn->name, ms, tick);
        if (IsGroupTask(task)) DumpRaw(task, m->unit, ms, tick);
    }
}

static void HandleOrder(const uint8_t* ctx, char kind) {
    if (!g_probe) return;
    uint32_t s = *(const uint32_t*)(ctx + FBR_OFF_ECX);
    uint32_t order = *(const uint32_t*)(ctx + FBR_OFF_ARG1);
    uint32_t ret = *(const uint32_t*)(ctx + FBR_OFF_RET);
    uint32_t unit = 0, idx = 0;
    if (!Rd32(s + SOL_OFF_UNIT, &unit)) return;
    UnitRec* u = FindUnit(unit, false);
    if (!u || !u->watched) return;
    if (!RowBudget(1)) return;
    InterlockedIncrement(&g_cOrders);
    Rd32(s + SOL_OFF_INDEX, &idx);
    // order A is 0x50 bytes (the reload order of the wait state has its type, 5, at +0x20), B is 8
    uint32_t w[12] = {};
    MemcpyTry(w, order, kind == 'A' ? 48 : 8);
    // fire orders from the row-fire state carry a target record at +4 (offset xyz, entity, ...)
    uint32_t target = kind == 'A' ? w[4] : 0, tunit = 0;
    if (PtrPlausible(target)) Rd32(target + SOL_OFF_UNIT, &tunit);
    else target = 0;
    DWORD ms = GetTickCount() - g_t0;
    EnterCriticalSection(&g_cs);
    if (g_fOrders)
        fprintf(g_fOrders, "%lu,%u,%c,%06X,%08X,%08X,%d,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,"
                "%08X,%08X\n", ms, Tick(), kind, ret - g_base, s, unit, (int)idx, w[0], w[1], w[2], w[3], w[4], w[5],
                w[6], w[7], w[8], w[9], w[10], w[11], target, tunit);
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// The fix
// ---------------------------------------------------------------------------

static const uint32_t RVA_READY_TEST = 0x3CBE40;     // FUN_103cbe40: a soldier's order is done (ready)
static const uint32_t SOL_OFF_CLASS = 0x48C;         // 8 for every gun man seen, 0x10 for the sword officer
static const uint32_t GRACE_DEFAULT = 15;            // game ticks (0.1 s each)
typedef uint32_t (__fastcall *ReadyFn)(uint32_t soldier);

static volatile LONG g_fixOn = 0;
static volatile LONG g_fixDisabled = 0;              // self-check switched it off; Y refused
static uint32_t g_grace = GRACE_DEFAULT;
static ReadyFn g_ready = nullptr;
static volatile LONG g_fReadyAll = 0;     // group test answered "ready": every shooter ready
static volatile LONG g_fReadyGrace = 0;   // ... loaded, some not ready yet, grace time over
static volatile LONG g_fNotLoaded = 0;    // ... "not ready": a shooter still reloading
static volatile LONG g_fWaiting = 0;      // ... "not ready": within the grace time
static volatile LONG g_fExcludedMen = 0;  // men left out of a group test (not of the shooter class)
static volatile LONG g_fOrderSkipped = 0; // ready / reload orders not given to such men
static volatile LONG g_fOrderGiven = 0;
static volatile LONG g_fBadTask = 0;      // group task fields implausible (self-check)
static FILE* g_fFix = nullptr;            // probe only: fbr_fix.csv
static bool g_allRanks = false;           // FBRFIX_ALL_RANKS=1: every rank fires in turn (default: 3 groups, vanilla)
static const uint32_t MAX_GROUPS = 8;
static volatile LONG g_fAllRanks = 0;     // group tasks whose count was raised
// Early reload (FUN_1017b9d0 hook)
static const uint32_t RVA_GROUP_ORDER = 0x188290;   // FUN_10188290, the task's slot +0x84
static const uint32_t GRP_VT_SLOT_ORDER = 0x84;
typedef void (__fastcall *GroupOrderFn)(uint32_t task, uint32_t edx, uint32_t soldier, uint32_t afterFire);
static bool g_earlyReload = true;         // FBRFIX_EARLY_RELOAD=0 turns it off
static uint32_t g_fireGrace = 0;          // FBRFIX_FIRE_GRACE=<ticks>: end the turn this long after the last shot (0 = off)
static volatile LONG g_fEarly = 0;        // early reload orders given
static volatile LONG g_fDone = 0;         // fire-completed test answered "done"
static volatile LONG g_fFiring = 0;       // ... answered "not done": a shooter still in his fire order
static volatile LONG g_fCut = 0;          // turns ended by the fire grace
static volatile LONG g_fNotFired = 0;     // shooters who had not fired when their turn was cut
static volatile LONG g_fExitSkipped = 0;  // second reload orders not given to men already reloading

// The unit's shooter class: the most common soldier +0x48C value among its men.
static bool ShooterClass(uint32_t unit, uint32_t* cls) {
    uint32_t count, arr;
    if (!IsUnit(unit, &count, &arr)) return false;
    uint32_t vals[8] = {}, hits[8] = {};
    int nv = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t s, c;
        if (!Rd32(arr + 4 * i, &s) || !Rd32(s + SOL_OFF_CLASS, &c)) continue;
        int k = 0;
        while (k < nv && vals[k] != c) k++;
        if (k == nv) { if (nv == 8) continue; vals[nv++] = c; }
        hits[k]++;
    }
    if (!nv) return false;
    int best = 0;
    for (int k = 1; k < nv; k++) if (hits[k] > hits[best]) best = k;
    *cls = vals[best];
    return true;
}

// How long the task has been asking about its current group: consecutive calls (one per tick
// while the wait state runs) with the same group number.
struct WaitRec { uint32_t task, group, first, last; };
static WaitRec g_waits[64];
static volatile LONG g_waitLock = 0;

static uint32_t WaitTicks(uint32_t task, uint32_t group, uint32_t tick) {
    while (InterlockedCompareExchange(&g_waitLock, 1, 0) != 0) {}
    WaitRec* w = nullptr;
    WaitRec* oldest = &g_waits[0];
    for (int i = 0; i < 64; i++) {
        if (g_waits[i].task == task) { w = &g_waits[i]; break; }
        if (g_waits[i].last < oldest->last) oldest = &g_waits[i];
    }
    if (!w) { w = oldest; w->task = task; w->group = group + 1; }
    if (w->group != group || tick < w->last || tick - w->last > 3) { w->group = group; w->first = tick; }
    w->last = tick;
    uint32_t waited = tick - w->first;
    InterlockedExchange(&g_waitLock, 0);
    return waited;
}

static void FixRow(uint32_t task, uint32_t unit, uint32_t group, uint32_t men, uint32_t excluded, uint32_t notLoaded,
                   uint32_t notReady, uint32_t waited, const char* answer) {
    if (!g_fFix || !RowBudget(1)) return;
    EnterCriticalSection(&g_cs);
    if (g_fFix)
        fprintf(g_fFix, "%lu,%u,%08X,%08X,%u,%u,%u,%u,%u,%u,%s\n", GetTickCount() - g_t0, Tick(), task, unit, group,
                men, excluded, notLoaded, notReady, waited, answer);
    LeaveCriticalSection(&g_cs);
}

// Self-check: a group task whose fields do not look like one. 20 in a row switch the fix off.
static void BadTask() {
    LONG n = InterlockedIncrement(&g_fBadTask);
    if (n >= 20 && g_fReadyAll + g_fReadyGrace + g_fNotLoaded + g_fWaiting == 0 &&
        !InterlockedExchange(&g_fixDisabled, 1)) {
        InterlockedExchange(&g_fixOn, 0);
        Log("fbrfix: DISABLED for this session, the first %ld group tasks did not have the decoded layout", n);
        MessageBeep(MB_ICONHAND);
    }
}

// FUN_1017b980 entry (ECX = group task): answer "current group loaded and ready" ourselves.
static uint32_t HandleGroupReady(const uint8_t* ctx) {
    if (!g_fixOn) return HOOK_CONTINUE;
    uint32_t task = *(const uint32_t*)(ctx + FBR_OFF_ECX);
    uint32_t unit = 0, group = 0, groups = 0, table = 0, count, arr, cls;
    if (!Rd32(task + TASK_OFF_UNIT, &unit) || !IsUnit(unit, &count, &arr) || !Rd32(task + GRP_OFF_CUR, &group) ||
        !Rd32(task + GRP_OFF_COUNT, &groups) || !Rd32(task + GRP_OFF_TABLE, &table) || !PtrPlausible(table) ||
        groups == 0 || groups > 8 || group >= groups || !ShooterClass(unit, &cls)) {
        BadTask();
        return HOOK_CONTINUE;
    }
    // Option FBRFIX_ALL_RANKS=1: the task caps its group count at 3 (constructor FUN_101295d0) while
    // the formation numbers its ranks 0..n-1, so ranks 4+ never fire. Before the first volley
    // (group 0) raise the count to the number of ranks present (at most MAX_GROUPS).
    if (g_allRanks && group == 0 && groups == 3) {
        uint32_t top = 0;
        for (uint32_t i = 0; i < count; i++) {
            uint32_t s, idx, g;
            if (Rd32(arr + 4 * i, &s) && Rd32(s + SOL_OFF_INDEX, &idx) && idx < MAX_MEN && Rd32(table + 4 * idx, &g) &&
                g < MAX_GROUPS && g > top)
                top = g;
        }
        if (top + 1 > groups && WriteDwordTry(task + GRP_OFF_COUNT, top + 1)) {
            InterlockedIncrement(&g_fAllRanks);
            if (g_fFix) FixRow(task, unit, 0, top + 1, 0, 0, 0, 0, "all_ranks");
            groups = top + 1;
        }
    }
    uint32_t tick = Tick();
    uint32_t waited = WaitTicks(task, group, tick);
    uint32_t men = 0, excluded = 0, notLoaded = 0, notReady = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t s, idx, g, c;
        float reload;
        if (!Rd32(arr + 4 * i, &s) || !Rd32(s + SOL_OFF_INDEX, &idx) || idx >= MAX_MEN || !Rd32(table + 4 * idx, &g))
            return HOOK_CONTINUE;
        if (g != group) continue;
        if (!Rd32(s + SOL_OFF_CLASS, &c) || !MemcpyTry(&reload, s + SOL_OFF_RELOAD, 4)) return HOOK_CONTINUE;
        if (c != cls) { excluded++; continue; }
        men++;
        if (reload > 0.0f) notLoaded++;
        else if ((g_ready(s) & 0xFF) == 0) notReady++;
    }
    if (men == 0) return HOOK_CONTINUE;   // nothing to decide: the game's own test
    if (excluded) InterlockedExchangeAdd(&g_fExcludedMen, (LONG)excluded);
    const char* answer;
    uint32_t result;
    if (notLoaded) { result = 0; answer = "reloading"; InterlockedIncrement(&g_fNotLoaded); }
    else if (!notReady) { result = 1; answer = "ready"; InterlockedIncrement(&g_fReadyAll); }
    else if (waited >= g_grace) { result = 1; answer = "grace"; InterlockedIncrement(&g_fReadyGrace); }
    else { result = 0; answer = "waiting"; InterlockedIncrement(&g_fWaiting); }
    if (g_fFix && (result || waited % 10 == 0))
        FixRow(task, unit, group, men, excluded, notLoaded, notReady, waited, answer);
    return HOOK_RETURN(result);
}

// A group's turn in WAIT_FOR_GROUP_FIRE_COMPLETED: consecutive calls (one per tick) for the same
// task and group. key = first tick and group, so a man's early order belongs to one turn only.
struct TurnRec { uint32_t task, group, first, last, lastShot, shots; };
static TurnRec g_turns[64];
static volatile LONG g_turnLock = 0;

static void TurnOf(uint32_t task, uint32_t group, uint32_t tick, uint32_t fired, uint32_t* key, uint32_t* sinceShot) {
    while (InterlockedCompareExchange(&g_turnLock, 1, 0) != 0) {}
    TurnRec* t = nullptr;
    TurnRec* oldest = &g_turns[0];
    for (int i = 0; i < 64; i++) {
        if (g_turns[i].task == task) { t = &g_turns[i]; break; }
        if (g_turns[i].last < oldest->last) oldest = &g_turns[i];
    }
    if (!t) { t = oldest; t->task = task; t->group = group + 1; }
    if (t->group != group || tick < t->last || tick - t->last > 3) {
        t->group = group; t->first = t->lastShot = tick; t->shots = 0;
    }
    t->last = tick;
    if (fired > t->shots) { t->shots = fired; t->lastShot = tick; }
    *key = (t->first << 4) | (group & 0xF);
    *sinceShot = t->shots ? tick - t->lastShot : 0;
    InterlockedExchange(&g_turnLock, 0);
}

// The task's current turn key, or 0 (no turn seen for it in the last ticks).
static uint32_t CurrentTurn(uint32_t task, uint32_t tick) {
    uint32_t key = 0;
    while (InterlockedCompareExchange(&g_turnLock, 1, 0) != 0) {}
    for (int i = 0; i < 64; i++)
        if (g_turns[i].task == task && tick >= g_turns[i].last && tick - g_turns[i].last <= 3) {
            key = (g_turns[i].first << 4) | (g_turns[i].group & 0xF);
            break;
        }
    InterlockedExchange(&g_turnLock, 0);
    return key;
}

// Soldiers who had their early reload order: soldier -> (task, turn key).
struct EarlyRec { uint32_t soldier, task, key; };
static const int EARLY_TABLE = 4096;   // power of two
static EarlyRec g_early[EARLY_TABLE];
static volatile LONG g_earlyLock = 0;

static EarlyRec* EarlyOf(uint32_t s, bool add) {
    uint32_t h = ((s >> 2) * 2654435761u) & (EARLY_TABLE - 1);
    EarlyRec* r = nullptr;
    while (InterlockedCompareExchange(&g_earlyLock, 1, 0) != 0) {}
    for (int i = 0; i < 64; i++) {
        EarlyRec& e = g_early[(h + i) & (EARLY_TABLE - 1)];
        if (e.soldier == s) { r = &e; break; }
        if (e.soldier == 0) { if (add) { e.soldier = s; r = &e; } break; }
    }
    if (!r && add) r = &g_early[h];   // table crowded: reuse the home slot
    if (r && add && r->soldier != s) { r->soldier = s; r->task = 0; r->key = 0; }
    InterlockedExchange(&g_earlyLock, 0);
    return r;
}

static bool HadEarly(uint32_t s, uint32_t task, uint32_t key) {
    EarlyRec* e = EarlyOf(s, false);
    return e && e->task == task && e->key == key;
}

// FUN_1017b9d0 entry (ECX = group task): "has every man of the current group finished his fire
// order". The game's answer holds the men who fired until the last man is done, and their reload
// time only runs once the exit FUN_101be8f0 gives them their reload order. Here each shooter who
// has fired (reload time above 0: the wait before the fire order needed everyone loaded) and is
// done with his fire order gets that order at once; he then counts as done although his new
// order is open. Shooters who have not fired count as done when their fire order is.
static uint32_t HandleGroupDone(const uint8_t* ctx) {
    if (!g_fixOn || (!g_earlyReload && !g_fireGrace)) return HOOK_CONTINUE;
    uint32_t task = *(const uint32_t*)(ctx + FBR_OFF_ECX);
    uint32_t unit = 0, group = 0, groups = 0, table = 0, count, arr, cls, vt = 0, slot = 0;
    if (!Rd32(task, &vt) || vt != g_base + RVA_GROUP_TASK_VT || !Rd32(vt + GRP_VT_SLOT_ORDER, &slot) ||
        slot != g_base + RVA_GROUP_ORDER || !Rd32(task + TASK_OFF_UNIT, &unit) || !IsUnit(unit, &count, &arr) ||
        !Rd32(task + GRP_OFF_CUR, &group) || !Rd32(task + GRP_OFF_COUNT, &groups) ||
        !Rd32(task + GRP_OFF_TABLE, &table) || !PtrPlausible(table) || groups == 0 || groups > MAX_GROUPS ||
        group >= groups || !ShooterClass(unit, &cls)) {
        BadTask();
        return HOOK_CONTINUE;
    }
    uint32_t tick = Tick();
    uint32_t men = 0, excluded = 0, fired = 0, firing = 0, notFired = 0;
    uint32_t todo[MAX_MEN];
    uint32_t nTodo = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t s, idx, g, c;
        float reload;
        if (!Rd32(arr + 4 * i, &s) || !Rd32(s + SOL_OFF_INDEX, &idx) || idx >= MAX_MEN || !Rd32(table + 4 * idx, &g))
            return HOOK_CONTINUE;
        if (g != group) continue;
        if (!Rd32(s + SOL_OFF_CLASS, &c) || !MemcpyTry(&reload, s + SOL_OFF_RELOAD, 4)) return HOOK_CONTINUE;
        if (c != cls) { excluded++; continue; }
        men++;
        if (reload > 0.0f) {
            fired++;
            if (nTodo < MAX_MEN) todo[nTodo++] = s;   // decided below, once the turn key is known
        } else if ((g_ready(s) & 0xFF) == 0) {
            firing++;
            notFired++;
        }
    }
    if (men == 0) return HOOK_CONTINUE;
    uint32_t key, sinceShot;
    TurnOf(task, group, tick, fired, &key, &sinceShot);
    GroupOrderFn order = (GroupOrderFn)(g_base + RVA_GROUP_ORDER);
    for (uint32_t k = 0; k < nTodo; k++) {
        uint32_t s = todo[k];
        if (HadEarly(s, task, key)) continue;
        if ((g_ready(s) & 0xFF) == 0) { firing++; continue; }   // fired, fire order not done yet
        if (!g_earlyReload) continue;                          // done with his fire order: as the game counts it
        order(task, 0, s, 1);                                  // the exit's own call: kneeling reload order
        EarlyRec* e = EarlyOf(s, true);
        if (e) { e->task = task; e->key = key; }
        InterlockedIncrement(&g_fEarly);
        if (g_fFix) FixRow(task, unit, group, men, excluded, fired, firing, tick - (key >> 4), "early");
    }
    const char* answer;
    uint32_t result;
    if (!firing) { result = 1; answer = "done"; InterlockedIncrement(&g_fDone); }
    else if (g_fireGrace && fired && sinceShot >= g_fireGrace) {
        result = 1; answer = "cut";
        InterlockedIncrement(&g_fCut);
        InterlockedExchangeAdd(&g_fNotFired, (LONG)notFired);
    } else { result = 0; answer = "firing"; InterlockedIncrement(&g_fFiring); }
    if (excluded) InterlockedExchangeAdd(&g_fExcludedMen, (LONG)excluded);
    if (g_fFix && (result || (tick - (key >> 4)) % 10 == 0))
        FixRow(task, unit, group, men, excluded, fired, firing, tick - (key >> 4), answer);
    return HOOK_RETURN(result);
}

// FUN_10188290 entry (ECX = group task, arg1 = soldier, arg2 = after-fire flag): no ready /
// reload order for a man outside the unit's shooter class, and no second reload order for a man
// who had his early one this turn and is still reloading (the exit FUN_101be8f0 orders the whole
// group).
static uint32_t HandleGroupOrder(const uint8_t* ctx) {
    if (!g_fixOn) return HOOK_CONTINUE;
    uint32_t task = *(const uint32_t*)(ctx + FBR_OFF_ECX);
    uint32_t s = *(const uint32_t*)(ctx + FBR_OFF_ARG1);
    uint32_t unit = 0, cls, c;
    if (!Rd32(task + TASK_OFF_UNIT, &unit) || !Rd32(s + SOL_OFF_CLASS, &c) || !ShooterClass(unit, &cls))
        return HOOK_CONTINUE;
    if (c == cls) {
        float reload = 0.0f;
        uint32_t key = g_earlyReload ? CurrentTurn(task, Tick()) : 0;
        if (key && MemcpyTry(&reload, s + SOL_OFF_RELOAD, 4) && reload > 0.0f && HadEarly(s, task, key)) {
            InterlockedIncrement(&g_fExitSkipped);
            return HOOK_RETURN(0);
        }
        InterlockedIncrement(&g_fOrderGiven);
        return HOOK_CONTINUE;
    }
    InterlockedIncrement(&g_fOrderSkipped);
    if (g_fFix) FixRow(task, unit, 0xFFFFFFFF, 0, 1, 0, 0, 0, "order_skipped");
    return HOOK_RETURN(0);   // the function's own RET 8
}

// Y toggles the fix (dev build; focus only through proxy/keys). Harness: FbrFixTestKeys bit 0.
static volatile LONG g_testKeys = 0;
static bool g_keyWatched = false;
extern "C" __declspec(dllexport) void __stdcall FbrFixTestKeys(LONG bits) { InterlockedExchange(&g_testKeys, bits); }

static void ToggleKey() {
    if (g_fixDisabled) {
        Log("fbrfix: Y refused, the fix is DISABLED for this session (self-check)");
        MessageBeep(MB_ICONHAND);
        return;
    }
    LONG on = g_fixOn ? 0 : 1;
    InterlockedExchange(&g_fixOn, on);
    Log("fbrfix: Y, fix %s (so far: ready %ld, grace %ld, orders skipped %ld)", on ? "ON" : "OFF", g_fReadyAll,
        g_fReadyGrace, g_fOrderSkipped);
    MessageBeep(on ? MB_ICONASTERISK : MB_OK);
}

// ---------------------------------------------------------------------------
// Detours
// ---------------------------------------------------------------------------

extern "C" void __stdcall FbrHandler0(const uint8_t* ctx) { HandleRunner(ctx); }
extern "C" void __stdcall FbrHandler1(const uint8_t* ctx) { HandleTransition(ctx); }
extern "C" void __stdcall FbrHandler2(const uint8_t* ctx) { HandleOrder(ctx, 'A'); }
extern "C" void __stdcall FbrHandler3(const uint8_t* ctx) { HandleOrder(ctx, 'B'); }
extern "C" uint32_t __stdcall FbrHandler4(const uint8_t* ctx) { return HandleGroupReady(ctx); }
extern "C" uint32_t __stdcall FbrHandler5(const uint8_t* ctx) { return HandleGroupOrder(ctx); }
extern "C" uint32_t __stdcall FbrHandler6(const uint8_t* ctx) { return HandleGroupDone(ctx); }

HOOK_DETOUR(FbrDetour0, FbrHandler0, g_fbrTrampolines, 0)
HOOK_DETOUR(FbrDetour1, FbrHandler1, g_fbrTrampolines, 1)
HOOK_DETOUR(FbrDetour2, FbrHandler2, g_fbrTrampolines, 2)
HOOK_DETOUR(FbrDetour3, FbrHandler3, g_fbrTrampolines, 3)
HOOK_DETOUR_RET(FbrDetour4, FbrHandler4, g_fbrTrampolines, 4, 0)   // FUN_1017b980: __thiscall, no stack args
HOOK_DETOUR_RET(FbrDetour5, FbrHandler5, g_fbrTrampolines, 5, 8)   // FUN_10188290: __thiscall, 2 stack args
HOOK_DETOUR_RET(FbrDetour6, FbrHandler6, g_fbrTrampolines, 6, 0)   // FUN_1017b9d0: __thiscall, no stack args

static const HookDetour g_detours[HOOK_COUNT] = { FbrDetour0, FbrDetour1, FbrDetour2, FbrDetour3, FbrDetour4,
                                                  FbrDetour5, FbrDetour6 };

// Harness only (fixes/fire-by-rank-fix/tests): probe and fix counters.
extern "C" __declspec(dllexport) void __stdcall FbrFixTestCounters(long* out) {
    out[0] = g_probe ? 1 : 0;
    out[1] = g_cTransitions;
    out[2] = g_cLogged;
    out[3] = g_cOrders;
    out[4] = g_cShots;
    out[5] = g_cLoaded;
    out[6] = g_cDumps;
    out[7] = g_fixOn;
    out[8] = g_fReadyAll;
    out[9] = g_fReadyGrace;
    out[10] = g_fNotLoaded;
    out[11] = g_fWaiting;
    out[12] = g_fExcludedMen;
    out[13] = g_fOrderSkipped;
    out[14] = g_fOrderGiven;
    out[15] = g_fBadTask;
    out[16] = g_fAllRanks;
    out[17] = g_fEarly;
    out[18] = g_fDone;
    out[19] = g_fFiring;
    out[20] = g_fCut;
    out[21] = g_fNotFired;
    out[22] = g_fExitSkipped;
}

// ---------------------------------------------------------------------------
// Install / remove
// ---------------------------------------------------------------------------

void FbrFix_Install() {
    Log("fbrfix: fire-by-rank fix (dev build)");
    Mem_Init();
    uint32_t size = 0;
    uint8_t* base = Hook_GameModule(false, &size);
    if (!base) { Log("fbrfix: REFUSED, empire.retail.dll is not loaded"); return; }
    if (size != HOOK_ANALYSED_IMAGE_SIZE) { Log("fbrfix: REFUSED, SizeOfImage 0x%08X is not the analysed build", size); return; }
    g_base = (uint32_t)(uintptr_t)base;
    for (int h = 0; h < HOOK_COUNT; h++) {
        if (!Hook_SiteMatches(base, g_hookDefs[h])) {
            Log("fbrfix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", g_hookDefs[h].rva,
                g_hookDefs[h].func);
            return;
        }
    }
    // the game's ready test, called by the group test: its entry must be the analysed one too
    static const uint8_t readyEntry[] = { 0x80, 0xB9, 0x84, 0x03, 0x00, 0x00, 0x01 };   // CMP byte [ECX+0x384],1
    if (memcmp(base + RVA_READY_TEST, readyEntry, sizeof(readyEntry)) != 0) {
        Log("fbrfix: REFUSED, FUN_103cbe40 differs from the analysed binary");
        return;
    }
    g_ready = (ReadyFn)(base + RVA_READY_TEST);
    InitializeCriticalSection(&g_cs);
    g_t0 = GetTickCount();
    char v[16] = "";
    if (GetEnvironmentVariableA("FBRFIX_GRACE", v, sizeof(v)) > 0) {
        long g = atol(v);
        if (g >= 0 && g <= 600) g_grace = (uint32_t)g;
    }
    g_allRanks = EnvIs1("FBRFIX_ALL_RANKS");
    char e[8] = "";
    g_earlyReload = !(GetEnvironmentVariableA("FBRFIX_EARLY_RELOAD", e, sizeof(e)) > 0 && e[0] == '0');
    if (GetEnvironmentVariableA("FBRFIX_FIRE_GRACE", v, sizeof(v)) > 0) {
        long g = atol(v);
        if (g >= 0 && g <= 600) g_fireGrace = (uint32_t)g;
    }
    bool probe = EnvIs1("FBRFIX_PROBE");
    if (probe) {
        g_units = (UnitRec*)VirtualAlloc(nullptr, sizeof(UnitRec) * UNIT_TABLE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (g_units && OpenProbe()) {
            g_probe = true;
            g_fFix = OpenCsv("fbr_fix.csv", "ms,tick,task,unit,group,men,excluded,not_loaded,not_ready,waited,answer\n");
        } else {
            Log("fbrfix: probe not started (no memory or no run folder)");
        }
    }
    bool off = EnvIs1("FBRFIX_OFF");
    InterlockedExchange(&g_fixOn, off ? 0 : 1);
    bool use[HOOK_COUNT];
    for (int h = 0; h < HOOK_COUNT; h++) use[h] = !g_hookDefs[h].probeOnly || g_probe;
    if (!Hook_Prepare(base, g_hookDefs, HOOK_COUNT, use, g_fbrTrampolines, g_slots)) {
        Log("fbrfix: REFUSED, no trampoline memory");
        g_probe = false;
        return;
    }
    Hook_Patch(g_detours, g_slots, HOOK_COUNT);
    for (int h = 0; h < HOOK_COUNT; h++)
        if (g_slots[h].active)
            Log("fbrfix: hook %s (%s) installed at RVA 0x%06X", g_hookDefs[h].func, g_hookDefs[h].role, g_hookDefs[h].rva);
    Log("fbrfix: fix %s, grace %u ticks (%.1f game s), %s", off ? "OFF (FBRFIX_OFF=1), hooks installed" : "ON", g_grace,
        g_grace / 10.0, g_allRanks ? "every rank fires (FBRFIX_ALL_RANKS=1)" : "3 groups as in the game");
    Log("fbrfix: early reload %s; fire grace %u ticks%s", g_earlyReload ? "ON" : "OFF (FBRFIX_EARLY_RELOAD=0)",
        g_fireGrace, g_fireGrace ? " after the group's last shot" : " (off)");
    g_keyWatched = Keys_Watch('Y', &g_testKeys, 1, ToggleKey);
    Log("fbrfix: Y toggles the fix on / off%s", g_keyWatched ? "" : " (ERROR: key poll not started)");
}

void FbrFix_Remove() {
    if (g_keyWatched) { Keys_Unwatch(ToggleKey); g_keyWatched = false; }
    Hook_Remove(g_slots, HOOK_COUNT);
    if (g_base)
        Log("fbrfix: group tests: ready %ld, grace %ld, reloading %ld, waiting %ld; men left out %ld; ready orders "
            "skipped %ld, given %ld; bad task %ld; group counts raised (all ranks) %ld", g_fReadyAll, g_fReadyGrace,
            g_fNotLoaded, g_fWaiting, g_fExcludedMen, g_fOrderSkipped, g_fOrderGiven, g_fBadTask, g_fAllRanks);
    if (g_base)
        Log("fbrfix: fire-completed tests: done %ld, firing %ld, cut by the fire grace %ld (%ld men not fired); "
            "early reload orders %ld, second reload orders skipped %ld", g_fDone, g_fFiring, g_fCut, g_fNotFired,
            g_fEarly, g_fExitSkipped);
    if (g_probe) {
        EnterCriticalSection(&g_cs);
        FILE* fs[] = { g_fStates, g_fRows, g_fReload, g_fOrders, g_fUnits, g_fInfo, g_fRaw, g_fFix };
        for (FILE* f : fs) if (f) fclose(f);
        g_fStates = g_fRows = g_fReload = g_fOrders = g_fUnits = g_fInfo = g_fRaw = g_fFix = nullptr;
        g_probe = false;
        LeaveCriticalSection(&g_cs);
        Log("fbrfix: probe closed: transitions %ld, logged %ld, orders %ld, shots %ld, loaded %ld, row dumps %ld, rows %ld",
            g_cTransitions, g_cLogged, g_cOrders, g_cShots, g_cLoaded, g_cDumps, g_rows);
    }
}
