// zombiefix_test.cpp -- offline harness for the zombie yari wall fix (fixes/zombie-yari-wall-fix/src/zombiefix.cpp).
//
// Maps the real empire.retail.dll (no game process), patches stubs right after the stolen bytes of
// the hooked sites, loads the proxy and enters those sites on fake game objects: two battle units,
// each with a group of four men and a squad task in a named state. The battle clock is faked
// through the same pointer path the proxy reads.
//
//   zombiefix_test <game dll> <proxy> <tag> fix|off|probe|corrupt
//
//   fix      defaults: only the rout tick patched; a routing unit's group marked set up but without
//            its rout task gets its mark cleared (the game then sets it up again), a group with the
//            rout task, an unmarked group and a dead group are left alone; I off: counted only
//   off      ZOMBIEFIX_OFF=1: counted only, until I
//   probe    ZOMBIEFIX_PROBE=1: every site patched; a squad task transition and a soldier machine
//            transition logged with their owners; pike and knockdown wrappers return the game
//            function's own outcome; kills logged, calls by a routing unit logged; the sampler
//            logs morale, group stacks, positions, men and raw dumps; squad tasks built for a
//            routing unit's group logged; the fix's re-arm logged (CSV files checked)
//   corrupt  one byte of a hooked site changed before the proxy loads: nothing installed
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

struct Site { uint32_t rva; int origLen; uint8_t orig[8]; uint8_t stub[32]; int stubLen; };
enum { S_TRANS = 0, S_PIKE, S_KNOCK, S_REFORM, S_INTERCEPT, S_ATTACK, S_ROUTTICK, SITES };
static const Site kSites[SITES] = {
    { 0x1374D0B, 5, { 0x89, 0x7E, 0x04, 0x8B, 0xCF }, { 0xC3 }, 1 },                           // ret (entered by CALL)
    // the pike outcome: result {outcome = [attacker+0x5F0], who = attacker}; returns the result; ret 0xC
    { 0x435410, 6, { 0x64, 0xA1, 0x2C, 0x00, 0x00, 0x00 },
      { 0x8B, 0x44, 0x24, 0x04,                     // mov eax,[esp+4]      result
        0x8B, 0x54, 0x24, 0x08,                     // mov edx,[esp+8]      attacker
        0x8B, 0x92, 0xF0, 0x05, 0x00, 0x00,         // mov edx,[edx+0x5F0]
        0x89, 0x10,                                 // mov [eax],edx
        0x8B, 0x54, 0x24, 0x08,                     // mov edx,[esp+8]
        0x89, 0x50, 0x04,                           // mov [eax+4],edx
        0xC2, 0x0C, 0x00 }, 26 },                   // ret 0xC
    // the knockdown outcome: [attacker+0x5F0]; after the stolen SUB ESP,0x98: add esp,0x98; ret 0x10
    { 0x430CF0, 6, { 0x81, 0xEC, 0x98, 0x00, 0x00, 0x00 },
      { 0x81, 0xC4, 0x98, 0x00, 0x00, 0x00,         // add esp,0x98
        0x8B, 0x44, 0x24, 0x04,                     // mov eax,[esp+4]      attacker
        0x8B, 0x80, 0xF0, 0x05, 0x00, 0x00,         // mov eax,[eax+0x5F0]
        0xC2, 0x10, 0x00 }, 19 },                   // ret 0x10
    // task constructors: after PUSH ESI; PUSH EDI; PUSH [ESP+0x10]: add esp,4; pop edi; pop esi;
    // mov eax,ecx; ret 0xC
    { 0x1283A0, 6, { 0x56, 0x57, 0xFF, 0x74, 0x24, 0x10 }, { 0x83, 0xC4, 0x04, 0x5F, 0x5E, 0x8B, 0xC1, 0xC2, 0x0C, 0x00 }, 10 },
    { 0x1283F0, 6, { 0x56, 0x57, 0xFF, 0x74, 0x24, 0x10 }, { 0x83, 0xC4, 0x04, 0x5F, 0x5E, 0x8B, 0xC1, 0xC2, 0x0C, 0x00 }, 10 },
    { 0x1273F0, 6, { 0x56, 0x57, 0xFF, 0x74, 0x24, 0x10 }, { 0x83, 0xC4, 0x04, 0x5F, 0x5E, 0x8B, 0xC1, 0xC2, 0x0C, 0x00 }, 10 },
    // the rout tick: after PUSH ESI; PUSH EDI; MOV EDI,ECX; MOV EAX,[EDI+0x10]: pop edi; pop esi; ret
    { 0x1BD3B0, 7, { 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0x47, 0x10 }, { 0x5F, 0x5E, 0xC3 }, 3 },
};
static const uint32_t RVA_ROUT_TASK_VT = 0x15E01F4;

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
static uint32_t Get32(const uint8_t* p, uint32_t off) { uint32_t v; memcpy(&v, p + off, 4); return v; }
static uint32_t Addr(const void* p) { return (uint32_t)(uintptr_t)p; }
static bool Patched(int s) { return g_base[kSites[s].rva] == 0xE9; }

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

// --- named states: vtable slot +0x10 is "MOV EAX,[table]; RET", [table] = the name ---------------
static uint8_t* MakeState(const char* name) {
    uint8_t* code = (uint8_t*)VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    uint32_t* table = (uint32_t*)Zalloc(4);
    *table = Addr(name);
    code[0] = 0xA1;
    uint32_t t = Addr(table);
    memcpy(code + 1, &t, 4);
    code[5] = 0xC3;
    uint8_t* vt = Zalloc(0x20);
    Put32(vt, 0x10, Addr(code));
    uint8_t* state = Zalloc(0x10);
    Put32(state, 0, Addr(vt));
    return state;
}

// --- fake battle unit: unit (morale block +0xCC4, main group +0xB9C), group (men +0x18 / +0x1C,
// behaviour stack at group+0x20+0x400), men (unit +0x214, group +0x3B4, position +0x48) ----------
struct Fake { uint8_t* u; uint8_t* g; uint8_t* s[4]; uint8_t* task; };
static Fake MakeUnit(float x, float z, const char* state) {
    Fake f = {};
    f.u = Zalloc(0x2400);   // the probe dumps 0x2010 bytes of a unit
    f.g = Zalloc(0x2800);
    uint32_t* list = (uint32_t*)Zalloc(16);
    for (int i = 0; i < 4; i++) {
        f.s[i] = Zalloc(0x800);
        Put32(f.s[i], 0x214, Addr(f.u));
        Put32(f.s[i], 0x3B4, Addr(f.g));
        PutF(f.s[i], 0x48, x + (float)i);
        PutF(f.s[i], 0x50, z);
        list[i] = Addr(f.s[i]);
    }
    Put32(f.g, 0x18, 4);
    Put32(f.g, 0x1C, Addr(list));
    Put32(f.u, 0xB9C, Addr(f.g));
    Put32(f.u, 0xCE8, 3);          // morale level: steady
    f.task = Zalloc(0x100);
    Put32(f.task, 0, Addr(Zalloc(0x40)));
    Put32(f.task, 0x04, Addr(f.g));
    Put32(f.task, 0x18 + 4, Addr(MakeState(state)));
    Put32(f.g, 0x20 + 0x400, Addr(f.task));
    Put32(f.g, 0x20 + 0x440, 1);
    return f;
}

static void Transition(uint8_t* ctx, uint8_t* oldState, uint8_t* next) {
    Put32(ctx, 4, Addr(oldState));
    uint32_t c = Addr(ctx), n = Addr(next);
    void* site = g_base + kSites[S_TRANS].rva;
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

// thiscall into the game code: ECX = self, stack arguments pushed right to left, callee pops them
static uint32_t CallThis(uint32_t rva, uint32_t self, int n, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0, uint32_t a4 = 0) {
    uint32_t fn = Addr(g_base) + rva, r;
    uint32_t args[4] = { a1, a2, a3, a4 };
    uint32_t* pa = args;
    __asm {
        mov ecx, n
        mov edx, pa
    next:
        test ecx, ecx
        jz go
        push dword ptr [edx + ecx * 4 - 4]
        dec ecx
        jmp next
    go:
        mov ecx, self
        call fn
        mov r, eax
    }
    return r;
}

static long C(int i) { long c[32] = {}; g_counters(c); return c[i]; }
enum { C_PROBE = 0, C_FIXON, C_INSTALLED, C_TRANS, C_TRANS_LOGGED, C_UNITS, C_PIKE, C_PIKE_KILL, C_KNOCK, C_KNOCK_KILL,
       C_KILL_ROWS, C_ROUT_KILLS, C_PIKE_HIGH, C_MORALE, C_REARMED, C_MISSING, C_TICKS, C_TASK_ROWS, C_FIX_ROWS };

// The rout interrupt context of a unit (unit +0xE78; its unit at +0x10) with the unit's group list
// (+0xB8C count, +0xB90 array) holding its group; the group marked "rout set up" (+0x2781).
static uint8_t* RoutCtx(Fake& f) {
    uint32_t* list = (uint32_t*)Zalloc(4);
    list[0] = Addr(f.g);
    Put32(f.u, 0xB8C, 1);
    Put32(f.u, 0xB90, Addr(list));
    Put32(f.u, 0xCEC, 2);
    Put32(f.u, 0xCE8, 6);
    f.g[0x2781] = 1;
    uint8_t* c = f.u + 0xE78;
    Put32(c, 0x10, Addr(f.u));
    return c;
}
static void RoutTick(uint8_t* c) { CallThis(kSites[S_ROUTTICK].rva, Addr(c), 0); }

// Puts the rout task (its vtable) on top of the group's stack, or takes it off again.
static void SetRoutTask(Fake& f, bool on) {
    static uint8_t* rt = nullptr;
    if (!rt) { rt = Zalloc(0x40); Put32(rt, 0, Addr(g_base) + RVA_ROUT_TASK_VT); }
    Put32(f.g, 0x20 + 0x400 + 8, Addr(rt));
    Put32(f.g, 0x20 + 0x440, on ? 2 : 1);
}

static void RoutScenes(bool fixOn) {
    Fake z = MakeUnit(50.0f, 50.0f, "READY_STATE");
    uint8_t* c = RoutCtx(z);
    long r0 = C(C_REARMED), m0 = C(C_MISSING);
    RoutTick(c);
    if (fixOn)
        CHECK(z.g[0x2781] == 0 && C(C_REARMED) == r0 + 1 && C(C_MISSING) == m0 + 1,
              "R1 routing group without its rout task: mark cleared (set up again by the game)");
    else
        CHECK(z.g[0x2781] == 1 && C(C_REARMED) == r0 && C(C_MISSING) == m0 + 1, "R1 fix off: counted, mark kept");
    z.g[0x2781] = 1;
    SetRoutTask(z, true);
    RoutTick(c);
    CHECK(z.g[0x2781] == 1 && C(C_MISSING) == m0 + 1, "R2 group with its rout task: left alone");
    SetRoutTask(z, false);
    z.g[0x2781] = 0;
    RoutTick(c);
    CHECK(z.g[0x2781] == 0 && C(C_MISSING) == m0 + 1, "R3 group not set up yet: left to the game");
    z.g[0x2781] = 1;
    Put32(z.g, 0x18, 0);
    RoutTick(c);
    CHECK(z.g[0x2781] == 1 && C(C_MISSING) == m0 + 1, "R4 group without men: left alone");
    Put32(z.g, 0x18, 4);
}

static void PressI() {
    g_keys(1);
    Sleep(200);
    g_keys(0);
    Sleep(120);
}

static void SitesScene(bool probe) {
    char name[96];
    for (int s = 0; s < SITES; s++) {
        bool want = probe || s == S_ROUTTICK;
        snprintf(name, sizeof(name), "I%d site RVA 0x%X %s", s + 1, kSites[s].rva, want ? "patched" : "untouched");
        CHECK(Patched(s) == want, name);
    }
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

// Field `col` (0-based) of the first data line (header skipped) whose field `keyCol` is `key`.
static bool Field(const char* dir, const char* name, int keyCol, const char* key, int col, char* out, size_t cap) {
    char path[MAX_PATH];
    static char line[0x4000];
    snprintf(path, sizeof(path), "%s%s", dir, name);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    bool found = false, header = true;
    while (fgets(line, sizeof(line), f) && !found) {
        if (header) { header = false; continue; }
        char* cols[64];
        int n = 0;
        cols[n++] = line;
        for (char* p = line; *p && n < 64; p++)
            if (*p == ',') { *p = 0; cols[n++] = p + 1; }
        if (keyCol >= n || col >= n || strcmp(cols[keyCol], key) != 0) continue;
        size_t k = strcspn(cols[col], "\r\n");
        if (k >= cap) k = cap - 1;
        memcpy(out, cols[col], k);
        out[k] = 0;
        found = true;
    }
    fclose(f);
    return found;
}

static bool LatestRun(char* out) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("zombiefix_runs\\*", &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    char best[MAX_PATH] = "";
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.' && strcmp(fd.cFileName, best) > 0)
            strcpy(best, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (!best[0]) return false;
    sprintf(out, "zombiefix_runs\\%s\\", best);
    return true;
}

static void Hex32(uint32_t v, char* out) { sprintf(out, "%X", v); }

static void RunProbe(HMODULE proxy) {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 1, "P0 installed, probe on");
    SitesScene(true);
    SetTick(0);   // the sampler waits for a battle tick: nothing sampled during the hook scenes
    Fake a = MakeUnit(10.0f, 20.0f, "FORM_PIKE_WALL_STATE");
    Fake b = MakeUnit(15.0f, 20.0f, "MELEE_STATE");
    // a squad task transition of unit A
    uint8_t* sNext = MakeState("MELEE_REACTION_STATE");
    Transition(a.task + 0x18, (uint8_t*)Get32(a.task, 0x18 + 4), sNext);
    CHECK(Get32(a.task, 0x18 + 4) == Addr(sNext), "T1 the stolen instructions ran: the context holds the new state");
    CHECK(C(C_TRANS) == 1 && C(C_TRANS_LOGGED) == 1 && C(C_UNITS) == 1, "T2 task transition logged, its unit registered");
    // a machine inside a soldier of unit A (an interrupt machine at soldier +0x100)
    Transition(a.s[2] + 0x100, MakeState("NO_INTERRUPT_STATE"), MakeState("MELEE_INTERRUPT_STATE"));
    CHECK(C(C_TRANS) == 2 && C(C_TRANS_LOGGED) == 2, "T3 soldier machine transition logged");
    // a pike kill by unit B's man 0 on unit A's man 1 (the stub's outcome: [attacker+0x5F0] = 5)
    uint32_t res[2] = { 1, 0 };
    Put32(b.s[0], 0x5F0, 5);
    uint32_t r = CallThis(kSites[S_PIKE].rva, 0x1234, 3, Addr(res), Addr(b.s[0]), Addr(a.s[1]));
    CHECK(r == Addr(res) && res[0] == 5 && res[1] == Addr(b.s[0]), "K1 pike wrapper: the game function ran, its result came back");
    CHECK(C(C_PIKE) == 1 && C(C_PIKE_KILL) == 1 && C(C_KILL_ROWS) == 1 && C(C_UNITS) == 2, "K2 pike kill logged, the attacker's unit registered");
    // a pike contact without a kill by a unit that is not routing: counted, not logged
    Put32(a.s[0], 0x5F0, 1);
    res[0] = 0;
    CallThis(kSites[S_PIKE].rva, 0x1234, 3, Addr(res), Addr(a.s[0]), Addr(b.s[1]));
    CHECK(C(C_PIKE) == 2 && C(C_KILL_ROWS) == 1, "K3 pike contact without a kill, attacker not routing: not logged");
    // unit A routs (rout state 3): its pike contacts are logged, its kills counted as kills by a router
    Put32(a.u, 0xCEC, 3);
    CallThis(kSites[S_PIKE].rva, 0x1234, 3, Addr(res), Addr(a.s[0]), Addr(b.s[1]));
    CHECK(C(C_KILL_ROWS) == 2 && C(C_ROUT_KILLS) == 0, "K4 routing attacker: contact logged");
    Put32(a.s[3], 0x5F0, 5);
    uint32_t k = CallThis(kSites[S_KNOCK].rva, 0x1234, 4, Addr(a.s[3]), Addr(b.s[2]), 3, 1);
    CHECK(k == 5 && C(C_KNOCK) == 1 && C(C_KNOCK_KILL) == 1, "K5 knockdown wrapper: the game's outcome (5) came back");
    CHECK(C(C_ROUT_KILLS) == 1 && C(C_KILL_ROWS) == 3, "K6 knockdown kill by the routing unit counted and logged");
    // the routing unit's state machines are all logged (past the per-machine sampling)
    for (int i = 0; i < 205; i++) Transition(a.task + 0x18, sNext, sNext);
    CHECK(C(C_TRANS_LOGGED) == 2 + 205, "T4 every transition of a routing unit logged");
    Put32(a.u, 0xCEC, 0);
    for (int i = 0; i < 205; i++) Transition(b.task + 0x18, sNext, sNext);
    long notRouting = C(C_TRANS_LOGGED) - 207;
    CHECK(notRouting == 200, "T5 a unit not routing: the first 200 per machine, then 1 in 20 (none of 201..205)");
    // squad tasks built for a group: logged only when its unit is routing
    uint8_t* params = Zalloc(0x40);
    uint32_t t1 = Addr(Zalloc(0x60));
    CHECK(CallThis(kSites[S_REFORM].rva, t1, 3, Addr(b.g), Addr(b.g), Addr(params)) == t1 && C(C_TASK_ROWS) == 0,
          "C1 re-form task for a unit not routing: constructor ran, not logged");
    Put32(b.u, 0xCEC, 2);
    CallThis(kSites[S_REFORM].rva, Addr(Zalloc(0x60)), 3, Addr(b.g), Addr(b.g), Addr(params));
    CallThis(kSites[S_INTERCEPT].rva, Addr(Zalloc(0x60)), 3, Addr(b.g), Addr(b.g), Addr(params));
    CallThis(kSites[S_ATTACK].rva, Addr(Zalloc(0x60)), 3, Addr(b.g), Addr(b.g), Addr(params));
    CHECK(C(C_TASK_ROWS) == 3, "C2 re-form, intercept and attack tasks for a routing unit logged");
    Put32(b.u, 0xCEC, 0);
    // the fix (on by default) with its row
    Fake z = MakeUnit(60.0f, 60.0f, "READY_STATE");
    uint8_t* zc = RoutCtx(z);
    RoutTick(zc);
    CHECK(z.g[0x2781] == 0 && C(C_REARMED) == 1 && C(C_FIX_ROWS) == 1, "R1 probe: re-arm done and logged");
    Put32(z.u, 0xCEC, 0);
    // the sampler: morale of both units, then a change
    SetTick(100);
    Sleep(250);
    CHECK(C(C_MORALE) == 2, "M1 morale of both units sampled once");
    Put32(a.u, 0xCEC, 2);
    Put32(a.u, 0xCE8, 7);
    SetTick(101);
    Sleep(250);
    CHECK(C(C_MORALE) == 3, "M2 a morale change logged");
    SetTick(112);
    Sleep(250);
    FreeLibrary(proxy);
    char dir[MAX_PATH];
    bool have = LatestRun(dir);
    CHECK(have, "F1 a run folder zombiefix_runs\\<stamp>\\ was written");
    if (!have) return;
    char v[256], key[16];
    CHECK(Lines(dir, "zombie_states.csv") == 1 + 2 + 205 + 200, "F2 zombie_states.csv: header + every logged transition");
    Hex32(Addr(a.task + 0x18), key);
    CHECK(Field(dir, "zombie_states.csv", 3, key, 4, v, sizeof(v)) && strcmp(v, "T") == 0, "F3 task machine: kind T");
    CHECK(Field(dir, "zombie_states.csv", 3, key, 11, v, sizeof(v)) && strcmp(v, "FORM_PIKE_WALL_STATE") == 0, "F4 ... old state named");
    CHECK(Field(dir, "zombie_states.csv", 3, key, 13, v, sizeof(v)) && strcmp(v, "MELEE_REACTION_STATE") == 0, "F5 ... new state named");
    Hex32(Addr(a.s[2] + 0x100), key);
    CHECK(Field(dir, "zombie_states.csv", 3, key, 4, v, sizeof(v)) && strcmp(v, "S") == 0, "F6 soldier machine: kind S");
    CHECK(Field(dir, "zombie_states.csv", 3, key, 6, v, sizeof(v)) && strcmp(v, "100") == 0, "F7 ... at soldier +0x100");
    CHECK(Field(dir, "zombie_states.csv", 3, key, 7, v, sizeof(v)) && strtoul(v, nullptr, 16) == Addr(a.u), "F8 ... of unit A");
    CHECK(Lines(dir, "zombie_kills.csv") == 4, "F9 zombie_kills.csv: header + pike kill, routing contact, knockdown kill");
    CHECK(Field(dir, "zombie_kills.csv", 2, "knock", 8, v, sizeof(v)) && strcmp(v, "3") == 0, "F10 knockdown row: attacker routing (3)");
    CHECK(Field(dir, "zombie_kills.csv", 2, "knock", 15, v, sizeof(v)) && strcmp(v, "5") == 0, "F11 ... outcome 5");
    CHECK(Field(dir, "zombie_kills.csv", 2, "pike", 17, v, sizeof(v)) && strcmp(v, "15.00") == 0, "F12 pike kill row: attacker x");
    Hex32(Addr(a.u), key);
    CHECK(Field(dir, "zombie_morale.csv", 4, "2", 2, v, sizeof(v)) && strcmp(v, key) == 0, "F13 morale row: unit A rout state 2");
    CHECK(Field(dir, "zombie_morale.csv", 4, "2", 3, v, sizeof(v)) && strcmp(v, "7") == 0, "F14 ... level 7");
    CHECK(Field(dir, "zombie_groups.csv", 3, "stack", 5, v, sizeof(v)) && strstr(v, "_STATE") != nullptr, "F15 a group stack row with a state name");
    CHECK(Lines(dir, "zombie_pos.csv") >= 3, "F16 zombie_pos.csv: both units' groups");
    CHECK(Lines(dir, "zombie_men.csv") >= 3, "F17 zombie_men.csv: men sampled");
    CHECK(Lines(dir, "zombie_raw.csv") >= 1 + 2 * 4, "F18 zombie_raw.csv: unit, group and two men of each unit");
    CHECK(Lines(dir, "zombie_tasks.csv") == 4, "F19 zombie_tasks.csv: header + 3 tasks");
    CHECK(Field(dir, "zombie_tasks.csv", 2, "intercept", 8, v, sizeof(v)) && strcmp(v, "2") == 0, "F20 ... rout state 2");
    Hex32(Addr(z.g), key);
    CHECK(Field(dir, "zombie_fix.csv", 2, "rearm", 4, v, sizeof(v)) && strcmp(v, key) == 0, "F21 zombie_fix.csv: the re-armed group");
}

static void RunFix() {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 0 && C(C_FIXON) == 1, "X1 installed, probe off, fix on at launch");
    SitesScene(false);
    RoutScenes(true);
    PressI();
    CHECK(C(C_FIXON) == 0, "K1 I: fix off");
    RoutScenes(false);
    PressI();
    CHECK(C(C_FIXON) == 1, "K2 I again: on");
    RoutScenes(true);
}

static void RunOff() {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 0 && C(C_FIXON) == 0, "X1 installed, probe off, fix off (ZOMBIEFIX_OFF=1)");
    SitesScene(false);
    RoutScenes(false);
    PressI();
    CHECK(C(C_FIXON) == 1, "K1 I: fix on");
    RoutScenes(true);
}

static void RunCorrupt() {
    CHECK(C(C_INSTALLED) == 0, "X1 a changed byte in a hooked site: not installed");
    bool none = true;
    for (int s = 0; s < SITES; s++) none &= !Patched(s);
    CHECK(none, "X2 no site patched");
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc < 5) { printf("usage: zombiefix_test <game dll> <proxy> <tag> fix|off|probe|corrupt\n"); return 2; }
    const char* mode = argv[4];
    bool probe = strcmp(mode, "probe") == 0;
    bool off = strcmp(mode, "off") == 0;
    bool corrupt = strcmp(mode, "corrupt") == 0;
    SetEnvironmentVariableA("ZOMBIEFIX_PROBE", probe || corrupt ? "1" : nullptr);
    SetEnvironmentVariableA("ZOMBIEFIX_OFF", off ? "1" : nullptr);
    SetEnvironmentVariableA("WALLFIX_PROBE", nullptr);
    SetEnvironmentVariableA("SIEGEFIX_PROBE", nullptr);
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
    if (corrupt) {
        uint8_t b = 0x90;
        Patch(g_base + kSites[S_PIKE].rva + 1, &b, 1);
    }
    static uint8_t before[SITES][8];
    for (int s = 0; s < SITES; s++) memcpy(before[s], g_base + kSites[s].rva, kSites[s].origLen);
    MakeClock();
    HMODULE proxy = LoadLibraryA(argv[2]);
    if (!proxy) { printf("cannot load proxy: %lu\n", GetLastError()); return 1; }
    g_counters = (TestCounters)GetProcAddress(proxy, "_ZombieFixTestCounters@4");
    if (!g_counters) g_counters = (TestCounters)GetProcAddress(proxy, "ZombieFixTestCounters");
    g_keys = (TestKeys)GetProcAddress(proxy, "_ZombieFixTestKeys@4");
    if (!g_keys) g_keys = (TestKeys)GetProcAddress(proxy, "ZombieFixTestKeys");
    DI8Create create = (DI8Create)GetProcAddress(proxy, "DirectInput8Create");
    if (!g_counters || !g_keys || !create) { printf("proxy exports missing\n"); return 1; }
    void* out = nullptr;
    static const GUID iidDI8A = {0xBF798030,0x483A,0x4DA2,{0xAA,0x99,0x5D,0x64,0xED,0x36,0x97,0x00}};
    create(GetModuleHandleA(nullptr), 0x0800, &iidDI8A, &out, nullptr);
    printf("TAG %s mode %s\n", argv[3], mode);
    if (probe) {
        RunProbe(proxy);   // unloads the proxy itself
    } else {
        if (off) RunOff();
        else if (corrupt) RunCorrupt();
        else RunFix();
        FreeLibrary(proxy);
    }
    bool restored = true;
    for (int s = 0; s < SITES; s++) restored &= memcmp(g_base + kSites[s].rva, before[s], kSites[s].origLen) == 0;
    CHECK(restored, "U1 after unload every hook site holds the bytes it had before the proxy loaded");
    printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
