// siegefix_test.cpp -- offline harness for the siege responsiveness fix
// (fixes/siege-responsiveness-fix/src/siegefix.cpp).
//
// Maps the real empire.retail.dll (no game process), patches return stubs right after the stolen
// bytes of the hooked sites, loads the proxy and enters those sites on fake game objects: a
// pathfinder with a request queue, entities of two sides, path requests. The battle clock is
// faked through the same pointer path the proxy reads.
//
//   siegefix_test <game dll> <proxy> <tag> fix|off|nosync|budget|range|corrupt|probe
//
//   fix      defaults: the node budget immediate is 2000, the queue gets its own budget (counter
//            zeroed where the queue is reached), the incremental cap untouched; a
//            queued request of a soldier of the local player's army (world +0x281E4, army unit
//            lists, group +0x510) from the movement state machine is made
//            immediate, other sides, other callers, immediate requests and unknown entities are
//            left alone; J switches both parts off and on
//   off      SIEGEFIX_OFF=1: the game's 500 and no request changed until J
//   nosync   SIEGEFIX_PLAYER_SYNC=0 SIEGEFIX_QUEUE_OWN=0: budget raised, requests left alone, no queue hook
//   budget   SIEGEFIX_BUDGET=1234 SIEGEFIX_INCR=300: both immediates set, restored at unload
//   range    SIEGEFIX_BUDGET=50 SIEGEFIX_INCR=100000: out of range, the defaults are used
//   corrupt  one byte of the budget compare changed before the proxy loads: nothing installed
//   probe    SIEGEFIX_PROBE=1 SIEGEFIX_OFF=1: a tick of the pathfinder update with queries,
//            queued requests and one served request; the CSV files
//
// Output: one PASS / FAIL line per check, then "RESULT <n> passed, <m> failed".

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

typedef HRESULT (WINAPI *DI8Create)(HINSTANCE, DWORD, const void*, void**, void*);
typedef void (__stdcall *TestCounters)(long*);
typedef void (__stdcall *TestKeys)(LONG);

static uint8_t* g_base = nullptr;
static int g_pass = 0, g_fail = 0;
static TestCounters g_counters = nullptr;
static TestKeys g_keys = nullptr;

#define CHECK(cond, name) do { if (cond) { g_pass++; printf("PASS %s\n", name); } \
                               else { g_fail++; printf("FAIL %s\n", name); } } while (0)

struct Site { uint32_t rva; int origLen; uint8_t orig[8]; uint8_t stub[8]; int stubLen; bool probe; };
enum { S_UPDATE = 0, S_INCR_END, S_SEARCH, S_SEARCHED, S_DRAIN_END, S_QUERY, S_ENQUEUE, SITES };
static const Site kSites[SITES] = {
    { 0x3AAC30, 6, { 0x81, 0xEC, 0x9C, 0x00, 0x00, 0x00 }, { 0x81, 0xC4, 0x9C, 0x00, 0x00, 0x00, 0xC3 }, 7, true },  // add esp,0x9c; ret
    { 0x3AAE18, 6, { 0x8B, 0xBE, 0x78, 0x01, 0x00, 0x00 }, { 0xC3 }, 1, false },                                        // ret (entered by CALL)
    { 0x39A500, 5, { 0x83, 0xEC, 0x40, 0x53, 0x55 }, { 0x5D, 0x5B, 0x83, 0xC4, 0x40, 0xC2, 0x04, 0x00 }, 8, true },  // pop ebp/ebx; add esp,0x40; ret 4
    { 0x3AAEBB, 7, { 0x88, 0x47, 0x21, 0xC6, 0x47, 0x20, 0x01 }, { 0xC3 }, 1, true },
    { 0x3AAF03, 6, { 0x8B, 0x86, 0x8C, 0x01, 0x00, 0x00 }, { 0xC3 }, 1, true },
    { 0x385FD0, 5, { 0x83, 0xEC, 0x40, 0x53, 0x55 }, { 0x5D, 0x5B, 0x83, 0xC4, 0x40, 0xC2, 0x04, 0x00 }, 8, false },
    { 0x35FB40, 5, { 0x8B, 0x44, 0x24, 0x04, 0x56 }, { 0x5E, 0xC2, 0x18, 0x00 }, 4, true },                           // pop esi; ret 0x18
};
// The two immediate patches: instruction start, bytes, immediate offset, game value.
struct Imm { uint32_t rva; uint8_t orig[12]; int immOff; uint32_t value; };
static const Imm kImm[2] = {
    { 0x3AAEA4, { 0x81, 0xBE, 0x70, 0x04, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00, 0x73, 0x53 }, 6, 500 },
    { 0x3AACC1, { 0x81, 0xBE, 0x68, 0x03, 0x00, 0x00, 0xC8, 0x00, 0x00, 0x00, 0x0F, 0x83 }, 6, 200 },
};
static const uint32_t RVA_REPLAN_CALL = 0x3DF508;   // the game's CALL FUN_10385fd0 in FUN_103df3f0
static const uint32_t RVA_REPLAN_RET = 0x3DF50D;    // its return address: a RET is placed there

static void Patch(uint8_t* at, const void* bytes, size_t n) {
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
}

static uint8_t* Zalloc(size_t n) { return (uint8_t*)calloc(1, n); }
static void Put32(uint8_t* p, uint32_t off, uint32_t v) { memcpy(p + off, &v, 4); }
static uint32_t Get32(const uint8_t* p, uint32_t off) { uint32_t v; memcpy(&v, p + off, 4); return v; }
static uint32_t Addr(const void* p) { return (uint32_t)(uintptr_t)p; }
static uint32_t Imm32(int i) { return Get32(g_base, kImm[i].rva + kImm[i].immOff); }
static bool ImmRest(int i) {   // every byte but the immediate is the game's
    for (int k = 0; k < 12; k++)
        if ((k < kImm[i].immOff || k >= kImm[i].immOff + 4) && g_base[kImm[i].rva + k] != kImm[i].orig[k]) return false;
    return true;
}

// --- fake battle clock: [base+0x017C755C] -> +0x50 -> +0x00 -> +0x1C -> +0x50 = tick -----------
static uint8_t* g_tickObj = nullptr;
static void SetTick(uint32_t t) { Put32(g_tickObj, 0x50, t); }
static void MakeClock() {
    uint8_t* shell = Zalloc(0x100);
    uint8_t* mode = Zalloc(0x100);
    uint8_t* owner = Zalloc(0x100);
    g_tickObj = Zalloc(0x100);
    Put32(shell, 0x50, Addr(mode));
    Put32(mode, 0x00, Addr(owner));
    Put32(owner, 0x1C, Addr(g_tickObj));
    uint32_t p = Addr(shell);
    Patch(g_base + 0x017C755C, &p, 4);
}

// --- fake pathfinder, entities and requests -------------------------------------------------------
static uint8_t* g_P = nullptr;
static uint8_t* g_world = nullptr;
// A battle of two alliances with one army each; the world names army 0 of alliance 0 as the local
// player's (world +0x281E4 -> pairs (army, alliance), count +0x281E0).
static uint8_t* g_armyUnits[2][4] = {};
static void MakeWorld() {
    g_P = Zalloc(0x800);
    g_world = Zalloc(0x28400);
    uint8_t* mid = Zalloc(0x100);
    Put32(g_world, 8, Addr(mid));
    Put32(mid, 0xB8, Addr(g_P));
    Put32(g_P, 0, Addr(g_world));
    uint8_t* battle = Zalloc(0x100);
    Put32(mid, 0xB4, Addr(battle));
    uint32_t* alls = (uint32_t*)Zalloc(8);
    Put32(battle, 0x88, 2);
    Put32(battle, 0x8C, Addr(alls));
    for (int a = 0; a < 2; a++) {
        uint8_t* al = Zalloc(0x100);
        uint8_t* army = Zalloc(0x400);
        uint32_t* armies = (uint32_t*)Zalloc(4);
        uint32_t* units = (uint32_t*)Zalloc(16);
        armies[0] = Addr(army);
        Put32(al, 0x10, (uint32_t)a);
        Put32(al, 0x20, 1);
        Put32(al, 0x24, Addr(armies));
        for (int u = 0; u < 4; u++) { g_armyUnits[a][u] = Zalloc(0x100); units[u] = Addr(g_armyUnits[a][u]); }
        Put32(army, 0x114, 4);
        Put32(army, 0x118, Addr(units));
        alls[a] = Addr(al);
    }
    uint32_t* pairs = (uint32_t*)Zalloc(8);
    pairs[0] = 0;   // army index
    pairs[1] = 0;   // alliance index
    Put32(g_world, 0x281E4, Addr(pairs));
    Put32(g_world, 0x281E0, 1);
}
// The local player known (1) or not (0); the fix reads it at most every 200 ms.
static void SetLocal(uint32_t n) { Put32(g_world, 0x281E0, n); Sleep(250); }
static const uint32_t LOCAL = 0, ENEMY = 1;
// A soldier of alliance `side`: its group (soldier +0x3B4) names an army unit at +0x510.
static uint8_t* MakeEntity(uint32_t side, bool linked = true) {
    static int k = 0;
    uint8_t* e = Zalloc(0x500);
    Put32(e, 0x28, linked ? Addr(g_world) : Addr(Zalloc(0x100)));
    uint8_t* group = Zalloc(0x600);
    Put32(group, 0x510, Addr(g_armyUnits[side & 1][(k++) & 3]));
    Put32(e, 0x3B4, Addr(group));
    return e;
}
static uint8_t* MakeRequest(uint32_t mode) {
    uint8_t* q = Zalloc(0x48);
    Put32(q, 0x30, mode);
    float w = 1.2f;
    memcpy(q + 0x2C, &w, 4);
    return q;
}
static uint8_t* MakeR(uint32_t mode) {   // a path result with its request copy at +0x24
    uint8_t* r = Zalloc(0x100);
    Put32(r, 0x54, mode);
    return r;
}

// --- entering the hooked sites ---------------------------------------------------------------------
static uint32_t g_replanCall = 0;
// [esp] = return, [esp+4] = request; ECX = pathfinder, EBX = entity. Jumps to the game's own CALL
// in the movement state machine, so the query sees its real return address; the RET placed there
// returns here with the request still on the stack (the caller removes it).
__declspec(naked) static void ReplanThunk() {
    __asm {
        push dword ptr [esp + 4]
        jmp dword ptr [g_replanCall]
    }
}

static void CallQuery(uint8_t* e, uint8_t* q, bool replan) {
    uint32_t P = Addr(g_P), Q = Addr(q), E = Addr(e), fn = Addr(g_base + 0x385FD0);
    if (replan) {
        __asm {
            pushad
            mov ecx, P
            mov ebx, E
            push Q
            call ReplanThunk
            add esp, 4
            popad
        }
    } else {
        __asm {
            pushad
            mov ecx, P
            mov ebx, E
            push Q
            call fn
            popad
        }
    }
}

static void CallEnqueue(uint8_t* r, uint8_t* q) {
    uint32_t P = Addr(g_P), Q = Addr(q), R = Addr(r), fn = Addr(g_base + 0x35FB40);
    __asm {
        pushad
        mov ecx, R
        push 0
        push 0
        push Q
        push 0
        push 0
        push P
        call fn
        popad
    }
}

static void CallUpdate() {
    uint32_t P = Addr(g_P), fn = Addr(g_base + 0x3AAC30);
    __asm { pushad
            mov ecx, P
            call fn
            popad }
}

static void CallSearch(uint8_t* r) {
    uint32_t P = Addr(g_P), R = Addr(r), fn = Addr(g_base + 0x39A500);
    __asm { pushad
            mov ecx, P
            push R
            call fn
            popad }
}

// A mid-function site, entered by CALL: the stolen instructions run, then the RET placed after them.
static void CallMid(uint32_t rva, uint32_t aEsi, uint32_t aEdi, uint32_t aEbx, uint32_t aEax) {
    uint32_t fn = Addr(g_base + rva);
    __asm {
        pushad
        mov esi, aEsi
        mov edi, aEdi
        mov ebx, aEbx
        mov eax, aEax
        call fn
        popad
    }
}

static long C(int i) { long c[32] = {}; g_counters(c); return c[i]; }
enum { C_PROBE = 0, C_FIXON, C_BUDGET, C_INCR, C_SYNC, C_SYNCED, C_NOTPLAYER, C_BADENTITY, C_PATCHFAIL,
       C_TICKS, C_QUERIES, C_QUEUED, C_SERVED, C_UNKNOWNR, C_INSTALLED, C_LOCALARMIES, C_LOCALREADS, C_OWN,
       C_OWNRESETS };

static void PressJ() { g_keys(1); Sleep(200); g_keys(0); Sleep(200); }

static bool Patched(int s) { return g_base[kSites[s].rva] == 0xE9; }

// --- scenes -----------------------------------------------------------------------------------------


// Part B: which requests are made immediate. `on` = the fix is on and makes requests immediate.
static void QueryScene(bool on, const char* tag) {
    char name[128];
    long synced0 = C(C_SYNCED);
    uint8_t* me = MakeEntity(LOCAL);
    uint8_t* q = MakeRequest(1);
    CallQuery(me, q, true);
    snprintf(name, sizeof(name), "%s B1 local side, mode 1, movement state machine: %s", tag,
             on ? "made immediate (mode 0)" : "left queued");
    CHECK(Get32(q, 0x30) == (on ? 0u : 1u), name);
    q = MakeRequest(2);
    CallQuery(me, q, true);
    snprintf(name, sizeof(name), "%s B2 local side, mode 2: %s", tag, on ? "made immediate" : "left queued");
    CHECK(Get32(q, 0x30) == (on ? 0u : 2u), name);
    snprintf(name, sizeof(name), "%s B2b counted", tag);
    CHECK(C(C_SYNCED) - synced0 == (on ? 2 : 0), name);
    synced0 = C(C_SYNCED);
    q = MakeRequest(1);
    CallQuery(MakeEntity(ENEMY), q, true);
    snprintf(name, sizeof(name), "%s B3 other side: left queued", tag);
    CHECK(Get32(q, 0x30) == 1, name);
    q = MakeRequest(1);
    CallQuery(me, q, false);
    snprintf(name, sizeof(name), "%s B4 another caller: left queued", tag);
    CHECK(Get32(q, 0x30) == 1, name);
    q = MakeRequest(0);
    CallQuery(me, q, true);
    snprintf(name, sizeof(name), "%s B5 an immediate request stays immediate", tag);
    CHECK(Get32(q, 0x30) == 0, name);
    long bad0 = C(C_BADENTITY);
    q = MakeRequest(1);
    CallQuery(MakeEntity(LOCAL, false), q, true);
    snprintf(name, sizeof(name), "%s B6 an entity of another pathfinder: left queued, counted", tag);
    CHECK(Get32(q, 0x30) == 1 && C(C_BADENTITY) == bad0 + 1, name);
    SetLocal(0);
    q = MakeRequest(1);
    CallQuery(MakeEntity(LOCAL), q, true);
    snprintf(name, sizeof(name), "%s B7 no local player in the world: left queued", tag);
    CHECK(Get32(q, 0x30) == 1, name);
    SetLocal(1);
    snprintf(name, sizeof(name), "%s B8 nothing else made immediate", tag);
    CHECK(C(C_SYNCED) == synced0, name);
}

static void SitesScene(bool probe) {
    char name[96];
    for (int s = 0; s < SITES; s++) {
        bool want = !kSites[s].probe || probe;
        snprintf(name, sizeof(name), "I%d site RVA 0x%X %s", s + 1, kSites[s].rva, want ? "patched" : "untouched");
        CHECK(Patched(s) == want, name);
    }
}

// Part A, the queue's own budget: entering the queue zeroes the node counter (fix on) or not.
static void OwnScene(bool on, const char* tag) {
    char name[128];
    long r0 = C(C_OWNRESETS);
    Put32(g_P, 0x470, 300);
    CallMid(0x3AAE18, Addr(g_P), 0, 0, 0);
    snprintf(name, sizeof(name), "%s A1 node counter at the queue: %s", tag, on ? "zeroed (300 -> 0)" : "left (300)");
    CHECK(Get32(g_P, 0x470) == (on ? 0u : 300u) && C(C_OWNRESETS) - r0 == (on ? 1 : 0), name);
}

static void RunFix() {
    CHECK(C(C_INSTALLED) == 1 && C(C_FIXON) == 1, "X1 installed, fix on at launch");
    CHECK(C(C_OWN) == 1, "X5 the queue's own budget by default");
    OwnScene(true, "on");
    CHECK(Imm32(0) == 2000 && ImmRest(0), "X2 node budget immediate 500 -> 2000, rest of the CMP / JAE unchanged");
    CHECK(Imm32(1) == 200 && ImmRest(1), "X3 incremental cap untouched (200) by default");
    CHECK(C(C_SYNC) == 1, "X4 player requests made immediate by default");
    SitesScene(false);
    QueryScene(true, "on");
    PressJ();
    CHECK(C(C_FIXON) == 0 && Imm32(0) == 500 && ImmRest(0), "K1 J: fix off, the game's 500 back");
    QueryScene(false, "J-off");
    OwnScene(false, "J-off");
    PressJ();
    CHECK(C(C_FIXON) == 1 && Imm32(0) == 2000, "K2 J again: fix on, 2000");
    uint8_t* q = MakeRequest(1);
    CallQuery(MakeEntity(LOCAL), q, true);
    CHECK(Get32(q, 0x30) == 0, "K3 ... and the local side's requests are immediate again");
    CHECK(C(C_PATCHFAIL) == 0, "K4 no patch failures");
}

static void RunOff() {
    CHECK(C(C_INSTALLED) == 1 && C(C_FIXON) == 0, "X1 installed, fix off with SIEGEFIX_OFF=1");
    CHECK(Imm32(0) == 500 && ImmRest(0), "X2 node budget left at 500");
    SitesScene(false);
    QueryScene(false, "off");
    OwnScene(false, "off");
    PressJ();
    CHECK(C(C_FIXON) == 1 && Imm32(0) == 2000 && ImmRest(0), "K1 J: fix on, 2000");
    uint8_t* q = MakeRequest(1);
    CallQuery(MakeEntity(LOCAL), q, true);
    CHECK(Get32(q, 0x30) == 0, "K2 ... local side's requests immediate");
}

static void RunNoSync() {
    CHECK(C(C_FIXON) == 1 && C(C_SYNC) == 0 && C(C_OWN) == 0, "X1 fix on, SIEGEFIX_PLAYER_SYNC=0 SIEGEFIX_QUEUE_OWN=0");
    CHECK(Imm32(0) == 2000 && ImmRest(0), "X2 node budget 2000");
    CHECK(!Patched(S_INCR_END), "X3 queue hook not installed without the own budget");
    QueryScene(false, "nosync");
}

static void RunBudget() {
    CHECK(C(C_BUDGET) == 1234 && Imm32(0) == 1234 && ImmRest(0), "X1 SIEGEFIX_BUDGET=1234 written");
    CHECK(C(C_INCR) == 300 && Imm32(1) == 300 && ImmRest(1), "X2 SIEGEFIX_INCR=300 written");
    PressJ();
    CHECK(Imm32(0) == 500 && Imm32(1) == 200, "K1 J: both back to the game's values");
    PressJ();
    CHECK(Imm32(0) == 1234 && Imm32(1) == 300, "K2 J again: both set");
}

static void RunRange() {
    CHECK(C(C_BUDGET) == 2000 && Imm32(0) == 2000, "X1 SIEGEFIX_BUDGET=50 out of range: 2000 used");
    CHECK(C(C_INCR) == 200 && Imm32(1) == 200 && ImmRest(1), "X2 SIEGEFIX_INCR=100000 out of range: 200 kept");
}

static void RunCorrupt() {
    CHECK(C(C_INSTALLED) == 0 && C(C_FIXON) == 0, "X1 a changed byte in the budget compare: not installed");
    bool none = true;
    for (int s = 0; s < SITES; s++) none &= !Patched(s);
    CHECK(none, "X2 no site patched");
    CHECK(Imm32(0) == 500 && Imm32(1) == 200, "X3 immediates untouched");
}

static int Lines(const char* dir, const char* name) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", dir, name);
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    int n = 0, ch;
    while ((ch = fgetc(f)) != EOF) if (ch == '\n') n++;
    fclose(f);
    return n;
}

// Field `col` (0-based) of the first data line (header skipped) starting with `kind`.
static bool Field(const char* dir, const char* name, const char* kind, int col, char* out, size_t cap) {
    char path[MAX_PATH], line[2048];
    snprintf(path, sizeof(path), "%s%s", dir, name);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    bool found = false, header = true;
    while (fgets(line, sizeof(line), f)) {
        if (header) { header = false; continue; }
        if (strncmp(line, kind, strlen(kind)) != 0) continue;
        char* p = line;
        for (int c = 0; c < col && p; c++) { p = strchr(p, ','); if (p) p++; }
        if (!p) break;
        size_t n = strcspn(p, ",\r\n");
        if (n >= cap) n = cap - 1;
        memcpy(out, p, n);
        out[n] = 0;
        found = true;
        break;
    }
    fclose(f);
    return found;
}

static bool LatestRun(char* out) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("siegefix_runs\\*", &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    char best[MAX_PATH] = "";
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.' && strcmp(fd.cFileName, best) > 0)
            strcpy(best, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (!best[0]) return false;
    sprintf(out, "siegefix_runs\\%s\\", best);
    return true;
}

static void RunProbe(HMODULE proxy) {
    CHECK(C(C_PROBE) == 1 && C(C_FIXON) == 0, "P0 probe on, fix off");
    SitesScene(true);
    // a queue of two requests, the first of them the last mode-1 request
    uint8_t* r1 = MakeR(1);
    uint8_t* r2 = MakeR(2);
    Put32(r1, 0x1C, Addr(r2));
    Put32(g_P, 0x148, Addr(r1));
    Put32(g_P, 0x14C, Addr(r2));
    Put32(g_P, 0x150, Addr(r1));
    SetTick(100);
    CallUpdate();
    // the local side's queued request, an immediate one, and the other side's queued request
    uint8_t* me = MakeEntity(LOCAL);
    uint8_t* q = MakeRequest(1);
    CallQuery(me, q, true);
    CHECK(Get32(q, 0x30) == 1, "P1 fix off: the request stays queued");
    uint8_t* rMe = MakeR(1);
    CallEnqueue(rMe, q);
    CallQuery(me, MakeRequest(0), true);
    uint8_t* q2 = MakeRequest(2);
    CallQuery(MakeEntity(ENEMY), q2, true);
    CallEnqueue(MakeR(2), q2);
    CHECK(C(C_QUERIES) == 3 && C(C_QUEUED) == 2, "P2 3 queries, 2 queued requests seen");
    SetTick(105);
    Put32(g_P, 0x368, 50);
    Put32(g_P, 0x470, 300);
    CallMid(0x3AAE18, Addr(g_P), 0, 0, 0);
    CallSearch(rMe);
    Put32(g_P, 0x470, 420);
    CallMid(0x3AAEBB, Addr(g_P), Addr(rMe), 0, 1);
    CHECK(rMe[0x20] == 1 && rMe[0x21] == 1, "P3 the stolen instructions ran: the request is marked served");
    CallMid(0x3AAF03, Addr(g_P), Addr(r1), 1, 0);
    CHECK(C(C_SERVED) == 1 && C(C_UNKNOWNR) == 0 && C(C_TICKS) == 1, "P4 one served request matched, one tick");
    FreeLibrary(proxy);
    char dir[MAX_PATH];
    bool have = LatestRun(dir);
    CHECK(have, "F1 a run folder siegefix_runs\\<stamp>\\ was written");
    if (!have) return;
    CHECK(Lines(dir, "siege_ticks.csv") == 2, "F2 siege_ticks.csv: header + 1 tick");
    CHECK(Lines(dir, "siege_requests.csv") == 5, "F3 siege_requests.csv: header + 2 queued + 1 immediate + 1 served");
    char v[64];
    CHECK(Field(dir, "siege_requests.csv", "queued", 10, v, sizeof(v)) && strcmp(v, "1") == 0, "F4 served row: local side");
    CHECK(Field(dir, "siege_requests.csv", "queued", 5, v, sizeof(v)) && strcmp(v, "3DF50D") == 0, "F5 ... from the movement state machine");
    CHECK(Field(dir, "siege_requests.csv", "queued", 20, v, sizeof(v)) && strcmp(v, "1") == 0, "F6 ... one request ahead of it");
    CHECK(Field(dir, "siege_requests.csv", "queued", 23, v, sizeof(v)) && strcmp(v, "5") == 0, "F7 ... waited 5 ticks");
    CHECK(Field(dir, "siege_requests.csv", "queued", 26, v, sizeof(v)) && strcmp(v, "120") == 0, "F8 ... 120 nodes");
    CHECK(Field(dir, "siege_requests.csv", "sync", 10, v, sizeof(v)) && strcmp(v, "1") == 0, "F9 immediate row of the local side");
    CHECK(Field(dir, "siege_ticks.csv", "", 3, v, sizeof(v)) && strcmp(v, "2") == 0, "F10 tick row: 2 requests queued at the start");
    CHECK(Field(dir, "siege_ticks.csv", "", 9, v, sizeof(v)) && strcmp(v, "1") == 0, "F11 ... 1 served");
    CHECK(Field(dir, "siege_ticks.csv", "", 10, v, sizeof(v)) && strcmp(v, "120") == 0, "F12 ... 120 nodes in the queue");
    CHECK(Field(dir, "siege_ticks.csv", "", 11, v, sizeof(v)) && strcmp(v, "1") == 0, "F13 ... budget hit with a backlog");
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc < 5) { printf("usage: siegefix_test <game dll> <proxy> <tag> fix|off|nosync|budget|range|corrupt|probe\n"); return 2; }
    const char* mode = argv[4];
    bool probe = strcmp(mode, "probe") == 0;
    bool off = strcmp(mode, "off") == 0;
    bool nosync = strcmp(mode, "nosync") == 0;
    bool budget = strcmp(mode, "budget") == 0;
    bool range = strcmp(mode, "range") == 0;
    bool corrupt = strcmp(mode, "corrupt") == 0;
    SetEnvironmentVariableA("SIEGEFIX_PROBE", probe ? "1" : nullptr);
    SetEnvironmentVariableA("SIEGEFIX_OFF", off || probe ? "1" : nullptr);
    SetEnvironmentVariableA("SIEGEFIX_PLAYER_SYNC", nosync ? "0" : nullptr);
    SetEnvironmentVariableA("SIEGEFIX_QUEUE_OWN", nosync ? "0" : nullptr);
    SetEnvironmentVariableA("SIEGEFIX_BUDGET", budget ? "1234" : range ? "50" : nullptr);
    SetEnvironmentVariableA("SIEGEFIX_INCR", budget ? "300" : range ? "100000" : nullptr);
    SetEnvironmentVariableA("FBRFIX_PROBE", nullptr);
    SetEnvironmentVariableA("ARTFIX_PROBE", nullptr);
    SetEnvironmentVariableA("PATHFIX_CAPTURE", nullptr);

    HMODULE game = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!game) { printf("cannot map game dll: %lu\n", GetLastError()); return 1; }
    g_base = (uint8_t*)game;
    for (int s = 0; s < SITES; s++) {
        if (memcmp(g_base + kSites[s].rva, kSites[s].orig, kSites[s].origLen) != 0) { printf("site %X differs\n", kSites[s].rva); return 1; }
        Patch(g_base + kSites[s].rva + kSites[s].origLen, kSites[s].stub, kSites[s].stubLen);
    }
    for (int i = 0; i < 2; i++)
        if (memcmp(g_base + kImm[i].rva, kImm[i].orig, 12) != 0) { printf("compare %X differs\n", kImm[i].rva); return 1; }
    static const uint8_t callQuery[5] = { 0xE8, 0xC3, 0x6A, 0xFA, 0xFF };   // CALL FUN_10385fd0
    if (memcmp(g_base + RVA_REPLAN_CALL, callQuery, 5) != 0) { printf("replan call differs\n"); return 1; }
    static const uint8_t ret = 0xC3;
    Patch(g_base + RVA_REPLAN_RET, &ret, 1);
    g_replanCall = Addr(g_base + RVA_REPLAN_CALL);
    uint8_t corruptByte = 0x54;
    if (corrupt) Patch(g_base + kImm[0].rva + 11, &corruptByte, 1);   // JAE +0x53 -> +0x54
    MakeClock();
    MakeWorld();

    HMODULE proxy = LoadLibraryA(argv[2]);
    if (!proxy) { printf("cannot load proxy: %lu\n", GetLastError()); return 1; }
    DI8Create create = (DI8Create)GetProcAddress(proxy, "DirectInput8Create");
    g_counters = (TestCounters)GetProcAddress(proxy, "_SiegeFixTestCounters@4");
    if (!g_counters) g_counters = (TestCounters)GetProcAddress(proxy, "SiegeFixTestCounters");
    g_keys = (TestKeys)GetProcAddress(proxy, "_SiegeFixTestKeys@4");
    if (!g_keys) g_keys = (TestKeys)GetProcAddress(proxy, "SiegeFixTestKeys");
    if (!create || !g_counters || !g_keys) {
        printf("missing export (create %p counters %p keys %p)\n", create, g_counters, g_keys);
        return 1;
    }
    void* out = nullptr;
    static const GUID iidDI8A = {0xBF798030,0x483A,0x4DA2,{0xAA,0x99,0x5D,0x64,0xED,0x36,0x97,0x00}};
    create(GetModuleHandleA(nullptr), 0x0800, &iidDI8A, &out, nullptr);
    printf("TAG %s mode %s\n", argv[3], mode);

    if (probe) {
        RunProbe(proxy);   // unloads the proxy itself
    } else {
        CHECK(C(C_PROBE) == 0, "N1 probe off without SIEGEFIX_PROBE");
        if (off) RunOff();
        else if (nosync) RunNoSync();
        else if (budget) RunBudget();
        else if (range) RunRange();
        else if (corrupt) RunCorrupt();
        else RunFix();
        FreeLibrary(proxy);
    }
    bool restored = true;
    for (int s = 0; s < SITES; s++) restored &= memcmp(g_base + kSites[s].rva, kSites[s].orig, kSites[s].origLen) == 0;
    CHECK(restored, "U1 after unload every hook site holds its original bytes");
    if (corrupt) {
        CHECK(g_base[kImm[0].rva + 11] == corruptByte && Imm32(0) == 500, "U2 the changed compare is left as it was");
    } else {
        CHECK(memcmp(g_base + kImm[0].rva, kImm[0].orig, 12) == 0 && memcmp(g_base + kImm[1].rva, kImm[1].orig, 12) == 0,
              "U2 after unload both compares hold the game's 500 / 200");
    }
    printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
