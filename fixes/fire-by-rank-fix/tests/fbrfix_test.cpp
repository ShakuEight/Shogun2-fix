// fbrfix_test.cpp -- offline harness for the fire-by-rank fix (fixes/fire-by-rank-fix/src/fbrfix.cpp).
//
// Maps the real empire.retail.dll (no game process), patches return stubs right after the stolen
// bytes of the four hooked sites, loads the proxy and enters those sites on fake game objects:
// state machine contexts and named states, a fire-by-rank task, a unit with soldiers, orders.
// The battle clock is faked through the same pointer path the proxy reads.
//
//   fbrfix_test <game dll> <proxy> <tag> fix|off|selfcheck|allranks|firegrace|probe
//
//   fix        the fix on (the default): the game's own group test FUN_1017b980 is entered on a
//              fake group task; the officer outside the shooter class is not waited for, a
//              reloading man is, a man slow to his ready pose only until the grace time; the
//              group order FUN_10188290 skips the officer; Y switches the fix off and on; the
//              fire-completed test FUN_1017b9d0 gives a man who fired his reload order at once
//              and the exit's second order to him is skipped (early reload)
//   off        FBRFIX_OFF=1: the same scenes give the game's own answers until Y
//   selfcheck  20 group tasks with a broken layout switch the fix off for good (Y refused)
//   allranks   FBRFIX_ALL_RANKS=1: a unit 5 ranks deep gets 5 groups before its first volley
//   firegrace  FBRFIX_EARLY_RELOAD=0 FBRFIX_FIRE_GRACE=20: no early reload orders; the
//              fire-completed test FUN_1017b9d0 ends the turn 20 ticks after the group's last shot
//   probe      FBRFIX_PROBE=1: the probe logs transitions of shooting machines only, samples
//              reloads per tick, logs orders of watched units, writes its CSV files
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
typedef void (__fastcall *Runner)(void* ctx);
typedef void (__fastcall *OrderFn)(void* soldier, void* edx, void* order);

static uint8_t* g_base = nullptr;
static int g_pass = 0, g_fail = 0;
static TestCounters g_counters = nullptr;

#define CHECK(cond, name) do { if (cond) { g_pass++; printf("PASS %s\n", name); } \
                               else { g_fail++; printf("FAIL %s\n", name); } } while (0)

struct Site { uint32_t rva; uint8_t orig[5]; uint8_t stub[6]; int stubLen; };
// The four probe sites, then the three fix sites. FUN_1017b980 and FUN_1017b9d0 run whole (no
// stub): they only read the fake objects, so with the fix off the game's own answer is checked.
static const int SITES = 7, PROBE_SITES = 4;
static const Site kSites[SITES] = {
    { 0x1374CF0, { 0x56, 0x8B, 0xF1, 0x57, 0x56 }, { 0x5E, 0x5F, 0x5E, 0xC3 }, 4 },        // pop esi; pop edi; pop esi; ret
    { 0x1374D0B, { 0x89, 0x7E, 0x04, 0x8B, 0xCF }, { 0xC3 }, 1 },                           // ret (entered by CALL)
    { 0x3DB8F0,  { 0x53, 0x56, 0x57, 0x8B, 0xF9 }, { 0x5F, 0x5E, 0x5B, 0xC2, 0x04, 0x00 }, 6 },  // pop edi/esi/ebx; ret 4
    { 0x3DB740,  { 0x53, 0x56, 0x57, 0x8B, 0xF9 }, { 0x5F, 0x5E, 0x5B, 0xC2, 0x04, 0x00 }, 6 },
    { 0x17B980,  { 0x53, 0x56, 0x57, 0x8B, 0xF9 }, { 0 }, 0 },                              // runs whole
    { 0x188290,  { 0x83, 0xEC, 0x50, 0x8B, 0xC1 }, { 0x83, 0xC4, 0x50, 0xC2, 0x08, 0x00 }, 6 },  // add esp,0x50; ret 8
    { 0x17B9D0,  { 0x53, 0x56, 0x57, 0x8B, 0xF1 }, { 0 }, 0 },                              // runs whole
};

static void Patch(uint8_t* at, const void* bytes, size_t n) {
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
}

static uint8_t* Zalloc(size_t n) { return (uint8_t*)calloc(1, n); }
static void Put32(uint8_t* p, uint32_t off, uint32_t v) { memcpy(p + off, &v, 4); }
static void PutF(uint8_t* p, uint32_t off, float v) { memcpy(p + off, &v, 4); }
static uint32_t Addr(const void* p) { return (uint32_t)(uintptr_t)p; }

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

// --- named states: vtable slot +0x10 -> "MOV EAX,[table]; RET", [table] -> name -----------------
static uint8_t* MakeState(const char* name) {
    char* str = (char*)Zalloc(64);
    strcpy(str, name);
    uint32_t* table = (uint32_t*)Zalloc(4);
    *table = Addr(str);
    uint8_t* code = Zalloc(8);
    code[0] = 0xA1;
    Put32(code, 1, Addr(table));
    code[5] = 0xC3;
    uint32_t* vt = (uint32_t*)Zalloc(0x20);
    vt[4] = Addr(code);
    uint8_t* obj = Zalloc(0x10);
    Put32(obj, 0, Addr(vt));
    return obj;
}

// --- a unit of n soldiers and a task whose machine context is at task+0x18 -----------------------
struct Unit { uint8_t* unit; uint32_t* arr; uint8_t* s[64]; int n; };
static Unit MakeUnit(int n) {
    Unit u = {};
    u.n = n;
    u.unit = Zalloc(0x600);
    u.arr = (uint32_t*)calloc(n, 4);
    for (int i = 0; i < n; i++) {
        uint8_t* s = Zalloc(0x400);
        Put32(s, 0, Addr(Zalloc(0x200)));      // some vtable
        PutF(s, 0x48, 10.0f + i);
        PutF(s, 0x50, 20.0f);
        Put32(s, 0x218, (uint32_t)i);          // soldier index
        Put32(s, 0x3B4, Addr(u.unit));
        u.s[i] = s;
        u.arr[i] = Addr(s);
    }
    Put32(u.unit, 0x18, (uint32_t)n);
    Put32(u.unit, 0x1C, Addr(u.arr));
    return u;
}

static uint8_t* MakeTask(const Unit& u, bool volley, int rows) {
    uint8_t* t = Zalloc(0x100);
    Put32(t, 0, volley ? Addr(g_base + 0x15C0564) : Addr(Zalloc(0x100)));
    Put32(t, 0x04, Addr(u.unit));
    if (volley) {
        Put32(t, 0x30, 0);
        Put32(t, 0x34, (uint32_t)rows);
        Put32(t, 0x38, (uint32_t)(u.n / rows));
        uint32_t* table = (uint32_t*)calloc(u.n * 2, 4);
        for (int i = 0; i < u.n; i++) { table[2 * i] = i / (u.n / rows); table[2 * i + 1] = i % (u.n / rows); }
        Put32(t, 0x58, Addr(table));
        Put32(t, 0x5C, (uint32_t)u.n);
    }
    return t;
}

// Enters the transition site with ESI = context and EDI = next state, as the runner does.
static void Transition(uint8_t* ctx, uint8_t* oldState, uint8_t* next) {
    Put32(ctx, 4, Addr(oldState));
    uint32_t c = Addr(ctx), n = Addr(next);
    void* site = g_base + 0x1374D0B;
    __asm {
        push esi
        push edi
        mov esi, c
        mov edi, n
        call site
        pop edi
        pop esi
    }
}

static long C(int i) { long c[32] = {}; g_counters(c); return c[i]; }
enum { C_PROBE = 0, C_FIXON = 7, C_READY = 8, C_GRACE = 9, C_RELOADING = 10, C_WAITING = 11, C_EXCLUDED = 12,
       C_SKIPPED = 13, C_GIVEN = 14, C_BADTASK = 15, C_ALLRANKS = 16, C_EARLY = 17, C_DONE = 18, C_FIRING = 19,
       C_CUT = 20, C_NOTFIRED = 21, C_EXITSKIP = 22 };

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

static bool LatestRun(char* out) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("fbrfix_runs\\*", &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    char best[MAX_PATH] = "";
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.' && strcmp(fd.cFileName, best) > 0)
            strcpy(best, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (!best[0]) return false;
    sprintf(out, "fbrfix_runs\\%s\\", best);
    return true;
}

static void RunProbe(HMODULE proxy) {
    CHECK(C(0) == 1, "P0 probe on with FBRFIX_PROBE=1");
    for (int i = 0; i < SITES; i++) {
        uint8_t* site = g_base + kSites[i].rva;
        int32_t rel;
        memcpy(&rel, site + 1, 4);
        uint8_t* dest = site + 5 + rel;
        bool intoProxy = (uintptr_t)dest >= (uintptr_t)proxy && (uintptr_t)dest < (uintptr_t)proxy + 0x200000;
        char name[96];
        snprintf(name, sizeof(name), "I%d site RVA 0x%X patched: JMP into the proxy", i + 1, kSites[i].rva);
        CHECK(site[0] == 0xE9 && intoProxy, name);
    }

    Unit u = MakeUnit(16);
    uint8_t* task = MakeTask(u, true, 4);
    uint8_t* ctx = task + 0x18;
    uint8_t* sStart = MakeState("START_STATE");
    uint8_t* sWait = MakeState("WAIT_FOR_CURRENT_ROW_LOADED_AND_READY_STATE");
    uint8_t* sAdv = MakeState("CURRENT_ROW_ADVANCE_STATE");

    SetTick(100);
    Transition(ctx, sStart, sWait);
    uint32_t now;
    memcpy(&now, ctx + 4, 4);
    CHECK(now == Addr(sWait), "T1 the stolen instructions ran: the context holds the new state");
    CHECK(C(1) == 1 && C(2) == 1, "T2 a shooting transition is counted and logged");
    CHECK(C(6) == 1, "T3 entering a row state dumps the task's soldiers");

    Unit other = MakeUnit(4);
    uint8_t* otask = MakeTask(other, false, 1);
    Transition(otask + 0x18, MakeState("IDLE_STATE"), MakeState("WALK_STATE"));
    CHECK(C(1) == 2 && C(2) == 1, "T4 a machine without shooting states is counted, not logged");

    Transition(ctx, sWait, sAdv);
    CHECK(C(2) == 2, "T5 later transitions of a watched machine are logged");

    // reload samples through the runner entry: one per tick
    Runner run = (Runner)(void*)(g_base + 0x1374CF0);
    SetTick(101); run(ctx);                                   // baseline
    PutF(u.s[3], 0x390, 2.5f);
    run(ctx);                                                 // same tick: not sampled again
    CHECK(C(4) == 0, "R1 one reload sample per tick");
    SetTick(102); run(ctx);
    CHECK(C(4) == 1, "R2 reload time leaving 0 counts as a shot");
    PutF(u.s[3], 0x390, 0.0f);
    SetTick(103); run(ctx);
    CHECK(C(5) == 1, "R3 reload time back at 0 counts as loaded");
    memcpy(&now, ctx + 4, 4);
    CHECK(now == Addr(sAdv), "R4 the runner entry leaves the machine alone");

    // orders: logged only for soldiers of watched units
    OrderFn orderA = (OrderFn)(void*)(g_base + 0x3DB8F0);
    OrderFn orderB = (OrderFn)(void*)(g_base + 0x3DB740);
    uint8_t* order = Zalloc(0x60);
    Put32(order, 0x20, 5);
    orderA(u.s[2], nullptr, order);
    orderB(u.s[2], nullptr, order);
    CHECK(C(3) == 2, "O1 orders A and B of a watched unit's soldier are logged");
    orderA(other.s[0], nullptr, order);
    CHECK(C(3) == 2, "O2 orders of other units are not logged");

    FreeLibrary(proxy);
    char dir[MAX_PATH];
    bool have = LatestRun(dir);
    CHECK(have, "F1 a run folder fbrfix_runs\\<stamp>\\ was written");
    if (have) {
        CHECK(Lines(dir, "fbr_states.csv") == 3, "F2 fbr_states.csv: header + 2 transitions");
        CHECK(Lines(dir, "fbr_rowdump.csv") == 1 + 2 * 16, "F3 fbr_rowdump.csv: header + 16 soldiers per row state entered (2)");
        CHECK(Lines(dir, "fbr_reload.csv") == 3, "F4 fbr_reload.csv: header + fired + loaded");
        CHECK(Lines(dir, "fbr_orders.csv") == 3, "F5 fbr_orders.csv: header + 2 orders");
        CHECK(Lines(dir, "fbr_units.csv") >= 1 + 16, "F6 fbr_units.csv: positions of the watched unit");
        CHECK(Lines(dir, "fbr_unitinfo.csv") >= 2, "F7 fbr_unitinfo.csv: one line per unit seen");
    }
}

// --- the fix ------------------------------------------------------------------------------------
//
// A unit of 12: soldier 0 is the officer (+0x48C = 0x10), the rest gun men (+0x48C = 8). Three
// groups of four by soldier index (the officer in group 0). The group task: unit at +4, group
// count +0x38, current group +0x40, group per soldier index at +0x48.

typedef uint32_t (__fastcall *GroupTest)(void* task);
typedef void (__fastcall *GroupOrder)(void* task, void* edx, void* soldier, int afterFire);
typedef void (__stdcall *TestKeys)(LONG);
static TestKeys g_keys = nullptr;

struct Group { Unit u; uint8_t* task; uint32_t* table; };

static Group MakeGroup() {
    Group g;
    g.u = MakeUnit(12);
    g.table = (uint32_t*)calloc(64, 4);
    for (int i = 0; i < 12; i++) {
        g.table[i] = i / 4;
        Put32(g.u.s[i], 0x48C, i == 0 ? 0x10 : 8);
    }
    g.task = Zalloc(0x100);
    Put32(g.task, 0, Addr(g_base + 0x15C1048));
    Put32(g.task, 0x04, Addr(g.u.unit));
    Put32(g.task, 0x38, 3);
    Put32(g.task, 0x40, 0);
    Put32(g.task, 0x48, Addr(g.table));
    return g;
}

// Order state as the game's ready test FUN_103cbe40 reads it: an open order (+0x384 = 1) that is
// not done (+0x308 = 0) is not ready; no action object at +0x248.
static void SetReady(uint8_t* s, bool ready) { s[0x384] = ready ? 0 : 1; s[0x308] = 0; Put32(s, 0x248, 0); }
static void SetReload(uint8_t* s, float t) { PutF(s, 0x390, t); }

static uint32_t Test(Group& g) {
    GroupTest f = (GroupTest)(void*)(g_base + 0x17B980);
    return f(g.task) & 0xFF;
}

static void PressY() { g_keys(1); Sleep(200); g_keys(0); Sleep(200); }

static void RunFix(bool on) {
    CHECK(C(C_FIXON) == (on ? 1 : 0), on ? "X0 fix on at launch" : "X0 fix off with FBRFIX_OFF=1");
    for (int i = PROBE_SITES; i < SITES; i++) {
        char name[80];
        snprintf(name, sizeof(name), "X%d fix site RVA 0x%X patched", i - PROBE_SITES + 1, kSites[i].rva);
        CHECK(g_base[kSites[i].rva] == 0xE9, name);
    }
    bool probeUntouched = true;
    for (int i = 0; i < PROBE_SITES; i++) probeUntouched &= memcmp(g_base + kSites[i].rva, kSites[i].orig, 5) == 0;
    CHECK(probeUntouched, "X9 probe sites untouched without FBRFIX_PROBE");

    Group g = MakeGroup();
    uint32_t tick = 200;
    SetTick(tick);
    // 1: every gun man ready and loaded, the officer stuck in an order he cannot finish (the clan bug)
    for (int i = 0; i < 12; i++) { SetReady(g.u.s[i], i != 0); SetReload(g.u.s[i], 0.0f); }
    uint32_t r = Test(g);
    if (on) CHECK(r == 1 && C(C_READY) == 1 && C(C_EXCLUDED) == 1, "G1 officer outside the shooter class is not waited for");
    else    CHECK(r == 0 && C(C_READY) == 0, "G1 fix off: the game waits for the officer (vanilla)");
    // 2: a gun man of the group still reloading (reload time left, his reload order open)
    SetReady(g.u.s[0], true);
    SetReload(g.u.s[2], 4.0f);
    SetReady(g.u.s[2], false);
    r = Test(g);
    CHECK(r == 0, "G2 a reloading gun man keeps the group waiting");
    if (on) CHECK(C(C_RELOADING) == 1, "G2b counted as reloading");
    SetReload(g.u.s[2], 0.0f);
    SetReady(g.u.s[2], true);
    // 3: a gun man loaded but slow to his ready pose: waited for until the grace time, then not
    SetReady(g.u.s[3], false);
    tick = 300;
    SetTick(tick);
    r = Test(g);
    CHECK(r == 0, "G3 a man slow to his ready pose is waited for at first");
    for (int k = 1; k <= 14; k++) { SetTick(tick + k); r = Test(g); }
    CHECK(r == 0, "G3b ... for 1.4 s");
    SetTick(tick + 15);
    r = Test(g);
    if (on) CHECK(r == 1 && C(C_GRACE) == 1, "G3c ... and no longer after the 1.5 s grace time");
    else    CHECK(r == 0, "G3c fix off: no grace time (vanilla)");
    // 4: a new wait (gap in the calls) starts the grace time again
    SetTick(tick + 40);
    r = Test(g);
    CHECK(r == 0, "G4 a new wait starts the grace time again");
    SetReady(g.u.s[3], true);
    // 5: men of other groups do not count
    SetReady(g.u.s[5], false);
    SetReload(g.u.s[6], 9.0f);
    SetTick(tick + 60);
    r = Test(g);
    CHECK(r == 1, "G5 men of other groups do not count");
    // 6: the group order skips the officer only
    GroupOrder order = (GroupOrder)(void*)(g_base + 0x188290);
    order(g.task, nullptr, g.u.s[0], 1);
    order(g.task, nullptr, g.u.s[1], 1);
    if (on) CHECK(C(C_SKIPPED) == 1 && C(C_GIVEN) == 1, "O1 no ready / reload order for the officer, one for a gun man");
    else    CHECK(C(C_SKIPPED) == 0 && C(C_GIVEN) == 0, "O1 fix off: the group order is the game's own");
    // 7: Y switches it
    PressY();
    CHECK(C(C_FIXON) == (on ? 0 : 1), "K1 Y switches the fix");
    for (int i = 0; i < 12; i++) { SetReady(g.u.s[i], i != 0); SetReload(g.u.s[i], 0.0f); }
    SetTick(tick + 100);
    r = Test(g);
    CHECK(r == (on ? 0u : 1u), "K2 ... and the answer follows the switch");
    PressY();
    CHECK(C(C_FIXON) == (on ? 1 : 0), "K3 Y again switches it back");
}

// --- early reload: the fire-completed test FUN_1017b9d0 on group 0 (officer + gun men 1..3) ----
static uint32_t Done(Group& g) {
    GroupTest f = (GroupTest)(void*)(g_base + 0x17B9D0);
    return f(g.task) & 0xFF;
}

static void EarlyScene(bool on) {
    Group g = MakeGroup();
    g.task[0x44] = 1;                                   // the group was ordered to fire
    for (int i = 0; i < 12; i++) { SetReady(g.u.s[i], true); SetReload(g.u.s[i], 0.0f); }
    SetReady(g.u.s[0], false);                          // the sword officer, stuck as ever
    // 1: men 1 and 2 fired (reload time set, fire order done), man 3 still aiming
    SetReload(g.u.s[1], 19.8f);
    SetReload(g.u.s[2], 19.8f);
    SetReady(g.u.s[3], false);
    SetTick(500);
    uint32_t r = Done(g);
    if (on) CHECK(r == 0 && C(C_EARLY) == 2, "E1 the two men who fired get their reload order at once, the turn goes on");
    else    CHECK(r == 0 && C(C_EARLY) == 0, "E1 fix off: no early reload order (vanilla)");
    SetReady(g.u.s[1], false);                          // their new reload orders are open
    SetReady(g.u.s[2], false);
    SetTick(501);
    r = Done(g);
    CHECK(r == 0, "E2 the turn waits for the man still aiming");
    if (on) CHECK(C(C_EARLY) == 2, "E2b ... and gives no second early order");
    // 2: man 3 finishes without a shot: the turn is over although two reload orders are open
    SetReady(g.u.s[3], true);
    SetTick(502);
    r = Done(g);
    if (on) CHECK(r == 1 && C(C_DONE) == 1, "E3 done once every shooter fired or finished (officer left out)");
    else    CHECK(r == 0, "E3 fix off: the game waits for every open order (vanilla)");
    // 3: the exit's reload order for the whole group: not again for a man already reloading
    GroupOrder order = (GroupOrder)(void*)(g_base + 0x188290);
    long given = C(C_GIVEN);
    order(g.task, nullptr, g.u.s[1], 1);
    order(g.task, nullptr, g.u.s[3], 1);
    if (on) CHECK(C(C_EXITSKIP) == 1 && C(C_GIVEN) == given + 1, "E4 the exit's order skips a man already reloading, not one loaded");
    else    CHECK(C(C_EXITSKIP) == 0, "E4 fix off: the exit's orders are the game's own");
    if (!on) return;
    // 4: a man who fired but is still in his fire order gets his reload order when it is done
    Put32(g.task, 0x40, 1);                             // group 1: soldiers 4..7
    SetReload(g.u.s[4], 19.8f);
    SetReady(g.u.s[4], false);
    SetTick(600);
    long early = C(C_EARLY);
    r = Done(g);
    CHECK(r == 0 && C(C_EARLY) == early, "E5 no early order while the man's fire order runs");
    SetReady(g.u.s[4], true);
    SetTick(601);
    r = Done(g);
    CHECK(r == 1 && C(C_EARLY) == early + 1, "E6 ... and one once it is done");
}

// FBRFIX_FIRE_GRACE=20 with FBRFIX_EARLY_RELOAD=0: the turn ends 2 s after the group's last shot.
static void FireGraceScene() {
    Group g = MakeGroup();
    g.task[0x44] = 1;
    for (int i = 0; i < 12; i++) { SetReady(g.u.s[i], true); SetReload(g.u.s[i], 0.0f); }
    SetReady(g.u.s[2], false);                          // men 2 and 3 aiming
    SetReady(g.u.s[3], false);
    SetReload(g.u.s[1], 19.8f);                         // man 1 fired
    SetTick(700);
    uint32_t r = Done(g);
    CHECK(r == 0 && C(C_EARLY) == 0, "F1 early reload off: no order, the turn waits for the men aiming");
    SetReload(g.u.s[2], 19.8f);                         // man 2 fires at 701
    SetReady(g.u.s[2], true);
    for (uint32_t t = 701; t <= 720; t++) { SetTick(t); r = Done(g); }
    CHECK(r == 0 && C(C_CUT) == 0, "F2 a new shot starts the fire grace again (19 ticks after it)");
    SetTick(721);
    r = Done(g);
    CHECK(r == 1 && C(C_CUT) == 1 && C(C_NOTFIRED) == 1, "F3 20 ticks after the last shot the turn ends, man 3 keeps his load");
}

// A 15-man unit five ranks deep (groups 0..4 by index) under a task capped at 3 groups.
static void DeepScene(bool expectRaised) {
    Group g = MakeGroup();
    Unit u = MakeUnit(15);
    for (int i = 0; i < 15; i++) {
        g.table[i] = i / 3;
        Put32(u.s[i], 0x48C, 8);
        SetReady(u.s[i], true);
        SetReload(u.s[i], 0.0f);
    }
    g.u = u;
    Put32(g.task, 0x04, Addr(u.unit));
    SetTick(900);
    Test(g);
    uint32_t n;
    memcpy(&n, g.task + 0x38, 4);
    if (expectRaised) CHECK(n == 5 && C(C_ALLRANKS) == 1, "A1 FBRFIX_ALL_RANKS=1: the group count is raised to the 5 ranks");
    else              CHECK(n == 3 && C(C_ALLRANKS) == 0, "A1 the group count stays at the game's 3 without the option");
    Put32(g.task, 0x40, 1);   // a later group: never raised there
    Put32(g.task, 0x38, 3);
    SetTick(950);
    Test(g);
    memcpy(&n, g.task + 0x38, 4);
    CHECK(n == 3, "A2 the count is only raised before the first volley (group 0)");
}

static void RunSelfcheck() {
    Group bad = MakeGroup();
    Put32(bad.task, 0x38, 0);   // group count 0: not the decoded layout (the game's own test still runs on it)
    for (int k = 0; k < 20; k++) Test(bad);
    CHECK(C(C_BADTASK) == 20 && C(C_FIXON) == 0, "S1 20 broken group tasks switch the fix off");
    Group g = MakeGroup();
    for (int i = 0; i < 12; i++) { SetReady(g.u.s[i], i != 0); SetReload(g.u.s[i], 0.0f); }
    CHECK(Test(g) == 0, "S2 ... the game's own test runs again (waits for the officer)");
    PressY();
    CHECK(C(C_FIXON) == 0, "S3 Y is refused after the self-check");
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc < 5) { printf("usage: fbrfix_test <game dll> <proxy> <tag> fix|off|selfcheck|allranks|firegrace|probe\n"); return 2; }
    const char* mode = argv[4];
    bool probe = strcmp(mode, "probe") == 0;
    bool off = strcmp(mode, "off") == 0;
    bool selfcheck = strcmp(mode, "selfcheck") == 0;
    bool allRanks = strcmp(mode, "allranks") == 0;
    bool fireGrace = strcmp(mode, "firegrace") == 0;
    SetEnvironmentVariableA("FBRFIX_PROBE", probe ? "1" : nullptr);
    SetEnvironmentVariableA("FBRFIX_OFF", off ? "1" : nullptr);
    SetEnvironmentVariableA("FBRFIX_ALL_RANKS", allRanks ? "1" : nullptr);
    SetEnvironmentVariableA("FBRFIX_EARLY_RELOAD", fireGrace ? "0" : nullptr);
    SetEnvironmentVariableA("FBRFIX_FIRE_GRACE", fireGrace ? "20" : nullptr);
    SetEnvironmentVariableA("FBRFIX_GRACE", nullptr);
    SetEnvironmentVariableA("ARTFIX_PROBE", nullptr);
    SetEnvironmentVariableA("PATHFIX_CAPTURE", nullptr);

    HMODULE game = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!game) { printf("cannot map game dll: %lu\n", GetLastError()); return 1; }
    g_base = (uint8_t*)game;
    for (int i = 0; i < SITES; i++) {
        if (memcmp(g_base + kSites[i].rva, kSites[i].orig, 5) != 0) { printf("site %X differs\n", kSites[i].rva); return 1; }
        if (kSites[i].stubLen) Patch(g_base + kSites[i].rva + 5, kSites[i].stub, kSites[i].stubLen);
    }
    MakeClock();

    HMODULE proxy = LoadLibraryA(argv[2]);
    if (!proxy) { printf("cannot load proxy: %lu\n", GetLastError()); return 1; }
    DI8Create create = (DI8Create)GetProcAddress(proxy, "DirectInput8Create");
    g_counters = (TestCounters)GetProcAddress(proxy, "_FbrFixTestCounters@4");
    if (!g_counters) g_counters = (TestCounters)GetProcAddress(proxy, "FbrFixTestCounters");
    g_keys = (TestKeys)GetProcAddress(proxy, "_FbrFixTestKeys@4");
    if (!g_keys) g_keys = (TestKeys)GetProcAddress(proxy, "FbrFixTestKeys");
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
        CHECK(C(C_PROBE) == 0, "N1 probe off without FBRFIX_PROBE");
        if (selfcheck) RunSelfcheck();
        else if (allRanks) DeepScene(true);
        else if (fireGrace) FireGraceScene();
        else { RunFix(!off); EarlyScene(!off); if (!off) DeepScene(false); }
        FreeLibrary(proxy);
    }
    bool restored = true;
    for (int i = 0; i < SITES; i++) restored &= memcmp(g_base + kSites[i].rva, kSites[i].orig, 5) == 0;
    CHECK(restored, "U1 after unload every site holds its original bytes");
    printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
