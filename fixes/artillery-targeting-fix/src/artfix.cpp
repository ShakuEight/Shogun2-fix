// artfix.cpp -- artillery targeting fix for Total War: Shogun 2 (original x86 Empire.Retail.dll).
// Folder fixes/artillery-targeting-fix/ (named aimfix until 2026-10-18).
//
// Dev build. The game aims each artillery piece at one soldier of the target unit, roughly the one
// in front of the gun, so a battery standing off to one side of its target hits the end of the
// line. The fix (one entry hook, FUN_1047fd90, see "The fix" below) points every artillery shot at
// the soldier nearest to the target unit's centroid. On at launch; ARTFIX_OFF=1 starts it off; the U
// key toggles it on / off in the game.
//
// ARTFIX_PROBE=1 (research, used with tools/autorun --env) installs two more hooks that write
// <game>\artfix_runs\<stamp>\: aim_calls.csv (one row per shot, from the gun's fire routine, via the
// target-position getter FUN_103a6850), aim_settarget.csv (every target record a gun takes,
// FUN_1045a8a0), aim_units.csv (target unit soldier positions once per game second) and
// aim_fix.csv (every change the fix makes). ARTFIX_WATCH=1 adds a write watchpoint on the first
// four guns' target records (aim_watch.csv); that is how the chain was found.
//
// Shared pieces (proxy/): hook engine (hook.h), guarded reads and the battle tick (gamemem.h),
// the key poll thread (keys.h).

#include "log.h"
#include "hook.h"
#include "gamemem.h"
#include "keys.h"
#include "stack_offsets_art.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

static uint32_t g_base = 0;

// ---------------------------------------------------------------------------
// Hook table
// ---------------------------------------------------------------------------

enum {
    H_TARGETPOS = 0,   // FUN_103a6850 entry
    H_SETTARGET,       // FUN_1045a8a0 entry (shooter entity takes a target record)
    H_ORDERCOPY,       // FUN_1047fd90 entry (engine fire action hands its target to the gun): the fix
    HOOK_COUNT
};

static const HookDef g_hookDefs[HOOK_COUNT] = {
    { "FUN_103a6850", "target position getter", 0x3A6850, 6,
      { 0x83, 0xEC, 0x0C, 0x56, 0x8B, 0xF1 }, -1, true },      // SUB ESP,0xC; PUSH ESI; MOV ESI,ECX
    { "FUN_1045a8a0", "shooter set target",     0x45A8A0, 6,
      { 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x10 }, -1, true },      // MOVSS XMM0,[ESP+0x10]
    { "FUN_1047fd90", "engine fire action update", 0x47FD90, 8,
      { 0x53, 0x56, 0x8B, 0xF1, 0x80, 0x7E, 0x7C, 0x00 }, -1, false },
      // PUSH EBX; PUSH ESI; MOV ESI,ECX; CMP byte [ESI+0x7C],0
};

static HookSlot g_slots[HOOK_COUNT] = {};
extern "C" uint8_t* g_artTrampolines[HOOK_COUNT] = {};

// ---------------------------------------------------------------------------
// Guarded reads (proxy/gamemem.h)
// ---------------------------------------------------------------------------

static bool Ptr(uint32_t p) { return PtrInUser(p); }
static bool Rd(uint32_t addr, void* out, uint32_t n) { return MemcpyTry(out, addr, n); }
static bool Rd32(uint32_t addr, uint32_t* out) { return MemcpyTry(out, addr, 4); }
static bool Wr32(uint32_t addr, uint32_t v) { return WriteDwordTry(addr, v); }

struct Vec3 { float x, y, z; };

static bool RdPos(uint32_t entity, Vec3* p) { return Rd(entity + 0x48, p, 12); }

static uint32_t GameTick() { return GameTickAt(g_base); }

// ---------------------------------------------------------------------------
// Unit of an entity: the dword at entity+off that points to a unit whose soldier array holds the
// entity. The offset is found once by search and then verified on every use.
// ---------------------------------------------------------------------------

static const uint32_t UNIT_OFF_COUNT = 0x18;
static const uint32_t UNIT_OFF_SOLDIERS = 0x1C;
static const uint32_t MAX_SOLDIERS = 1024;
static volatile LONG g_entUnitOff = -1;

static bool UnitHolds(uint32_t unit, uint32_t entity, uint32_t* count, uint32_t* arr, int* index) {
    uint32_t n, a;
    if (!Rd32(unit + UNIT_OFF_COUNT, &n) || !Rd32(unit + UNIT_OFF_SOLDIERS, &a)) return false;
    if (n == 0 || n > MAX_SOLDIERS || !Ptr(a)) return false;
    static uint32_t buf[MAX_SOLDIERS];   // callers hold g_cs
    if (!Rd(a, buf, n * 4)) return false;
    for (uint32_t i = 0; i < n; i++) {
        if (buf[i] == entity) {
            *count = n; *arr = a; *index = (int)i;
            return true;
        }
    }
    return false;
}

static uint32_t UnitOf(uint32_t entity, uint32_t* count, uint32_t* arr, int* index) {
    LONG off = g_entUnitOff;
    uint32_t u;
    if (off >= 0 && Rd32(entity + off, &u) && Ptr(u) && UnitHolds(u, entity, count, arr, index)) return u;
    for (uint32_t k = 0; k < 0x1000; k += 4) {
        if (Rd32(entity + k, &u) && Ptr(u) && u != entity && UnitHolds(u, entity, count, arr, index)) {
            if (off != (LONG)k) {
                InterlockedExchange(&g_entUnitOff, (LONG)k);
                Log("artfix: entity -> unit pointer found at entity+0x%X (unit 0x%08X, %u soldiers)", k, u, *count);
            }
            return u;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Probe output (ARTFIX_PROBE=1): <game>\artfix_runs\<stamp>\aim_calls.csv and aim_units.csv
// ---------------------------------------------------------------------------

static CRITICAL_SECTION g_cs;
static void WatchStart(const char* dir);
static bool g_probe = false;
static char g_dir[MAX_PATH] = "";
static FILE* g_calls = nullptr;
static FILE* g_units = nullptr;
static FILE* g_sets = nullptr;
static uint32_t g_rows = 0;
static const uint32_t MAX_ROWS = 400000;
static DWORD g_lastFlush = 0;

struct UnitDump { uint32_t unit; uint32_t tick; };
static UnitDump g_dumped[64];

static bool EnvIs1(const char* name) {
    char v[8] = "";
    return GetEnvironmentVariableA(name, v, sizeof(v)) > 0 && v[0] == '1';
}

static void OpenProbe() {
    if (!EnvIs1("ARTFIX_PROBE")) return;
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    char* slash = strrchr(exe, '\\');
    if (slash) slash[1] = 0;
    char root[MAX_PATH], dir[MAX_PATH], path[MAX_PATH];
    snprintf(root, sizeof(root), "%sartfix_runs\\", exe);
    CreateDirectoryA(root, nullptr);
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(dir, sizeof(dir), "%s%04d%02d%02d_%02d%02d%02d\\", root, st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond);
    if (!CreateDirectoryA(dir, nullptr)) { Log("artfix: cannot create %s", dir); return; }
    strcpy(g_dir, dir);
    snprintf(path, sizeof(path), "%saim_calls.csv", dir);
    g_calls = fopen(path, "w");
    snprintf(path, sizeof(path), "%saim_units.csv", dir);
    g_units = fopen(path, "w");
    snprintf(path, sizeof(path), "%saim_settarget.csv", dir);
    g_sets = fopen(path, "w");
    if (!g_calls || !g_units || !g_sets) { Log("artfix: cannot open the probe files in %s", dir); return; }
    setvbuf(g_calls, nullptr, _IOFBF, 1 << 20);
    setvbuf(g_units, nullptr, _IOFBF, 1 << 20);
    fprintf(g_calls, "ms,tick,thread,ret_rva,rec,off_x,off_y,off_z,ent,obj,xform,ent_x,ent_y,ent_z,"
                     "gun,gun_x,gun_y,gun_z,unit,count,index,cen_x,cen_z,near_idx,near_x,near_z,"
                     "far_idx,far_x,far_z\n");
    fprintf(g_units, "ms,tick,unit,count,i,ent,x,y,z\n");
    fprintf(g_sets, "ms,tick,thread,ret_rva,shooter,sh_x,sh_y,sh_z,src,off_x,off_y,off_z,ent,obj,xform,"
                    "ent_x,ent_z,unit,count,index,cen_x,cen_z,flags,esi,esi_vt,stack\n");
    setvbuf(g_sets, nullptr, _IOFBF, 1 << 20);
    g_probe = true;
    Log("artfix: probe on, writing %s", dir);
    WatchStart(dir);
}

static void DumpUnit(uint32_t unit, uint32_t tick, uint32_t count, uint32_t arr, DWORD ms) {
    int slot = -1, freeSlot = -1;
    for (int i = 0; i < 64; i++) {
        if (g_dumped[i].unit == unit) { slot = i; break; }
        if (!g_dumped[i].unit && freeSlot < 0) freeSlot = i;
    }
    if (slot >= 0 && tick - g_dumped[slot].tick < 10) return;   // once per game second
    if (slot < 0) { if (freeSlot < 0) return; slot = freeSlot; g_dumped[slot].unit = unit; }
    g_dumped[slot].tick = tick;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t e;
        Vec3 p;
        if (!Rd32(arr + 4 * i, &e) || !RdPos(e, &p)) continue;
        fprintf(g_units, "%lu,%u,%08X,%u,%u,%08X,%.3f,%.3f,%.3f\n", ms, tick, unit, count, i, e, p.x, p.y, p.z);
    }
}

static DWORD g_t0 = 0;
static bool IsArtillery(uint32_t shooter);

static void ProbeCall(uint32_t rec, uint32_t retRva) {
    struct { float ox, oy, oz; uint32_t ent, obj, xform; } r;
    if (!Rd(rec, &r, sizeof(r))) return;
    DWORD ms = GetTickCount() - g_t0;
    uint32_t tick = GameTick();
    Vec3 ep = {0, 0, 0}, gp = {0, 0, 0};
    bool hasEnt = Ptr(r.ent) && RdPos(r.ent, &ep);
    uint32_t gun = 0;
    // from the gun's fire routine (FUN_10488c40) the record is the gun's own, at gun+0x71C
    gun = rec - 0x71C;
    bool hasGun = IsArtillery(gun) && RdPos(gun, &gp);
    EnterCriticalSection(&g_cs);
    if (g_rows < MAX_ROWS) {
        g_rows++;
        uint32_t unit = 0, count = 0, arr = 0;
        int index = -1, nearI = -1, farI = -1;
        float cx = 0, cz = 0;
        Vec3 nearP = {0, 0, 0}, farP = {0, 0, 0};
        if (hasEnt) unit = UnitOf(r.ent, &count, &arr, &index);
        if (unit) {
            float best = 1e30f, worst = -1.0f;
            int n = 0;
            for (uint32_t i = 0; i < count; i++) {
                uint32_t e;
                Vec3 p;
                if (!Rd32(arr + 4 * i, &e) || !RdPos(e, &p)) continue;
                cx += p.x; cz += p.z; n++;
                if (hasGun) {
                    float d = (p.x - gp.x) * (p.x - gp.x) + (p.z - gp.z) * (p.z - gp.z);
                    if (d < best) { best = d; nearI = (int)i; nearP = p; }
                    if (d > worst) { worst = d; farI = (int)i; farP = p; }
                }
            }
            if (n) { cx /= n; cz /= n; }
            DumpUnit(unit, tick, count, arr, ms);
        }
        fprintf(g_calls, "%lu,%u,%lu,%06X,%08X,%.3f,%.3f,%.3f,%08X,%08X,%08X,%.3f,%.3f,%.3f,"
                         "%08X,%.3f,%.3f,%.3f,%08X,%u,%d,%.3f,%.3f,%d,%.3f,%.3f,%d,%.3f,%.3f\n",
                ms, tick, GetCurrentThreadId(), retRva, rec, r.ox, r.oy, r.oz, r.ent, r.obj, r.xform,
                ep.x, ep.y, ep.z, hasGun ? gun : 0, gp.x, gp.y, gp.z, unit, count, index, cx, cz,
                nearI, nearP.x, nearP.z, farI, farP.x, farP.z);
        DWORD now = GetTickCount();
        if (now - g_lastFlush > 2000) { fflush(g_calls); fflush(g_units); fflush(g_sets); g_lastFlush = now; }
    }
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// Write watchpoint (probe only): Dr0..Dr3 on the entity field (+0x0C) of the first four target
// records seen at fire time, armed on every other thread by a background thread. The vectored
// handler records the instruction after the write and module return-address candidates found on
// the stack, into aim_watch.csv.
// ---------------------------------------------------------------------------

#include <tlhelp32.h>

static volatile LONG g_watchAddr[4] = {};
static volatile LONG g_watchCount = 0;
static FILE* g_watch = nullptr;
static volatile LONG g_watchRows = 0;
static HANDLE g_watchThread = nullptr;
static volatile LONG g_quit = 0;
static PVOID g_veh = nullptr;
static DWORD g_watchTid = 0;

static void WatchAdd(uint32_t addr) {
    for (int i = 0; i < 4; i++) if ((uint32_t)g_watchAddr[i] == addr) return;
    LONG n = g_watchCount;
    if (n >= 4) return;
    if (InterlockedCompareExchange(&g_watchCount, n + 1, n) != n) return;
    InterlockedExchange(&g_watchAddr[n], (LONG)addr);
    Log("artfix: watch Dr%ld on 0x%08X (target record entity field)", n, addr);
}

static LONG CALLBACK WatchVeh(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* c = ep->ContextRecord;
    DWORD hit = c->Dr6 & 0xF;
    if (!hit) return EXCEPTION_CONTINUE_SEARCH;
    c->Dr6 = 0;
    if (g_watch && InterlockedIncrement(&g_watchRows) < 20000) {
        int slot = 0;
        while (slot < 4 && !(hit & (1u << slot))) slot++;
        uint32_t addr = (uint32_t)g_watchAddr[slot], val = 0;
        Rd32(addr, &val);
        char cands[256];
        int len = 0;
        cands[0] = 0;
        int found = 0;
        for (uint32_t k = 0; k < 0x600 && found < 10; k += 4) {
            uint32_t v;
            if (!Rd32(c->Esp + k, &v)) break;
            if (v - g_base - 0x1000 < 0x159FC3E) {   // inside the module's .text
                len += snprintf(cands + len, sizeof(cands) - len, "%s%06X@%X", found ? " " : "", v - g_base, k);
                found++;
            }
        }
        EnterCriticalSection(&g_cs);
        fprintf(g_watch, "%lu,%u,%lu,%d,%08X,%08X,%06X,%08X,%08X,%08X,%08X,%s\n", GetTickCount() - g_t0, GameTick(),
                GetCurrentThreadId(), slot, addr, val, c->Eip - g_base, c->Ecx, c->Esi, c->Edi, c->Ebx, cands);
        LeaveCriticalSection(&g_cs);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

static DWORD WINAPI WatchThread(LPVOID) {
    DWORD pid = GetCurrentProcessId();
    while (!g_quit) {
        Sleep(250);
        LONG n = g_watchCount;
        if (!n) continue;
        DWORD dr7 = 0;
        for (int i = 0; i < n; i++) dr7 |= (1u << (2 * i)) | (1u << (16 + 4 * i)) | (3u << (18 + 4 * i));
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) continue;
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
            if (te.th32OwnerProcessID != pid || te.th32ThreadID == g_watchTid) continue;
            HANDLE h = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE,
                                  te.th32ThreadID);
            if (!h) continue;
            if (SuspendThread(h) != (DWORD)-1) {
                CONTEXT ctx;
                ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(h, &ctx) && ctx.Dr7 != dr7) {
                    ctx.Dr0 = (DWORD)g_watchAddr[0];
                    ctx.Dr1 = (DWORD)g_watchAddr[1];
                    ctx.Dr2 = (DWORD)g_watchAddr[2];
                    ctx.Dr3 = (DWORD)g_watchAddr[3];
                    ctx.Dr7 = dr7;
                    SetThreadContext(h, &ctx);
                }
                ResumeThread(h);
            }
            CloseHandle(h);
        }
        CloseHandle(snap);
    }
    return 0;
}

static void WatchStart(const char* dir) {
    if (!EnvIs1("ARTFIX_WATCH")) return;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%saim_watch.csv", dir);
    g_watch = fopen(path, "w");
    if (!g_watch) return;
    setvbuf(g_watch, nullptr, _IOFBF, 1 << 16);
    fprintf(g_watch, "ms,tick,thread,slot,addr,value,eip_rva,ecx,esi,edi,ebx,stack_candidates\n");
    g_veh = AddVectoredExceptionHandler(1, WatchVeh);
    g_watchThread = CreateThread(nullptr, 0, WatchThread, nullptr, 0, &g_watchTid);
    Log("artfix: write watchpoint on (ARTFIX_WATCH=1)");
}

// ---------------------------------------------------------------------------
// The fix. An artillery piece is an entity whose class sends fire orders through the engine fire
// action (vtable slot +0x1B0 = FUN_1045a2e0; six engine classes, no infantry or cavalry class).
// The fire action (vtable 0x115E7D94) holds the target record at +0x20 and its gun at +0x60; on its
// first update ([+0x7C] == 0) FUN_1047fd90 copies the record into the gun (FUN_1045a8a0 or
// FUN_1044fb60), and the gun turns, aims and fires at record.entity + record.offset. The game
// picks a soldier of the target unit roughly in front of each gun, so a battery standing off to
// one side of its target aims every gun at the near end of the line. Before that copy the fix
// replaces the soldier with the living soldier of the same unit nearest to the unit's centroid
// (soldier array [unit+0x1C], count [unit+0x18], unit = [soldier+0x3B4], positions +0x48 / +0x50);
// the record's offset (aim height) is kept.
// ---------------------------------------------------------------------------

static const uint32_t RVA_ENGINE_FIRE_ORDER = 0x45A2E0;   // FUN_1045a2e0
static const uint32_t SHOOTER_VT_SLOT_FIRE_ORDER = 0x1B0;
static const uint32_t ACTION_OFF_RECORD = 0x20;
static const uint32_t ACTION_OFF_GUN = 0x60;
static const uint32_t ACTION_OFF_COPIED = 0x7C;
static const uint32_t ENTITY_OFF_UNIT = 0x3B4;

static volatile LONG g_fixOn = 0;
static volatile LONG g_fixChanged = 0;     // record pointed at another soldier
static volatile LONG g_fixKept = 0;        // the game's soldier was already the centre one
static volatile LONG g_fixNotSoldier = 0;  // artillery record without a soldier target (ground, building)
static volatile LONG g_fixBadUnit = 0;     // soldier not found in [soldier+0x3B4]'s array
static volatile LONG g_fixDisabled = 0;    // self-check switched the fix off; U is refused from then on
static FILE* g_fixRows = nullptr;          // probe only

static bool IsArtillery(uint32_t shooter) {
    uint32_t vt, fn;
    return Ptr(shooter) && Rd32(shooter, &vt) && Rd32(vt + SHOOTER_VT_SLOT_FIRE_ORDER, &fn) &&
           fn == g_base + RVA_ENGINE_FIRE_ORDER;
}

// Soldier of `unit` nearest to the unit's centroid (x / z). Returns 0 when `member` is not in the
// unit's array or a read fails.
static uint32_t CentreSoldier(uint32_t unit, uint32_t member, uint32_t* countOut, float* cx, float* cz) {
    uint32_t n, arr;
    if (!Rd32(unit + UNIT_OFF_COUNT, &n) || !Rd32(unit + UNIT_OFF_SOLDIERS, &arr)) return 0;
    if (n == 0 || n > MAX_SOLDIERS || !Ptr(arr)) return 0;
    double sx = 0, sz = 0;
    int got = 0;
    bool holds = false;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t e;
        Vec3 p;
        if (!Rd32(arr + 4 * i, &e) || !RdPos(e, &p)) return 0;
        holds |= e == member;
        sx += p.x; sz += p.z; got++;
    }
    if (!holds || !got) return 0;
    *cx = (float)(sx / got);
    *cz = (float)(sz / got);
    uint32_t best = 0;
    float bestD = 3.4e38f;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t e;
        Vec3 p;
        if (!Rd32(arr + 4 * i, &e) || !RdPos(e, &p)) return 0;
        float d = (p.x - *cx) * (p.x - *cx) + (p.z - *cz) * (p.z - *cz);
        if (d < bestD) { bestD = d; best = e; }
    }
    *countOut = n;
    return best;
}

static void HandleOrderCopy(const uint8_t* ctx) {
    if (!g_fixOn) return;
    uint32_t action = *(const uint32_t*)(ctx + ART_OFF_ECX);
    uint8_t copied = 1;
    if (!Rd(action + ACTION_OFF_COPIED, &copied, 1) || copied) return;   // only right before the copy
    uint32_t gun;
    if (!Rd32(action + ACTION_OFF_GUN, &gun) || !IsArtillery(gun)) return;
    struct { float ox, oy, oz; uint32_t ent, obj, xform; } r;
    if (!Rd(action + ACTION_OFF_RECORD, &r, sizeof(r))) return;
    if (!Ptr(r.ent) || r.obj || r.xform) { InterlockedIncrement(&g_fixNotSoldier); return; }
    uint32_t unit, count = 0;
    float cx = 0, cz = 0;
    uint32_t centre = 0;
    if (Rd32(r.ent + ENTITY_OFF_UNIT, &unit) && Ptr(unit)) centre = CentreSoldier(unit, r.ent, &count, &cx, &cz);
    if (!centre) {
        // self-check: the soldier -> unit link must hold; switch off for the session if it never does
        LONG bad = InterlockedIncrement(&g_fixBadUnit);
        if (bad >= 20 && g_fixChanged + g_fixKept == 0 && !InterlockedExchange(&g_fixDisabled, 1)) {
            InterlockedExchange(&g_fixOn, 0);
            Log("artfix: DISABLED for this session, the first %ld artillery targets were not found in their "
                "unit's soldier array", bad);
            MessageBeep(MB_ICONHAND);
        }
        return;
    }
    if (centre == r.ent) { InterlockedIncrement(&g_fixKept); }
    else if (Wr32(action + ACTION_OFF_RECORD + 0x0C, centre)) { InterlockedIncrement(&g_fixChanged); }
    else return;
    if (g_fixRows) {
        Vec3 gp = {0, 0, 0}, op = {0, 0, 0}, np = {0, 0, 0};
        RdPos(gun, &gp); RdPos(r.ent, &op); RdPos(centre, &np);
        EnterCriticalSection(&g_cs);
        fprintf(g_fixRows, "%lu,%u,%08X,%08X,%.3f,%.3f,%08X,%u,%.3f,%.3f,%08X,%.3f,%.3f,%08X,%.3f,%.3f\n",
                GetTickCount() - g_t0, GameTick(), action, gun, gp.x, gp.z, unit, count, cx, cz, r.ent, op.x, op.z,
                centre, np.x, np.z);
        LeaveCriticalSection(&g_cs);
    }
}

// Harness only (fixes/artillery-targeting-fix/tests): the fix state and its counters.
extern "C" __declspec(dllexport) void __stdcall ArtFixTestCounters(long* out) {
    out[0] = g_fixOn;
    out[1] = g_fixChanged;
    out[2] = g_fixKept;
    out[3] = g_fixNotSoldier;
    out[4] = g_fixBadUnit;
}

// U toggles the fix on / off at any time (dev build), through the shared key poll thread
// (proxy/keys.h: real key only while the game has the focus). The harness presses the key through
// ArtFixTestKeys instead (bit 0 = U); the game never calls it.
static const int KEY_TOGGLE_VK = 'U';
static volatile LONG g_testKeys = 0;
static bool g_keyWatched = false;
extern "C" __declspec(dllexport) void __stdcall ArtFixTestKeys(LONG bits) {
    InterlockedExchange(&g_testKeys, bits);
}

static void ToggleKey() {
    if (g_fixDisabled) {
        Log("artfix: U refused, the fix is DISABLED for this session (unit check failed)");
        MessageBeep(MB_ICONHAND);
        return;
    }
    LONG on = g_fixOn ? 0 : 1;
    InterlockedExchange(&g_fixOn, on);
    Log("artfix: U, fix %s (so far: moved %ld, already centre %ld)", on ? "ON" : "OFF", g_fixChanged, g_fixKept);
    MessageBeep(on ? MB_ICONASTERISK : MB_OK);
}

// ---------------------------------------------------------------------------
// Handlers and naked detours. The detour sequence is exactly the one given to
// tools/re/stack_calc.py:   pushad, pushfd, mark:ctx, push:ctxptr  (stack_offsets_art.h)
// ---------------------------------------------------------------------------

static void HandleTargetPos(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t ret = *(const uint32_t*)(ctx + ART_OFF_RET);
    uint32_t rva = ret - g_base;
    bool shootAction = rva >= 0x3D3000 && rva < 0x3D5000;
    bool launchCheck = rva == 0x420D16;   // FUN_10420d00 (called by FUN_103d4320 with action+0x2C)
    if (!shootAction && !launchCheck) return;
    uint32_t rec = *(const uint32_t*)(ctx + ART_OFF_ECX);
    if (g_watch) WatchAdd(rec + 0x0C);
    ProbeCall(rec, rva);
}

// FUN_1045a8a0 (__thiscall, ECX = shooter entity, arg1 flags, arg2 = source target record)
static void HandleSetTarget(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t shooter = *(const uint32_t*)(ctx + ART_OFF_ECX);
    uint32_t src = *(const uint32_t*)(ctx + ART_OFF_ARG2);
    uint32_t flags = *(const uint32_t*)(ctx + ART_OFF_ARG1);
    uint32_t esi = *(const uint32_t*)(ctx + ART_OFF_ESI);
    uint32_t ret = *(const uint32_t*)(ctx + ART_OFF_RET);
    uint32_t espEntry = *(const uint32_t*)(ctx + ART_OFF_ESP_BEFORE_PUSHAD);
    struct { float ox, oy, oz; uint32_t ent, obj, xform; } r;
    if (!Rd(src, &r, sizeof(r))) return;
    Vec3 sp = {0, 0, 0}, ep = {0, 0, 0};
    RdPos(shooter, &sp);
    bool hasEnt = Ptr(r.ent) && RdPos(r.ent, &ep);
    uint32_t vt = 0;
    Rd32(esi, &vt);
    char stack[200];
    int len = 0, found = 0;
    stack[0] = 0;
    for (uint32_t k = 4; k < 0x400 && found < 10; k += 4) {
        uint32_t v;
        if (!Rd32(espEntry + k, &v)) break;
        if (v - g_base - 0x1000 < 0x159FC3E) {
            len += snprintf(stack + len, sizeof(stack) - len, "%s%06X", found ? " " : "", v - g_base);
            found++;
        }
    }
    EnterCriticalSection(&g_cs);
    if (g_rows < MAX_ROWS) {
        g_rows++;
        uint32_t unit = 0, count = 0, arr = 0;
        int index = -1;
        float cx = 0, cz = 0;
        if (hasEnt) unit = UnitOf(r.ent, &count, &arr, &index);
        if (unit) {
            int n = 0;
            for (uint32_t i = 0; i < count; i++) {
                uint32_t e;
                Vec3 p;
                if (!Rd32(arr + 4 * i, &e) || !RdPos(e, &p)) continue;
                cx += p.x; cz += p.z; n++;
            }
            if (n) { cx /= n; cz /= n; }
        }
        static uint32_t s_described[16];
        static int s_nDescribed = 0;
        bool known = false;
        for (int i = 0; i < s_nDescribed; i++) known |= s_described[i] == shooter;
        if (!known && s_nDescribed < 16) {
            // one-time description of each shooter: class, unit link, the engine's artillery test inputs
            s_described[s_nDescribed++] = shooter;
            uint32_t svt = 0, su = 0, sc = 0, sa = 0, t2c = 0, t1c = 0, l2ac = 0, l10 = 0, slot = 0xFFFF;
            int sidx = -1;
            Rd32(shooter, &svt);
            for (uint32_t k = 0; k < 0x1000; k += 4) {
                uint32_t v;
                if (Rd32(svt + k, &v) && v == g_base + 0x45A2E0) { slot = k; break; }
            }
            Rd32(shooter + 0x3B4, &su);
            bool inUnit = Ptr(su) && UnitHolds(su, shooter, &sc, &sa, &sidx);
            if (Ptr(su) && Rd32(su + 0x2C, &t2c)) Rd32(t2c + 0x1C, &t1c);
            if (Ptr(su) && Rd32(su + 0x2AC, &l2ac)) Rd32(l2ac + 0x10, &l10);
            uint32_t tt2c = 0, tt1c = 0, tl2ac = 0, tl10 = 0;
            if (unit && Rd32(unit + 0x2C, &tt2c)) Rd32(tt2c + 0x1C, &tt1c);
            if (unit && Rd32(unit + 0x2AC, &tl2ac)) Rd32(tl2ac + 0x10, &tl10);
            uint32_t tvt = 0, srec = 0, trec = 0;
            Rd32(r.ent, &tvt);
            Rd32(su + 0x510, &srec);
            Rd32(unit + 0x510, &trec);
            Log("artfix: shooter 0x%08X vt RVA 0x%06X, fire-order slot +0x%X; [s+0x3B4] = 0x%08X holds it: %d "
                "(index %d of %u); its unit type [[u+0x2C]+0x1C] = %u, [[u+0x2AC]+0x10] = %u; target unit "
                "0x%08X type %u, list %u; target entity vt RVA 0x%06X; unit records 0x%08X / 0x%08X",
                shooter, svt - g_base, slot, su, inUnit, sidx, sc, t1c, l10, unit, tt1c, tl10,
                tvt ? tvt - g_base : 0, srec, trec);
            if (s_nDescribed == 1) {
                // raw words of both units and both unit records, for an offline diff
                char path[MAX_PATH];
                snprintf(path, sizeof(path), "%saim_unitdiff.csv", g_dir);
                FILE* f = fopen(path, "w");
                if (f) {
                    fprintf(f, "off,shooter_unit,target_unit,shooter_rec,target_rec\n");
                    for (uint32_t k = 0; k < 0x1800; k += 4) {
                        uint32_t a = 0, b = 0, c = 0, d = 0;
                        Rd32(su + k, &a); Rd32(unit + k, &b); Rd32(srec + k, &c); Rd32(trec + k, &d);
                        fprintf(f, "%04X,%08X,%08X,%08X,%08X\n", k, a, b, c, d);
                    }
                    fclose(f);
                }
            }
        }
        DWORD ms = GetTickCount() - g_t0;
        fprintf(g_sets,"%lu,%u,%lu,%06X,%08X,%.3f,%.3f,%.3f,%08X,%.3f,%.3f,%.3f,%08X,%08X,%08X,%.3f,%.3f,"
                        "%08X,%u,%d,%.3f,%.3f,%08X,%08X,%06X,%s\n",
                ms, GameTick(), GetCurrentThreadId(), ret - g_base, shooter, sp.x, sp.y, sp.z, src, r.ox, r.oy,
                r.oz, r.ent, r.obj, r.xform, ep.x, ep.z, unit, count, index, cx, cz, flags, esi,
                vt ? vt - g_base : 0, stack);
    }
    LeaveCriticalSection(&g_cs);
}

extern "C" void __stdcall ArtHandler0(const uint8_t* ctx) { HandleTargetPos(ctx); }
extern "C" void __stdcall ArtHandler1(const uint8_t* ctx) { HandleSetTarget(ctx); }
extern "C" void __stdcall ArtHandler2(const uint8_t* ctx) { HandleOrderCopy(ctx); }

HOOK_DETOUR(ArtDetour0, ArtHandler0, g_artTrampolines, 0)
HOOK_DETOUR(ArtDetour1, ArtHandler1, g_artTrampolines, 1)
HOOK_DETOUR(ArtDetour2, ArtHandler2, g_artTrampolines, 2)

static const HookDetour g_detours[HOOK_COUNT] = { ArtDetour0, ArtDetour1, ArtDetour2 };

// ---------------------------------------------------------------------------
// Install / remove
// ---------------------------------------------------------------------------

void ArtFix_Install() {
    Log("artfix: artillery aims at the centre of the target unit (dev build)");
    Mem_Init();
    uint32_t size = 0;
    uint8_t* base = Hook_GameModule(false, &size);
    if (!base) { Log("artfix: REFUSED, empire.retail.dll is not loaded"); return; }
    g_base = (uint32_t)(uintptr_t)base;
    if (size != HOOK_ANALYSED_IMAGE_SIZE) {
        Log("artfix: REFUSED, SizeOfImage 0x%08X is not the analysed build", size);
        return;
    }
    for (int h = 0; h < HOOK_COUNT; h++) {
        if (!Hook_SiteMatches(base, g_hookDefs[h])) {
            Log("artfix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", g_hookDefs[h].rva,
                g_hookDefs[h].func);
            return;
        }
    }
    InitializeCriticalSection(&g_cs);
    g_t0 = GetTickCount();
    OpenProbe();
    if (g_probe) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%saim_fix.csv", g_dir);
        g_fixRows = fopen(path, "w");
        if (g_fixRows) {
            setvbuf(g_fixRows, nullptr, _IOFBF, 1 << 16);
            fprintf(g_fixRows, "ms,tick,action,gun,gun_x,gun_z,unit,count,cen_x,cen_z,old_ent,old_x,old_z,"
                               "new_ent,new_x,new_z\n");
        }
    }
    bool disabled = EnvIs1("ARTFIX_OFF");
    InterlockedExchange(&g_fixOn, disabled ? 0 : 1);
    Log("artfix: fix %s", disabled ? "OFF (ARTFIX_OFF=1), hooks still installed" : "ON");
    bool use[HOOK_COUNT];
    for (int h = 0; h < HOOK_COUNT; h++) use[h] = !g_hookDefs[h].probeOnly || g_probe;
    if (!Hook_Prepare(base, g_hookDefs, HOOK_COUNT, use, g_artTrampolines, g_slots)) {
        Log("artfix: REFUSED, no trampoline memory");
        return;
    }
    Hook_Patch(g_detours, g_slots, HOOK_COUNT);
    for (int h = 0; h < HOOK_COUNT; h++)
        if (g_slots[h].active)
            Log("artfix: hook %s (%s) installed at RVA 0x%06X", g_hookDefs[h].func, g_hookDefs[h].role,
                g_hookDefs[h].rva);
    g_keyWatched = Keys_Watch(KEY_TOGGLE_VK, &g_testKeys, 1, ToggleKey);
    Log("artfix: U toggles the fix on / off%s", g_keyWatched ? "" : " (ERROR: key poll not started)");
}

void ArtFix_Remove() {
    InterlockedExchange(&g_quit, 1);
    if (g_base)
        Log("artfix: artillery targets moved to the centre soldier %ld, already centre %ld, not a soldier %ld, "
            "unit check failed %ld", g_fixChanged, g_fixKept, g_fixNotSoldier, g_fixBadUnit);
    if (g_watchThread) { WaitForSingleObject(g_watchThread, 1000); CloseHandle(g_watchThread); }
    if (g_keyWatched) { Keys_Unwatch(ToggleKey); g_keyWatched = false; }
    if (g_veh) RemoveVectoredExceptionHandler(g_veh);
    Hook_Remove(g_slots, HOOK_COUNT);
    if (g_probe) {
        EnterCriticalSection(&g_cs);
        fclose(g_calls);
        fclose(g_units);
        if (g_watch) fclose(g_watch);
        fclose(g_sets);
        if (g_fixRows) fclose(g_fixRows);
        g_fixRows = nullptr;
        g_watch = nullptr;
        g_probe = false;
        LeaveCriticalSection(&g_cs);
        Log("artfix: probe closed, %u rows", g_rows);
    }
}
