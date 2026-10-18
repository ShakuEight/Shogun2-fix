// siegefix.cpp -- siege responsiveness fix for Total War: Shogun 2 (original x86 Empire.Retail.dll).
// Folder fixes/siege-responsiveness-fix/, code name siegefix.
//
// Dev build. Fault: in a large siege, once the attackers climb the walls, the defender's units
// answer move orders late (a unit on the wall takes long to leave it, a unit sent onto the wall
// long to get there). Engine decode (docs/re/path_queue_x86.md):
//
//   - An entity's move runs the movement state machine FUN_103df3f0. State 1 builds a path
//     request and calls the query FUN_10385fd0. A request whose mode (request +0x30) is 0 is
//     searched at once; any other mode puts it in the pathfinder's request queue and the entity
//     waits in state 2 until the queue has served it.
//   - The pathfinder's per-tick update FUN_103aac30 serves the queue in order, and stops for the
//     tick once its A* node counter (pathfinder +0x470, which also counts the immediate searches
//     made since the last tick) reaches 500. At least one request is served per tick.
//   - With thousands of attackers on ladders and walls the queue grows, and a request waits in
//     it for many ticks.
//
//   - When an entity asks again (a new order, a replan), its old request is freed and leaves the
//     queue; the new one joins at the back. Measured in the user's wall assault replay: from the
//     climb on, 500 to 1100 requests waited, 140 to 180 joined per tick, 3 to 5 were served per
//     tick, and only 7 % of all queued requests were ever served (the defender's included). The
//     soldiers' immediate searches alone used about 2000 nodes per tick, so the queue usually got
//     its one request per tick.
//
// The fix, on from launch (SIEGEFIX_OFF=1 starts off, J toggles):
//   A. The queue gets its own node budget: the counter is zeroed where the queue is reached
//      (mid-function hook FUN_103aac30+0x1E8; SIEGEFIX_QUEUE_OWN=0 turns it off), and the budget
//      500 becomes SIEGEFIX_BUDGET (default 2000; 500..20000; a 4-byte patch of the compare's
//      immediate). SIEGEFIX_INCR (default 200 = unchanged; 200..20000) patches the cap of the
//      incremental searches the same way.
//   B. The local player's requests skip the queue: in FUN_10385fd0 (entry hook), a request from
//      the movement state machine for a soldier of the local player's army is searched at once,
//      as the game does for every soldier's slot order (SIEGEFIX_PLAYER_SYNC=0 turns it off). The
//      local player's army: world +0x281E4 holds (army index, alliance index) pairs; its unit list
//      is read every 200 ms; a soldier's group (soldier +0x3B4) holds its army unit at +0x510.
//
// The probe (SIEGEFIX_PROBE=1, research) adds hooks on the pathfinder update and writes
// <game>\siegefix_runs\<stamp>\:
//   siege_ticks.csv     one row per pathfinder update: queue length, nodes used before the
//                       queue, requests served, nodes they used, queue left, wall time, queries
//                       of the tick by side and mode, the game's own counters
//   siege_requests.csv  one row per queued request when it is served (side, mode, wait, place
//                       in the queue, nodes, wall time), and one per immediate query of the
//                       player's side

#include "log.h"
#include "hook.h"
#include "gamemem.h"
#include "keys.h"
#include "stack_offsets_siege.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint32_t g_base = 0;

// ---------------------------------------------------------------------------
// Hooks (stolen bytes: docs/re/prologues_siegefix.txt from tools/ghidra/dump_prologue.py; whole
// instructions, no branch into the range) and the two immediate patches
// ---------------------------------------------------------------------------

enum {
    H_UPDATE = 0,      // FUN_103aac30 entry: pathfinder per-tick update (ECX = pathfinder)
    H_INCR_END,        // FUN_103aac30+0x1E8: incremental searches done, queue not yet served (ESI = pathfinder)
    H_SEARCH,          // FUN_1039a500 entry: serve one queued request (ECX = pathfinder, arg 1 = request)
    H_SEARCHED,        // FUN_103aac30+0x28B: after it (AL = result, EDI = request, ESI = pathfinder)
    H_DRAIN_END,       // FUN_103aac30+0x2D3: queue served for this tick (EBX = served, EDI = first left)
    H_QUERY,           // FUN_10385fd0 entry: path query (ECX = pathfinder, arg 1 = request; the fix, part B)
    H_ENQUEUE,         // FUN_1035fb40 entry: queued request built (ECX = it, arg 1 = pathfinder, arg 4 = request)
    HOOK_COUNT
};

static const HookDef g_hookDefs[HOOK_COUNT] = {
    { "FUN_103aac30", "pathfinder update", 0x3AAC30, 6,
      { 0x81, 0xEC, 0x9C, 0x00, 0x00, 0x00 }, -1, true },                  // SUB ESP,0x9C
    { "FUN_103aac30+0x1E8", "queue reached (the fix, part A)", 0x3AAE18, 6,
      { 0x8B, 0xBE, 0x78, 0x01, 0x00, 0x00 }, -1, false },                  // MOV EDI,[ESI+0x178]
    { "FUN_1039a500", "queued search", 0x39A500, 5,
      { 0x83, 0xEC, 0x40, 0x53, 0x55 }, -1, true },                        // SUB ESP,0x40; PUSH EBX; PUSH EBP
    { "FUN_103aac30+0x28B", "queued search done", 0x3AAEBB, 7,
      { 0x88, 0x47, 0x21, 0xC6, 0x47, 0x20, 0x01 }, -1, true },            // MOV [EDI+0x21],AL; MOV byte [EDI+0x20],1
    { "FUN_103aac30+0x2D3", "queue served", 0x3AAF03, 6,
      { 0x8B, 0x86, 0x8C, 0x01, 0x00, 0x00 }, -1, true },                  // MOV EAX,[ESI+0x18C]
    { "FUN_10385fd0", "path query", 0x385FD0, 5,
      { 0x83, 0xEC, 0x40, 0x53, 0x55 }, -1, false },                       // SUB ESP,0x40; PUSH EBX; PUSH EBP
    { "FUN_1035fb40", "request queued", 0x35FB40, 5,
      { 0x8B, 0x44, 0x24, 0x04, 0x56 }, -1, true },                        // MOV EAX,[ESP+4]; PUSH ESI
};

static HookSlot g_slots[HOOK_COUNT] = {};
extern "C" uint8_t* g_siegeTrampolines[HOOK_COUNT] = {};

// A data patch: the whole instruction is byte-checked, then only the immediate is written.
struct PatchDef {
    const char* func;
    const char* role;
    uint32_t    rva;           // instruction start
    int         len;
    uint8_t     expected[12];
    int         immOff;        // offset of the imm32 in the instruction
    uint32_t    original;      // the game's value
};

enum { P_BUDGET = 0, P_INCR, PATCH_COUNT };
static const PatchDef g_patchDefs[PATCH_COUNT] = {
    { "FUN_103aac30+0x274", "queued-request node budget", 0x3AAEA4, 12,
      { 0x81, 0xBE, 0x70, 0x04, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00, 0x73, 0x53 }, 6, 500 },   // CMP [ESI+0x470],500; JAE
    { "FUN_103aac30+0x91", "incremental search cap", 0x3AACC1, 12,
      { 0x81, 0xBE, 0x68, 0x03, 0x00, 0x00, 0xC8, 0x00, 0x00, 0x00, 0x0F, 0x83 }, 6, 200 },   // CMP [ESI+0x368],200; JAE
};

// ---------------------------------------------------------------------------
// Engine layout (docs/re/path_queue_x86.md, docs/re/request_radius_x86.md)
// ---------------------------------------------------------------------------

static const uint32_t P_OFF_QHEAD = 0x148, P_OFF_QTAIL = 0x14C, P_OFF_QLAST1 = 0x150;   // queue: head, tail, last mode-1
static const uint32_t P_OFF_INCR = 0x114;              // incremental search list (next at +0x24)
static const uint32_t P_OFF_INCR_COUNT = 0x368;        // incremental work this tick (cap 200)
static const uint32_t P_OFF_NODES = 0x470;             // A* nodes since the last served queue (budget 500)
static const uint32_t P_OFF_COUNTERS = 0x680;          // game counters +0x680..+0x6C4 (18 dwords)
static const int      P_COUNTERS = 18;
static const uint32_t R_OFF_NEXT = 0x1C, R_OFF_INCR_NEXT = 0x24;
static const uint32_t R_OFF_W = 0x50, R_OFF_MODE = 0x54;                        // the request copy at +0x24
static const uint32_t R_OFF_SX = 0x28, R_OFF_SZ = 0x30, R_OFF_SREG = 0x34;
static const uint32_t R_OFF_EX = 0x3C, R_OFF_EZ = 0x44, R_OFF_EREG = 0x48;
static const uint32_t Q_OFF_W = 0x2C, Q_OFF_MODE = 0x30;
static const uint32_t E_OFF_WORLD = 0x28, E_OFF_UNIT = 0x3B4;   // soldier entity: world, soldier group
static const uint32_t G_OFF_ARMY_UNIT = 0x510;          // soldier group: the army's unit it belongs to
static const uint32_t RVA_RET_REPLAN = 0x3DF50D;       // return address of the query call in FUN_103df3f0
// Local players (docs/re/path_queue_x86.md): world +0x281E4 -> (army index, alliance index) pairs,
// count world +0x281E0; battle = [[world+8]+0xB4]; battle +0x88 alliance count, +0x8C array; alliance
// +0x20 army count, +0x24 array; army +0x114 unit count, +0x118 array.
static const uint32_t W_OFF_LOCAL = 0x281E4, W_OFF_LOCAL_N = 0x281E0;
static const uint32_t B_OFF_NALL = 0x88, B_OFF_ALL = 0x8C, A_OFF_NARMY = 0x20, A_OFF_ARMY = 0x24;
static const uint32_t ARMY_OFF_NUNIT = 0x114, ARMY_OFF_UNIT = 0x118;
static const int LOCAL_MAX = 256;

static inline uint32_t R32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint32_t Rd(uint32_t addr) { uint32_t v = 0; ReadDword(addr, &v); return v; }
static inline float RdF(uint32_t addr) { uint32_t v = Rd(addr); float f; memcpy(&f, &v, 4); return f; }

// ---------------------------------------------------------------------------
// Fix state
// ---------------------------------------------------------------------------

static volatile LONG g_fixOn = 0;
static bool g_playerSync = true;
static bool g_queueOwn = true;          // the queue gets its own node budget (counter zeroed before it)
static volatile LONG g_fOwnResets = 0;
static uint32_t g_budget = 2000, g_incr = 200;
static bool g_patched[PATCH_COUNT] = {};
static CRITICAL_SECTION g_patchCs;
static volatile LONG g_fSynced = 0, g_fNotPlayer = 0, g_fBadEntity = 0, g_fPatchFail = 0;
static bool g_keyWatched = false;
static volatile LONG g_testKeys = 0;

static uint8_t* PatchImm(int i) { return (uint8_t*)(uintptr_t)(g_base + g_patchDefs[i].rva + g_patchDefs[i].immOff); }

// The instruction holds the expected bytes with `imm` as its immediate.
static bool PatchIs(int i, uint32_t imm) {
    const PatchDef& d = g_patchDefs[i];
    uint8_t want[12], have[12];
    memcpy(want, d.expected, d.len);
    memcpy(want + d.immOff, &imm, 4);
    if (!MemcpyTry(have, g_base + d.rva, d.len)) return false;
    return memcmp(want, have, d.len) == 0;
}

// Writes `imm` and reads it back; only from a state the fix knows (the game's value or ours).
static bool PatchSet(int i, uint32_t imm) {
    const PatchDef& d = g_patchDefs[i];
    uint32_t ours = i == P_BUDGET ? g_budget : g_incr;
    if (!PatchIs(i, d.original) && !PatchIs(i, ours)) return false;
    if (!Hook_WriteCode(PatchImm(i), &imm, 4)) return false;
    return PatchIs(i, imm);
}

static void ApplyPatches(bool on) {
    EnterCriticalSection(&g_patchCs);
    for (int i = 0; i < PATCH_COUNT; i++) {
        uint32_t ours = i == P_BUDGET ? g_budget : g_incr;
        if (ours == g_patchDefs[i].original) continue;           // nothing to change
        uint32_t want = on ? ours : g_patchDefs[i].original;
        if (PatchSet(i, want)) {
            g_patched[i] = on;
        } else {
            InterlockedIncrement(&g_fPatchFail);
            Log("siegefix: ERROR, %s (%s) at RVA 0x%06X could not be set to %u", g_patchDefs[i].func,
                g_patchDefs[i].role, g_patchDefs[i].rva, want);
        }
    }
    LeaveCriticalSection(&g_patchCs);
}

// ---------------------------------------------------------------------------
// Probe
// ---------------------------------------------------------------------------

static bool g_probe = false;
static CRITICAL_SECTION g_cs;
static char g_dir[MAX_PATH] = "";
static FILE* g_fTicks = nullptr;
static FILE* g_fReq = nullptr;
static FILE* g_fUnits = nullptr;   // one dump per unit seen (research: which field holds the side)
static uint32_t g_seenUnits[1024] = {};
static int g_nSeenUnits = 0;
static uint32_t g_seenObjs[8192] = {};
static int g_nSeenObjs = 0;
static DWORD g_lastFlush = 0;
static LARGE_INTEGER g_qpf = {};
static volatile LONG g_rows = 0;
static const LONG MAX_ROWS = 4000000;
static volatile LONG g_cTicks = 0, g_cQueries = 0, g_cQueued = 0, g_cServed = 0, g_cUnknownR = 0, g_cTableFull = 0;
static uint32_t g_seq = 0;

// Queries of the current tick (from any thread), reset by the tick row.
static volatile LONG g_tSyncPlayer = 0, g_tSyncOther = 0, g_tQueuedPlayer = 0, g_tQueuedOther = 0;
static volatile LONG g_tEnqPlayer = 0, g_tEnqOther = 0, g_tSynced = 0;

// The update's own values (one thread: the battle tick).
static struct {
    LARGE_INTEGER t0, tIncr, tSearch;
    uint32_t tick, qStart, nodesEntry, incrLen, incrCount, nodesPre, qAtDrain, searchNodes0;
    uint32_t servedPlayer, servedOther, nodesPlayer, nodesOther, waitMaxPlayer, drainBase;
} g_u = {};

// A query between its entry and the request it queues (same thread, nested call).
struct Pending { DWORD thread; uint32_t q, e, unit, side, local, ret, modeOrig; bool player; };
static Pending g_pending[64] = {};

// A queued request, from its queueing to its serving.
struct ReqRec {
    uint32_t r;               // 0 free, 1 removed
    uint32_t seq, e, unit, side, local, ret, mode, thread, tickEnq, qPos, qLen;
    bool player;
    LARGE_INTEGER tEnq;
};
static const uint32_t REQ_TABLE = 1 << 17;
static ReqRec* g_req = nullptr;

static int64_t UsSince(const LARGE_INTEGER& t0) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return g_qpf.QuadPart ? (now.QuadPart - t0.QuadPart) * 1000000 / g_qpf.QuadPart : 0;
}

static uint32_t Ms() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return g_qpf.QuadPart ? (uint32_t)(now.QuadPart * 1000 / g_qpf.QuadPart) : GetTickCount();
}

static bool RowBudget() { return InterlockedIncrement(&g_rows) < MAX_ROWS; }

// Length of a list from `head` through `nextOff` (guarded, capped); `upTo` stops after that node.
static uint32_t ListLen(uint32_t head, uint32_t nextOff, uint32_t upTo = 0) {
    uint32_t n = 0;
    for (uint32_t p = head; p && n < 100000; ) {
        n++;
        if (p == upTo) break;
        if (!PtrPlausible(p) || !ReadDword(p + nextOff, &p)) break;
    }
    return n;
}

static ReqRec* ReqFind(uint32_t r, bool insert) {
    uint32_t h = (r >> 3) * 2654435761u;
    for (uint32_t k = 0; k < REQ_TABLE; k++) {
        ReqRec* e = &g_req[(h + k) & (REQ_TABLE - 1)];
        if (e->r == r) return e;
        if (e->r == 0) return insert ? e : nullptr;
        if (e->r == 1 && insert) {
            // reuse a removed slot unless r sits further on
            ReqRec* later = ReqFind(r, false);
            return later ? later : e;
        }
    }
    return nullptr;
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
    snprintf(root, sizeof(root), "%ssiegefix_runs\\", exe);
    CreateDirectoryA(root, nullptr);
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_dir, sizeof(g_dir), "%s%04d%02d%02d_%02d%02d%02d\\", root, st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute, st.wSecond);
    if (!CreateDirectoryA(g_dir, nullptr)) { Log("siegefix: cannot create %s", g_dir); return false; }
    char hdr[1024];
    int n = snprintf(hdr, sizeof(hdr),
                     "ms,tick,thread,q_start,nodes_entry,incr_len,incr_count,nodes_pre,q_at_drain,served,nodes_drain,"
                     "budget_hit,q_end,us_total,us_incr,us_drain,budget_imm,incr_imm,fix_on,served_player,"
                     "served_other,nodes_player,nodes_other,wait_max_player,sync_player,sync_other,queued_player,"
                     "queued_other,enq_player,enq_other,synced");
    for (int i = 0; i < P_COUNTERS; i++) n += snprintf(hdr + n, sizeof(hdr) - n, ",c%03x", P_OFF_COUNTERS + 4 * i);
    snprintf(hdr + n, sizeof(hdr) - n, "\n");
    g_fTicks = OpenCsv("siege_ticks.csv", hdr);
    g_fReq = OpenCsv("siege_requests.csv",
                     "kind,seq,ms,tick_q,thread,ret_rva,e,unit,aunit,local,player,mode,w,sx,sz,sreg,ex,ez,ereg,r,q_pos,"
                     "q_len,tick_served,wait_ticks,wait_ms,result,nodes,us\n");
    g_fUnits = OpenCsv("siege_units.csv", "kind,unit,off,ptr,first_e,sx,sz,hex\n");
    if (!g_fTicks || !g_fReq || !g_fUnits) { Log("siegefix: cannot open the probe files in %s", g_dir); return false; }
    Log("siegefix: probe on, writing %s", g_dir);
    return true;
}

static void MaybeFlush() {
    DWORD now = GetTickCount();
    if (now - g_lastFlush < 2000) return;
    g_lastFlush = now;
    if (g_fTicks) fflush(g_fTicks);
    if (g_fReq) fflush(g_fReq);
    if (g_fUnits) fflush(g_fUnits);
}

static void HexRow(const char* kind, uint32_t unit, uint32_t off, uint32_t ptr, uint32_t e, float sx, float sz,
                   const uint8_t* b, uint32_t n) {
    static const char* hx = "0123456789ABCDEF";
    fprintf(g_fUnits, "%s,%X,%X,%X,%X,%.1f,%.1f,", kind, unit, off, ptr, e, sx, sz);
    for (uint32_t i = 0; i < n; i++) { fputc(hx[b[i] >> 4], g_fUnits); fputc(hx[b[i] & 15], g_fUnits); }
    fputc('\n', g_fUnits);
}

// Research: the first time a unit is seen, its first 0x3000 bytes, and 0x100 bytes of every object
// its first 0x400 bytes point to (once per object). Under g_cs.
static void DumpUnit(uint32_t unit, uint32_t e, float sx, float sz) {
    if (!g_fUnits || !PtrPlausible(unit)) return;
    for (int i = 0; i < g_nSeenUnits; i++) if (g_seenUnits[i] == unit) return;
    if (g_nSeenUnits >= 1024) return;
    g_seenUnits[g_nSeenUnits++] = unit;
    static uint8_t buf[0x3000];
    if (CopyGuarded(unit, buf, sizeof(buf)) != sizeof(buf)) return;
    HexRow("unit", unit, 0, 0, e, sx, sz, buf, sizeof(buf));
    // candidate army object at unit +0x748: 0x800 bytes, and 0x200 of every object it points to
    uint32_t army;
    memcpy(&army, buf + 0x748, 4);
    bool armySeen = false;
    for (int i = 0; i < g_nSeenObjs && !armySeen; i++) armySeen = g_seenObjs[i] == army;
    static uint8_t ab[0x800];
    if (!armySeen && PtrPlausible(army) && g_nSeenObjs < 8192 && CopyGuarded(army, ab, sizeof(ab)) == sizeof(ab)) {
        g_seenObjs[g_nSeenObjs++] = army;
        HexRow("army", unit, 0x748, army, 0, 0, 0, ab, sizeof(ab));
        for (uint32_t off = 0; off < 0x200; off += 4) {
            uint32_t p;
            memcpy(&p, ab + off, 4);
            uint8_t ob[0x200];
            if (!PtrPlausible(p) || p == army || CopyGuarded(p, ob, sizeof(ob)) != sizeof(ob)) continue;
            HexRow("army_obj", army, off, p, 0, 0, 0, ob, sizeof(ob));
        }
    }
    for (uint32_t off = 0; off < 0x400; off += 4) {
        uint32_t p;
        memcpy(&p, buf + off, 4);
        if (!PtrPlausible(p) || p == unit) continue;
        bool seen = false;
        for (int i = 0; i < g_nSeenObjs && !seen; i++) seen = g_seenObjs[i] == p;
        if (seen || g_nSeenObjs >= 8192) continue;
        uint8_t ob[0x100];
        if (CopyGuarded(p, ob, sizeof(ob)) != sizeof(ob)) continue;
        g_seenObjs[g_nSeenObjs++] = p;
        HexRow("obj", unit, off, p, 0, 0, 0, ob, sizeof(ob));
    }
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

// The local player's army units, read from the battle every 200 ms (any thread; under g_localCs).
static CRITICAL_SECTION g_localCs;
static uint32_t g_localUnits[LOCAL_MAX];
static int g_nLocal = 0;
static DWORD g_localAt = 0;
static uint32_t g_localWorld = 0;
static volatile LONG g_localArmies = 0, g_localReads = 0;

static int ReadLocalUnits(uint32_t world, uint32_t* out, int cap, LONG* armies) {
    int n = 0;
    *armies = 0;
    uint32_t pairs = Rd(world + W_OFF_LOCAL), np = Rd(world + W_OFF_LOCAL_N);
    uint32_t x = Rd(world + 8);
    uint32_t battle = PtrPlausible(x) ? Rd(x + 0xB4) : 0;
    if (!PtrPlausible(pairs) || np == 0 || np > 8 || !PtrPlausible(battle)) return 0;
    uint32_t nAll = Rd(battle + B_OFF_NALL), arrAll = Rd(battle + B_OFF_ALL);
    if (nAll == 0 || nAll > 16 || !PtrPlausible(arrAll)) return 0;
    for (uint32_t k = 0; k < np; k++) {
        uint32_t armyIdx = Rd(pairs + 8 * k), allIdx = Rd(pairs + 8 * k + 4);
        if (allIdx >= nAll) continue;
        uint32_t al = Rd(arrAll + 4 * allIdx);
        uint32_t nArmy = PtrPlausible(al) ? Rd(al + A_OFF_NARMY) : 0, arrArmy = PtrPlausible(al) ? Rd(al + A_OFF_ARMY) : 0;
        if (armyIdx >= nArmy || nArmy > 64 || !PtrPlausible(arrArmy)) continue;
        uint32_t army = Rd(arrArmy + 4 * armyIdx);
        uint32_t nU = PtrPlausible(army) ? Rd(army + ARMY_OFF_NUNIT) : 0, arrU = PtrPlausible(army) ? Rd(army + ARMY_OFF_UNIT) : 0;
        if (nU > (uint32_t)LOCAL_MAX || !PtrPlausible(arrU)) continue;
        (*armies)++;
        for (uint32_t i = 0; i < nU && n < cap; i++) {
            uint32_t u = Rd(arrU + 4 * i);
            if (PtrPlausible(u)) out[n++] = u;
        }
    }
    return n;
}

static bool IsLocalUnit(uint32_t world, uint32_t armyUnit) {
    if (!PtrPlausible(armyUnit)) return false;
    DWORD now = GetTickCount();
    EnterCriticalSection(&g_localCs);
    if (g_localAt == 0 || now - g_localAt >= 200 || world != g_localWorld) {
        LONG armies = 0;
        g_nLocal = ReadLocalUnits(world, g_localUnits, LOCAL_MAX, &armies);
        g_localAt = now ? now : 1;
        g_localWorld = world;
        InterlockedExchange(&g_localArmies, armies);
        InterlockedIncrement(&g_localReads);
    }
    bool r = false;
    for (int i = 0; i < g_nLocal && !r; i++) r = g_localUnits[i] == armyUnit;
    LeaveCriticalSection(&g_localCs);
    return r;
}

// Whose entity asks, from the movement state machine's query call (EBX = entity). side = the army
// unit of the soldier's group, local = 1 for the local player's.
struct Who { uint32_t e, unit, side, local; bool player; };
static Who Identify(const uint8_t* ctx, uint32_t ret, uint32_t P) {
    Who w = {};
    if (ret != g_base + RVA_RET_REPLAN) return w;
    uint32_t e = R32(ctx + SIEGE_OFF_EBX);
    if (!PtrPlausible(e)) { InterlockedIncrement(&g_fBadEntity); return w; }
    uint32_t world = Rd(e + E_OFF_WORLD);
    uint32_t p = PtrPlausible(world) ? Rd(world + 8) : 0;
    p = PtrPlausible(p) ? Rd(p + 0xB8) : 0;
    if (p != P) { InterlockedIncrement(&g_fBadEntity); return w; }   // not the entity we expect
    w.e = e;
    w.unit = Rd(e + E_OFF_UNIT);
    w.side = PtrPlausible(w.unit) ? Rd(w.unit + G_OFF_ARMY_UNIT) : 0;
    w.player = IsLocalUnit(world, w.side);
    w.local = w.player ? 1 : 0;
    return w;
}

static uint32_t HandleQuery(const uint8_t* ctx) {
    uint32_t ret = R32(ctx + SIEGE_OFF_RET);
    uint32_t P = R32(ctx + SIEGE_OFF_ECX);
    uint32_t Q = R32(ctx + SIEGE_OFF_ARG1);
    uint32_t mode = 0;
    if (!ReadDword(Q + Q_OFF_MODE, &mode)) return HOOK_CONTINUE;
    Who w = Identify(ctx, ret, P);
    bool synced = false;
    if (mode != 0 && g_fixOn && g_playerSync) {
        if (w.player) {
            synced = WriteDwordTry(Q + Q_OFF_MODE, 0);   // searched at once, as a soldier's slot order
            if (synced) InterlockedIncrement(&g_fSynced);
        } else {
            InterlockedIncrement(&g_fNotPlayer);
        }
    }
    if (!g_probe) return HOOK_CONTINUE;
    InterlockedIncrement(&g_cQueries);
    if (w.unit) {
        bool known = false;
        int n = g_nSeenUnits;
        for (int i = 0; i < n && !known; i++) known = g_seenUnits[i] == w.unit;
        if (!known) {
            EnterCriticalSection(&g_cs);
            DumpUnit(w.unit, w.e, RdF(Q + 0x04), RdF(Q + 0x0C));
            LeaveCriticalSection(&g_cs);
        }
    }
    if (synced) InterlockedIncrement(&g_tSynced);
    bool queued = mode != 0 && !synced;
    if (!queued) {
        InterlockedIncrement(w.player ? &g_tSyncPlayer : &g_tSyncOther);
        if (w.player && RowBudget()) {
            float w0 = RdF(Q + Q_OFF_W);
            EnterCriticalSection(&g_cs);
            if (g_fReq)
                fprintf(g_fReq, "%s,%u,%u,%u,%lu,%X,%X,%X,%X,%u,1,%u,%.2f,%.1f,%.1f,%X,%.1f,%.1f,%X,,,,,,,,,\n",
                        synced ? "synced" : "sync", g_seq++, Ms(), GameTickAt(g_base), GetCurrentThreadId(),
                        ret - g_base, w.e, w.unit, w.side, w.local, mode, w0, RdF(Q + 0x04), RdF(Q + 0x0C),
                        Rd(Q + 0x10), RdF(Q + 0x18), RdF(Q + 0x20), Rd(Q + 0x24));
            LeaveCriticalSection(&g_cs);
        }
        return HOOK_CONTINUE;
    }
    InterlockedIncrement(w.player ? &g_tQueuedPlayer : &g_tQueuedOther);
    // remember it for the request it may queue (cache hits and failures queue nothing)
    DWORD tid = GetCurrentThreadId();
    EnterCriticalSection(&g_cs);
    Pending* slot = nullptr;
    for (auto& p : g_pending) if (p.thread == tid) { slot = &p; break; }
    if (!slot) for (auto& p : g_pending) if (p.thread == 0) { slot = &p; break; }
    if (slot) {
        slot->thread = tid;
        slot->q = Q;
        slot->e = w.e;
        slot->unit = w.unit;
        slot->side = w.side;
        slot->local = w.local;
        slot->ret = ret - g_base;
        slot->modeOrig = mode;
        slot->player = w.player;
    }
    LeaveCriticalSection(&g_cs);
    return HOOK_CONTINUE;
}

static void HandleEnqueue(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t r = R32(ctx + SIEGE_OFF_ECX);
    uint32_t P = R32(ctx + SIEGE_OFF_ARG1);
    uint32_t Q = R32(ctx + SIEGE_OFF_ARG4);
    uint32_t mode = Rd(Q + Q_OFF_MODE);
    uint32_t head = Rd(P + P_OFF_QHEAD);
    uint32_t qLen = head ? ListLen(head, R_OFF_NEXT) : 0;
    uint32_t last1 = Rd(P + P_OFF_QLAST1);
    uint32_t qPos = mode == 1 ? (last1 ? ListLen(head, R_OFF_NEXT, last1) : 0) : qLen;   // requests ahead of it
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    uint32_t tick = GameTickAt(g_base);
    DWORD tid = GetCurrentThreadId();
    InterlockedIncrement(&g_cQueued);
    EnterCriticalSection(&g_cs);
    Pending* pend = nullptr;
    for (auto& p : g_pending) if (p.thread == tid && p.q == Q) { pend = &p; break; }
    ReqRec* rec = ReqFind(r, true);
    if (!rec) {
        InterlockedIncrement(&g_cTableFull);
    } else {
        memset(rec, 0, sizeof(*rec));
        rec->r = r;
        rec->seq = g_seq++;
        rec->mode = mode;
        rec->thread = tid;
        rec->tickEnq = tick;
        rec->tEnq = now;
        rec->qPos = qPos;
        rec->qLen = qLen;
        if (pend) {
            rec->e = pend->e;
            rec->unit = pend->unit;
            rec->side = pend->side;
            rec->local = pend->local;
            rec->ret = pend->ret;
            rec->player = pend->player;
        } else {
            rec->local = 0;
        }
    }
    bool player = pend && pend->player;
    if (g_fReq && rec && RowBudget())
        fprintf(g_fReq, "enq,%u,%u,%u,%lu,%X,%X,%X,%X,%u,%d,%u,%.2f,%.1f,%.1f,%X,%.1f,%.1f,%X,%X,%u,%u,,,,,,\n",
                rec->seq, Ms(), tick, tid, rec->ret, rec->e, rec->unit, rec->side, rec->local, rec->player ? 1 : 0,
                mode, RdF(Q + Q_OFF_W), RdF(Q + 0x04), RdF(Q + 0x0C), Rd(Q + 0x10), RdF(Q + 0x18), RdF(Q + 0x20),
                Rd(Q + 0x24), r, qPos, qLen);
    if (pend) pend->q = 0;
    LeaveCriticalSection(&g_cs);
    InterlockedIncrement(player ? &g_tEnqPlayer : &g_tEnqOther);
}

// Research: the battle's alliances, armies and their units (battle = [[[P]+8]+0xB4]; battle +0x88
// alliance count, +0x8C array; alliance +0x20 army count, +0x24 array; army +0x114 unit count,
// +0x118 array; army +0xFC its alliance, alliance +0x10 its index). Alliance and army objects are
// dumped (0x400 bytes) to find which army is the local player's.
static void DumpBattle(uint32_t P, uint32_t tick) {
    if (!g_fUnits) return;
    uint32_t world = Rd(P), x = PtrPlausible(world) ? Rd(world + 8) : 0;
    uint32_t battle = PtrPlausible(x) ? Rd(x + 0xB4) : 0;
    if (!PtrPlausible(battle)) return;
    uint32_t nAll = Rd(battle + 0x88), arrAll = Rd(battle + 0x8C);
    if (nAll > 16 || !PtrPlausible(arrAll)) return;
    // candidate local players: (army index, alliance index) pairs at world +0x281E4, count +0x281E0
    uint32_t lp = Rd(world + 0x281E4), lpn = Rd(world + 0x281E0);
    uint8_t lb[64] = {};
    if (PtrPlausible(lp) && lpn && lpn <= 8) CopyGuarded(lp, lb, lpn * 8);
    HexRow("local", tick, lpn, lp, world, 0, 0, lb, 16);
    static uint8_t ob[0x400];
    for (uint32_t i = 0; i < nAll; i++) {
        uint32_t al = Rd(arrAll + 4 * i);
        if (!PtrPlausible(al)) continue;
        if (CopyGuarded(al, ob, sizeof(ob)) == sizeof(ob)) HexRow("alliance", tick, i, al, 0, 0, 0, ob, sizeof(ob));
        uint32_t nArmy = Rd(al + 0x20), arrArmy = Rd(al + 0x24);
        if (nArmy > 32 || !PtrPlausible(arrArmy)) continue;
        for (uint32_t j = 0; j < nArmy; j++) {
            uint32_t ar = Rd(arrArmy + 4 * j);
            if (!PtrPlausible(ar)) continue;
            if (CopyGuarded(ar, ob, sizeof(ob)) == sizeof(ob)) HexRow("armyobj", tick, i * 256 + j, ar, 0, 0, 0, ob, sizeof(ob));
            uint32_t nU = Rd(ar + 0x114), arrU = Rd(ar + 0x118);
            if (nU > 256 || !PtrPlausible(arrU)) continue;
            uint8_t ub[256 * 4];
            if (nU && CopyGuarded(arrU, ub, nU * 4) == nU * 4) HexRow("armyunits", tick, i * 256 + j, ar, nU, 0, 0, ub, nU * 4);
        }
    }
}

static void HandleUpdate(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t P = R32(ctx + SIEGE_OFF_ECX);
    QueryPerformanceCounter(&g_u.t0);
    g_u.tick = GameTickAt(g_base);
    static uint32_t s_dumped = 0;
    if ((g_u.tick >= 40 && s_dumped == 0) || (g_u.tick >= 2000 && s_dumped == 1)) {
        s_dumped++;
        EnterCriticalSection(&g_cs);
        DumpBattle(P, g_u.tick);
        LeaveCriticalSection(&g_cs);
    }
    uint32_t head = Rd(P + P_OFF_QHEAD);
    g_u.qStart = head ? ListLen(head, R_OFF_NEXT) : 0;
    g_u.nodesEntry = Rd(P + P_OFF_NODES);
    g_u.incrLen = g_u.incrCount = g_u.nodesPre = g_u.qAtDrain = 0;
    g_u.servedPlayer = g_u.servedOther = g_u.nodesPlayer = g_u.nodesOther = g_u.waitMaxPlayer = 0;
    g_u.tIncr = g_u.t0;
}

static void HandleIncrEnd(const uint8_t* ctx) {
    uint32_t P = R32(ctx + SIEGE_OFF_ESI);
    uint32_t nodes = Rd(P + P_OFF_NODES);
    // part A: the immediate searches of the tick no longer use up the queue's budget
    bool reset = g_fixOn && g_queueOwn && nodes != 0 && WriteDwordTry(P + P_OFF_NODES, 0);
    if (reset) InterlockedIncrement(&g_fOwnResets);
    if (!g_probe) return;
    g_u.drainBase = reset ? 0 : nodes;
    QueryPerformanceCounter(&g_u.tIncr);
    g_u.incrCount = Rd(P + P_OFF_INCR_COUNT);
    uint32_t incr = Rd(P + P_OFF_INCR);
    g_u.incrLen = incr ? ListLen(incr, R_OFF_INCR_NEXT) : 0;
    g_u.nodesPre = nodes;
    uint32_t head = Rd(P + P_OFF_QHEAD);
    g_u.qAtDrain = head ? ListLen(head, R_OFF_NEXT) : 0;
}

static void HandleSearch(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t P = R32(ctx + SIEGE_OFF_ECX);
    g_u.searchNodes0 = Rd(P + P_OFF_NODES);
    QueryPerformanceCounter(&g_u.tSearch);
}

static void HandleSearched(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t r = R32(ctx + SIEGE_OFF_EDI);
    uint32_t P = R32(ctx + SIEGE_OFF_ESI);
    uint32_t result = R32(ctx + SIEGE_OFF_EAX) & 0xFF;
    uint32_t nodes = Rd(P + P_OFF_NODES) - g_u.searchNodes0;
    int64_t us = UsSince(g_u.tSearch);
    uint32_t tick = GameTickAt(g_base);
    InterlockedIncrement(&g_cServed);
    EnterCriticalSection(&g_cs);
    ReqRec* rec = ReqFind(r, false);
    ReqRec copy = {};
    if (rec) { copy = *rec; rec->r = 1; } else InterlockedIncrement(&g_cUnknownR);
    uint32_t waitTicks = rec && tick >= copy.tickEnq ? tick - copy.tickEnq : 0;
    if (copy.player) {
        g_u.servedPlayer++;
        g_u.nodesPlayer += nodes;
        if (waitTicks > g_u.waitMaxPlayer) g_u.waitMaxPlayer = waitTicks;
    } else {
        g_u.servedOther++;
        g_u.nodesOther += nodes;
    }
    if (g_fReq && RowBudget()) {
        if (rec)
            fprintf(g_fReq, "queued,%u,%u,%u,%u,%X,%X,%X,%X,%u,%d,%u,%.2f,%.1f,%.1f,%X,%.1f,%.1f,%X,%X,%u,%u,%u,%u,%lld,%u,%u,%lld\n",
                    copy.seq, Ms(), copy.tickEnq, copy.thread, copy.ret, copy.e, copy.unit, copy.side, copy.local,
                    copy.player ? 1 : 0, copy.mode, RdF(r + R_OFF_W), RdF(r + R_OFF_SX), RdF(r + R_OFF_SZ),
                    Rd(r + R_OFF_SREG), RdF(r + R_OFF_EX), RdF(r + R_OFF_EZ), Rd(r + R_OFF_EREG), r, copy.qPos,
                    copy.qLen, tick, waitTicks, UsSince(copy.tEnq) / 1000, result, nodes, us);
        else
            fprintf(g_fReq, "queued,,%u,,,,,,,,,%u,%.2f,%.1f,%.1f,%X,%.1f,%.1f,%X,%X,,,%u,,,%u,%u,%lld\n", Ms(),
                    Rd(r + R_OFF_MODE), RdF(r + R_OFF_W), RdF(r + R_OFF_SX), RdF(r + R_OFF_SZ), Rd(r + R_OFF_SREG),
                    RdF(r + R_OFF_EX), RdF(r + R_OFF_EZ), Rd(r + R_OFF_EREG), r, tick, result, nodes, us);
    }
    LeaveCriticalSection(&g_cs);
}

static void HandleDrainEnd(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t P = R32(ctx + SIEGE_OFF_ESI);
    uint32_t served = R32(ctx + SIEGE_OFF_EBX);
    uint32_t left = R32(ctx + SIEGE_OFF_EDI);
    uint32_t nodes = Rd(P + P_OFF_NODES);
    uint32_t qEnd = left ? ListLen(left, R_OFF_NEXT) : 0;
    int64_t usTotal = UsSince(g_u.t0), usDrain = UsSince(g_u.tIncr);
    int64_t usIncr = g_qpf.QuadPart ? (g_u.tIncr.QuadPart - g_u.t0.QuadPart) * 1000000 / g_qpf.QuadPart : 0;
    uint32_t counters[P_COUNTERS] = {};
    MemcpyTry(counters, P + P_OFF_COUNTERS, sizeof(counters));
    InterlockedIncrement(&g_cTicks);
    if (g_u.tick == 0) return;                              // not in a battle
    if (!RowBudget()) return;
    EnterCriticalSection(&g_cs);
    if (g_fTicks) {
        fprintf(g_fTicks, "%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%lld,%lld,%lld,%u,%u,%ld,%u,%u,%u,%u,%u,%ld,%ld,%ld,%ld,%ld,%ld,%ld",
                Ms(), g_u.tick, GetCurrentThreadId(), g_u.qStart, g_u.nodesEntry, g_u.incrLen, g_u.incrCount,
                g_u.nodesPre, g_u.qAtDrain, served, nodes - g_u.drainBase, left ? 1 : 0, qEnd, usTotal, usIncr,
                usDrain, Rd((uint32_t)(uintptr_t)PatchImm(P_BUDGET)), Rd((uint32_t)(uintptr_t)PatchImm(P_INCR)),
                g_fixOn, g_u.servedPlayer, g_u.servedOther, g_u.nodesPlayer, g_u.nodesOther, g_u.waitMaxPlayer,
                InterlockedExchange(&g_tSyncPlayer, 0), InterlockedExchange(&g_tSyncOther, 0),
                InterlockedExchange(&g_tQueuedPlayer, 0), InterlockedExchange(&g_tQueuedOther, 0),
                InterlockedExchange(&g_tEnqPlayer, 0), InterlockedExchange(&g_tEnqOther, 0),
                InterlockedExchange(&g_tSynced, 0));
        for (int i = 0; i < P_COUNTERS; i++) fprintf(g_fTicks, ",%u", counters[i]);
        fputc('\n', g_fTicks);
    }
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// Key, detours, harness exports
// ---------------------------------------------------------------------------

// J toggles the fix (dev build; focus only through proxy/keys). Harness: SiegeFixTestKeys bit 0.
extern "C" __declspec(dllexport) void __stdcall SiegeFixTestKeys(LONG bits) { InterlockedExchange(&g_testKeys, bits); }

static void ToggleKey() {
    bool on = InterlockedCompareExchange(&g_fixOn, 0, 0) == 0;
    ApplyPatches(on);
    InterlockedExchange(&g_fixOn, on ? 1 : 0);
    Log("siegefix: J, fix %s (player requests searched at once so far: %ld)", on ? "ON" : "OFF", g_fSynced);
    MessageBeep(on ? MB_ICONASTERISK : MB_OK);
}

extern "C" void __stdcall SiegeHandler0(const uint8_t* ctx) { HandleUpdate(ctx); }
extern "C" void __stdcall SiegeHandler1(const uint8_t* ctx) { HandleIncrEnd(ctx); }
extern "C" void __stdcall SiegeHandler2(const uint8_t* ctx) { HandleSearch(ctx); }
extern "C" void __stdcall SiegeHandler3(const uint8_t* ctx) { HandleSearched(ctx); }
extern "C" void __stdcall SiegeHandler4(const uint8_t* ctx) { HandleDrainEnd(ctx); }
extern "C" void __stdcall SiegeHandler5(const uint8_t* ctx) { HandleQuery(ctx); }
extern "C" void __stdcall SiegeHandler6(const uint8_t* ctx) { HandleEnqueue(ctx); }

HOOK_DETOUR(SiegeDetour0, SiegeHandler0, g_siegeTrampolines, 0)
HOOK_DETOUR(SiegeDetour1, SiegeHandler1, g_siegeTrampolines, 1)
HOOK_DETOUR(SiegeDetour2, SiegeHandler2, g_siegeTrampolines, 2)
HOOK_DETOUR(SiegeDetour3, SiegeHandler3, g_siegeTrampolines, 3)
HOOK_DETOUR(SiegeDetour4, SiegeHandler4, g_siegeTrampolines, 4)
HOOK_DETOUR(SiegeDetour5, SiegeHandler5, g_siegeTrampolines, 5)
HOOK_DETOUR(SiegeDetour6, SiegeHandler6, g_siegeTrampolines, 6)

static const HookDetour g_detours[HOOK_COUNT] = { SiegeDetour0, SiegeDetour1, SiegeDetour2, SiegeDetour3,
                                                  SiegeDetour4, SiegeDetour5, SiegeDetour6 };

// Harness only (fixes/siege-responsiveness-fix/tests): fix and probe counters.
extern "C" __declspec(dllexport) void __stdcall SiegeFixTestCounters(long* out) {
    out[0] = g_probe ? 1 : 0;
    out[1] = g_fixOn;
    out[2] = (long)g_budget;
    out[3] = (long)g_incr;
    out[4] = g_playerSync ? 1 : 0;
    out[5] = g_fSynced;
    out[6] = g_fNotPlayer;
    out[7] = g_fBadEntity;
    out[8] = g_fPatchFail;
    out[9] = g_cTicks;
    out[10] = g_cQueries;
    out[11] = g_cQueued;
    out[12] = g_cServed;
    out[13] = g_cUnknownR;
    out[14] = g_base ? 1 : 0;
    out[15] = g_localArmies;
    out[16] = g_localReads;
    out[17] = g_queueOwn ? 1 : 0;
    out[18] = g_fOwnResets;
}

// ---------------------------------------------------------------------------
// Install / remove
// ---------------------------------------------------------------------------

static uint32_t EnvRange(const char* name, uint32_t def, uint32_t lo, uint32_t hi) {
    char v[16] = "";
    if (GetEnvironmentVariableA(name, v, sizeof(v)) == 0) return def;
    long x = atol(v);
    if (x < (long)lo || x > (long)hi) {
        Log("siegefix: %s=%s out of range %u..%u, using %u", name, v, lo, hi, def);
        return def;
    }
    return (uint32_t)x;
}

static bool EnvIs(const char* name, char c) {
    char v[8] = "";
    return GetEnvironmentVariableA(name, v, sizeof(v)) > 0 && v[0] == c;
}

void SiegeFix_Install() {
    Log("siegefix: siege responsiveness fix (dev build)");
    Mem_Init();
    uint32_t size = 0;
    uint8_t* base = Hook_GameModule(false, &size);
    if (!base) { Log("siegefix: REFUSED, empire.retail.dll is not loaded"); return; }
    if (size != HOOK_ANALYSED_IMAGE_SIZE) { Log("siegefix: REFUSED, SizeOfImage 0x%08X is not the analysed build", size); return; }
    for (int h = 0; h < HOOK_COUNT; h++) {
        if (!Hook_SiteMatches(base, g_hookDefs[h])) {
            Log("siegefix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", g_hookDefs[h].rva,
                g_hookDefs[h].func);
            return;
        }
    }
    for (int i = 0; i < PATCH_COUNT; i++) {
        const PatchDef& d = g_patchDefs[i];
        if (memcmp(base + d.rva, d.expected, d.len) != 0) {
            Log("siegefix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", d.rva, d.func);
            return;
        }
    }
    g_base = (uint32_t)(uintptr_t)base;
    InitializeCriticalSection(&g_cs);
    InitializeCriticalSection(&g_patchCs);
    InitializeCriticalSection(&g_localCs);
    QueryPerformanceFrequency(&g_qpf);
    g_budget = EnvRange("SIEGEFIX_BUDGET", 2000, 500, 20000);
    g_incr = EnvRange("SIEGEFIX_INCR", 200, 200, 20000);
    g_playerSync = !EnvIs("SIEGEFIX_PLAYER_SYNC", '0');
    g_queueOwn = !EnvIs("SIEGEFIX_QUEUE_OWN", '0');
    bool off = EnvIs("SIEGEFIX_OFF", '1');
    if (EnvIs("SIEGEFIX_PROBE", '1')) {
        g_req = (ReqRec*)VirtualAlloc(nullptr, sizeof(ReqRec) * REQ_TABLE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (g_req && OpenProbe()) g_probe = true;
        else Log("siegefix: probe not started (no memory or no run folder)");
    }
    bool use[HOOK_COUNT];
    for (int h = 0; h < HOOK_COUNT; h++) use[h] = !g_hookDefs[h].probeOnly || g_probe;
    use[H_INCR_END] = g_queueOwn || g_probe;
    if (!Hook_Prepare(base, g_hookDefs, HOOK_COUNT, use, g_siegeTrampolines, g_slots)) {
        Log("siegefix: REFUSED, no trampoline memory");
        g_probe = false;
        g_base = 0;
        return;
    }
    if (!off) ApplyPatches(true);
    InterlockedExchange(&g_fixOn, off ? 0 : 1);
    Hook_Patch(g_detours, g_slots, HOOK_COUNT);
    for (int h = 0; h < HOOK_COUNT; h++)
        if (g_slots[h].active)
            Log("siegefix: hook %s (%s) installed at RVA 0x%06X", g_hookDefs[h].func, g_hookDefs[h].role, g_hookDefs[h].rva);
    Log("siegefix: fix %s; queued-request node budget %u per tick (game 500)%s, incremental cap %u (game 200); "
        "local player's requests %s", off ? "OFF (SIEGEFIX_OFF=1)" : "ON", g_budget,
        g_queueOwn ? ", the queue's own" : ", shared with the immediate searches (SIEGEFIX_QUEUE_OWN=0)", g_incr,
        g_playerSync ? "searched at once" : "queued as in the game (SIEGEFIX_PLAYER_SYNC=0)");
    g_keyWatched = Keys_Watch('J', &g_testKeys, 1, ToggleKey);
    Log("siegefix: J toggles the fix on / off%s", g_keyWatched ? "" : " (ERROR: key poll not started)");
}

void SiegeFix_Remove() {
    if (g_keyWatched) { Keys_Unwatch(ToggleKey); g_keyWatched = false; }
    Hook_Remove(g_slots, HOOK_COUNT);
    if (!g_base) return;
    ApplyPatches(false);
    for (int i = 0; i < PATCH_COUNT; i++)
        if (!PatchIs(i, g_patchDefs[i].original))
            Log("siegefix: WARNING, %s at RVA 0x%06X not restored (bytes changed by someone else)",
                g_patchDefs[i].func, g_patchDefs[i].rva);
    Log("siegefix: local player's requests searched at once %ld, other queued requests %ld, entities not "
        "recognised %ld, queue budget resets %ld, local army reads %ld (armies %ld), patch failures %ld", g_fSynced,
        g_fNotPlayer, g_fBadEntity, g_fOwnResets, g_localReads, g_localArmies, g_fPatchFail);
    if (g_probe) {
        EnterCriticalSection(&g_cs);
        if (g_fTicks) fclose(g_fTicks);
        if (g_fReq) fclose(g_fReq);
        if (g_fUnits) fclose(g_fUnits);
        g_fTicks = g_fReq = g_fUnits = nullptr;
        g_probe = false;
        LeaveCriticalSection(&g_cs);
        Log("siegefix: probe closed: updates %ld, queries %ld, queued %ld, served %ld (not seen queued %ld, table "
            "full %ld), rows %ld", g_cTicks, g_cQueries, g_cQueued, g_cServed, g_cUnknownR, g_cTableFull, g_rows);
    }
}
