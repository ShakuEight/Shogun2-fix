// collapsefix_test.cpp -- offline harness for the wall collapse fix (fixes/wall-collapse-fix/src/collapsefix.cpp).
//
// Maps the real empire.retail.dll (no game process), patches stubs right after the stolen bytes of
// the piece functions the probe wraps, loads the proxy and enters the hooked sites on fake game
// objects: a battle with buildings and pieces (gate doors and wall pieces), the gates-open flag,
// the battle clock and phase through the same pointer path the proxy reads. The battle start's
// gate loop (FUN_1043c870) and the per-piece gate damage (FUN_1043c8b0) are the game's own code;
// only the damage function they end in (FUN_1047b370) is a stub (marks the piece destroyed).
//
//   collapsefix_test <game dll> <proxy> <tag> fix|off|probe|corrupt
//
//   fix      defaults: only the gate loop patched; gates open to all: the intact gate door is
//            not damaged, the counters say kept; I off: the game's code destroys it; a castle
//            whose gates are not open: nothing damaged either way; damage added to a piece
//            outside the battle start (artillery) still destroys it, fix on
//   off      COLLAPSEFIX_OFF=1: the door is destroyed until I, then kept
//   probe    COLLAPSEFIX_PROBE=1: every site patched; the wrappers run the game function (stub)
//            and log before / after; phase change, hit point set, fort damage and the gate loop
//            logged (CSV files checked)
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

struct Site { uint32_t rva; int origLen; uint8_t orig[10]; uint8_t stub[32]; int stubLen; };
enum { S_SETTER = 0, S_DMGSET, S_COLLAPSE, S_DAMAGE, S_FORT, S_PHASE, S_HP, S_GATES, SITES };
static const Site kSites[SITES] = {
    // level apply: after SUB ESP,0xC; PUSH ESI; PUSH EDI: piece +0x68 = level; pop edi; pop esi;
    // add esp,0xC; ret 8
    { 0x477F40, 5, { 0x83, 0xEC, 0x0C, 0x56, 0x57 },
      { 0x8B, 0x44, 0x24, 0x18, 0x89, 0x41, 0x68, 0x5F, 0x5E, 0x83, 0xC4, 0x0C, 0xC2, 0x08, 0x00 }, 15 },
    // damage set: after MOV EAX,[ESP+4]; PUSH EBX: piece +0xEC = damage; pop ebx; ret 4
    { 0x477FF0, 5, { 0x8B, 0x44, 0x24, 0x04, 0x53 }, { 0x89, 0x81, 0xEC, 0x00, 0x00, 0x00, 0x5B, 0xC2, 0x04, 0x00 }, 10 },
    // collapse effects: after SUB ESP,0x14; PUSH EBP; MOV EBP,ECX: pop ebp; add esp,0x14; ret
    { 0x479CA0, 6, { 0x83, 0xEC, 0x14, 0x55, 0x8B, 0xE9 }, { 0x5D, 0x83, 0xC4, 0x14, 0xC3 }, 5 },
    // damage added: after CMP byte [ECX+0xF4],0: piece +0xEC = damage (float bits), destroyed
    // (+0xF1 = 1), return 3; ret 4
    { 0x47B370, 7, { 0x80, 0xB9, 0xF4, 0x00, 0x00, 0x00, 0x00 },
      { 0x8B, 0x44, 0x24, 0x04, 0x89, 0x81, 0xEC, 0x00, 0x00, 0x00, 0xC6, 0x81, 0xF1, 0x00, 0x00, 0x00, 0x01,
        0xB8, 0x03, 0x00, 0x00, 0x00, 0xC2, 0x04, 0x00 }, 25 },
    // fort damage: after SUB ESP,0x10; PUSH ESI; PUSH EDI: pop edi; pop esi; add esp,0x10; ret 4
    { 0x426940, 5, { 0x83, 0xEC, 0x10, 0x56, 0x57 }, { 0x5F, 0x5E, 0x83, 0xC4, 0x10, 0xC2, 0x04, 0x00 }, 8 },
    // phase set, hit point set, gate loop: the game's own code continues (no stub)
    { 0x1ACFB0, 7, { 0x8B, 0x44, 0x24, 0x04, 0x89, 0x41, 0x14 }, { 0 }, 0 },
    { 0x3A3080, 8, { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x44, 0x24, 0x08 }, { 0 }, 0 },
    { 0x43C870, 10, { 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0xB7, 0xD0, 0x01, 0x00, 0x00 }, { 0 }, 0 },
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
static uint32_t Get32(const uint8_t* p, uint32_t off) { uint32_t v; memcpy(&v, p + off, 4); return v; }
static uint32_t Addr(const void* p) { return (uint32_t)(uintptr_t)p; }
static uint32_t FBits(float f) { uint32_t v; memcpy(&v, &f, 4); return v; }
static bool Patched(int s) { return g_base[kSites[s].rva] == 0xE9; }

// --- fake battle: [base+0x017C755C] -> +0x50 -> +0x00 -> +0x1C = tick object (tick +0x50, battle
// +0xB4, settings +0x5C with the gates-open flag +0x9C, manager +0x98) ---------------------------
static uint8_t* g_tickObj = nullptr;
static uint8_t* g_battle = nullptr;
static uint8_t* g_settings = nullptr;
static uint8_t* g_owner = nullptr;     // a piece's +0xC: its +8 is the tick object
static void SetTick(uint32_t t) { Put32(g_tickObj, 0x50, t); }
static void SetGatesOpen(bool on) { g_settings[0x9C] = on ? 1 : 0; }
static void MakeBattle() {
    uint8_t* shell = Zalloc(0x100);
    uint8_t* mode = Zalloc(0x100);
    uint8_t* owner = Zalloc(0x100);
    g_tickObj = Zalloc(0x200);
    g_battle = Zalloc(0x400);
    g_settings = Zalloc(0x200);
    g_owner = Zalloc(0x40);
    Put32(shell, 0x50, Addr(mode));
    Put32(mode, 0x00, Addr(owner));
    Put32(owner, 0x1C, Addr(g_tickObj));
    Put32(g_tickObj, 0xB4, Addr(g_battle));
    Put32(g_tickObj, 0x5C, Addr(g_settings));
    Put32(g_tickObj, 0x98, Addr(Zalloc(0x1000)));
    Put32(g_owner, 0x08, Addr(g_tickObj));
    Put32(g_battle, 0x14, 3);          // deployment
    uint32_t p = Addr(shell);
    Patch(g_base + 0x017C755C, &p, 4);
}

// --- pieces: template +0x18 (two levels, thresholds 75 and 150), model +0x64, level +0x68, gate
// +0x6C, damage +0xEC, destroyed +0xF1 --------------------------------------------------------
static uint8_t* g_models[2];
static uint8_t* g_tmpl = nullptr;
static uint8_t* MakePiece(bool gate, float x, float z) {
    if (!g_tmpl) {
        g_tmpl = Zalloc(0x100);
        uint32_t* lev = (uint32_t*)Zalloc(8);
        for (int i = 0; i < 2; i++) {
            g_models[i] = Zalloc(0x100);
            Put32(g_models[i], 0x10, i);
            Put32(g_models[i], 0x90, i ? 150 : 75);
            lev[i] = Addr(g_models[i]);
        }
        Put32(g_tmpl, 0x6C, 2);
        Put32(g_tmpl, 0x70, Addr(lev));
    }
    uint8_t* p = Zalloc(0x400);
    Put32(p, 0x0C, Addr(g_owner));
    Put32(p, 0x18, Addr(g_tmpl));
    Put32(p, 0x64, Addr(g_models[0]));
    p[0x6C] = gate ? 1 : 0;
    p[0xF3] = 1;
    PutF(p, 0xB4, x);
    PutF(p, 0xBC, z);
    return p;
}
static bool Destroyed(const uint8_t* p) { return p[0xF1] != 0; }
static void Repair(uint8_t* p) { p[0xF1] = 0; Put32(p, 0xEC, 0); }

// A building of two pieces: [0] the gate structure (not a gate piece), [1] the gate door.
struct Bld { uint8_t* b; uint8_t* frame; uint8_t* door; };
static uint8_t* g_blds[8];
static int g_nBlds = 0;
static Bld MakeGate(float x, float z) {
    Bld r;
    r.b = Zalloc(0x400);
    r.frame = MakePiece(false, x, z);
    r.door = MakePiece(true, x, z + 4);
    uint32_t* list = (uint32_t*)Zalloc(8);
    list[0] = Addr(r.frame);
    list[1] = Addr(r.door);
    Put32(r.b, 0x1CC, 2);
    Put32(r.b, 0x1D0, Addr(list));
    g_blds[g_nBlds++] = r.b;
    uint32_t* bl = (uint32_t*)Zalloc(4 * 8);
    for (int i = 0; i < g_nBlds; i++) bl[i] = Addr(g_blds[i]);
    Put32(g_battle, 0x114, g_nBlds);
    Put32(g_battle, 0x118, Addr(bl));
    return r;
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
enum { C_PROBE = 0, C_FIXON, C_INSTALLED, C_PHASE, C_SETTER, C_DMGSET, C_COLLAPSE, C_DAMAGE, C_FORT, C_HP, C_HPROWS,
       C_DEATHS, C_PIECEROWS, C_MENROWS, C_START, C_BUILDINGS, C_SKIPPED, C_DOORS, C_KEPT };

static void PressI() {
    g_keys(1);
    Sleep(200);
    g_keys(0);
    Sleep(120);
}

static void GateLoop(Bld& g) { CallThis(kSites[S_GATES].rva, Addr(g.b), 0); }

static void SitesScene(bool probe) {
    char name[96];
    for (int s = 0; s < SITES; s++) {
        bool want = probe || s == S_GATES;
        snprintf(name, sizeof(name), "I%d site RVA 0x%X %s", s + 1, kSites[s].rva, want ? "patched" : "untouched");
        CHECK(Patched(s) == want, name);
    }
}

// The battle start on one castle gate: the door is kept with the fix on, destroyed by the game's
// own code with it off; the gate structure is never touched; nothing happens when the gates are
// not open to all; damage from anything else still destroys a piece.
static void GateScenes(bool fixOn, const char* tag) {
    char name[128];
    Bld g = MakeGate(300.0f, -400.0f);
    SetGatesOpen(true);
    long b0 = C(C_BUILDINGS), s0 = C(C_SKIPPED), d0 = C(C_DOORS), k0 = C(C_KEPT);
    GateLoop(g);
    if (fixOn) {
        snprintf(name, sizeof(name), "G1%s gates open to all, fix on: the intact door is not damaged", tag);
        CHECK(!Destroyed(g.door) && Get32(g.door, 0xEC) == 0, name);
        snprintf(name, sizeof(name), "G2%s ... counted: one call skipped, one door kept", tag);
        CHECK(C(C_BUILDINGS) == b0 + 1 && C(C_SKIPPED) == s0 + 1 && C(C_DOORS) == d0 + 1 && C(C_KEPT) == k0 + 1, name);
    } else {
        snprintf(name, sizeof(name), "G1%s gates open to all, fix off: the game's code destroys the door (threshold + 1)", tag);
        CHECK(Destroyed(g.door) && Get32(g.door, 0xEC) == FBits(76.0f), name);
        snprintf(name, sizeof(name), "G2%s ... counted: not skipped, one door, none kept", tag);
        CHECK(C(C_BUILDINGS) == b0 + 1 && C(C_SKIPPED) == s0 && C(C_DOORS) == d0 + 1 && C(C_KEPT) == k0, name);
    }
    snprintf(name, sizeof(name), "G3%s the gate structure (not a gate piece) is never damaged", tag);
    CHECK(!Destroyed(g.frame), name);
    Repair(g.door);
    SetGatesOpen(false);
    d0 = C(C_DOORS);
    GateLoop(g);
    snprintf(name, sizeof(name), "G4%s gates not open: no door damaged, none counted", tag);
    CHECK(!Destroyed(g.door) && C(C_DOORS) == d0, name);
    // artillery: damage added to a wall piece outside the battle start
    SetGatesOpen(true);
    uint8_t* wall = MakePiece(false, 10.0f, 10.0f);
    uint32_t r = CallThis(kSites[S_DAMAGE].rva, Addr(wall), 1, FBits(200.0f));
    snprintf(name, sizeof(name), "G5%s damage from a projectile still destroys a wall piece", tag);
    CHECK(r == 3 && Destroyed(wall) && Get32(wall, 0xEC) == FBits(200.0f), name);
    r = CallThis(kSites[S_DAMAGE].rva, Addr(g.door), 1, FBits(90.0f));
    snprintf(name, sizeof(name), "G6%s ... and a gate door (a ram or artillery in battle)", tag);
    CHECK(r == 3 && Destroyed(g.door), name);
}

static bool LatestRun(char* out) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("collapsefix_runs\\*", &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    char best[MAX_PATH] = "";
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.' && strcmp(fd.cFileName, best) > 0)
            strcpy(best, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (!best[0]) return false;
    sprintf(out, "collapsefix_runs\\%s\\", best);
    return true;
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

static void RunProbe(HMODULE proxy) {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 1, "P0 installed, probe on");
    SitesScene(true);
    SetTick(10);
    Bld g = MakeGate(100.0f, 50.0f);
    // level apply wrapped: the game function (stub) stored the level
    CallThis(kSites[S_SETTER].rva, Addr(g.frame), 2, 1, 0);
    CHECK(Get32(g.frame, 0x68) == 1 && C(C_SETTER) == 1, "W1 level apply wrapped: the game function ran, counted");
    CallThis(kSites[S_DMGSET].rva, Addr(g.frame), 1, 150);
    CHECK(Get32(g.frame, 0xEC) == 150 && C(C_DMGSET) == 1, "W2 damage set wrapped: the game function ran, counted");
    CallThis(kSites[S_COLLAPSE].rva, Addr(g.frame), 0);
    CHECK(C(C_COLLAPSE) == 1, "W3 collapse effects counted");
    uint8_t* fortData = Zalloc(0x40);
    CallThis(kSites[S_FORT].rva, Addr(Zalloc(0x40)), 1, Addr(fortData));
    CHECK(C(C_FORT) == 1, "W4 fort damage wrapped, counted");
    // a soldier's hit points set during deployment (logged) and a death
    uint8_t* sol = Zalloc(0x800);
    CallThis(kSites[S_HP].rva, 0, 3, Addr(sol), 40, 0);
    CHECK(Get32(sol, 0x680) == 40 && C(C_HP) == 1 && C(C_HPROWS) == 1, "H1 hit point set: the game's store ran, logged in deployment");
    // Start Battle: phase 3 -> 5 (the game's store ran, the start tick noted)
    SetTick(451);
    CallThis(kSites[S_PHASE].rva, Addr(g_battle), 1, 5);
    CHECK(Get32(g_battle, 0x14) == 5 && C(C_PHASE) == 1 && C(C_START) == 451, "H2 phase set: the game's store ran, Start Battle tick noted");
    CallThis(kSites[S_HP].rva, 0, 3, Addr(sol), 0, 0);
    CHECK(C(C_DEATHS) == 1 && C(C_HPROWS) == 2, "H3 a death logged");
    SetTick(2000);
    CallThis(kSites[S_HP].rva, 0, 3, Addr(sol), 30, 0);
    CHECK(C(C_HPROWS) == 2, "H4 a hit point set long after Start Battle: not logged");
    // the gate loop with the probe (fix on): the door kept, a gate_kept row
    SetGatesOpen(true);
    GateLoop(g);
    CHECK(!Destroyed(g.door) && C(C_KEPT) == 1, "G1 probe build, fix on: door kept");
    // damage added wrapped: the stub's result came back
    uint32_t r = CallThis(kSites[S_DAMAGE].rva, Addr(g.door), 1, FBits(90.0f));
    CHECK(r == 3 && Destroyed(g.door) && C(C_DAMAGE) == 1, "W5 damage added wrapped: the game's result (3) came back");
    Sleep(250);
    CHECK(C(C_PIECEROWS) >= 2, "S1 sampler: piece rows written");
    FreeLibrary(proxy);
    char dir[MAX_PATH];
    bool have = LatestRun(dir);
    CHECK(have, "F1 a run folder collapsefix_runs\\<stamp>\\ was written");
    if (!have) return;
    char v[256], key[16];
    sprintf(key, "%X", Addr(g.frame));
    CHECK(Field(dir, "collapse_events.csv", 3, "apply", 5, v, sizeof(v)) && strcmp(v, key) == 0, "F2 apply row: the piece");
    CHECK(Field(dir, "collapse_events.csv", 3, "apply", 10, v, sizeof(v)) && strcmp(v, "1") == 0, "F3 ... level after 1");
    CHECK(Field(dir, "collapse_events.csv", 3, "gate_kept", 6, v, sizeof(v)) && strtoul(v, nullptr, 16) == Addr(g.b),
          "F4 gate_kept row: the building");
    CHECK(Field(dir, "collapse_events.csv", 3, "damage", 8, v, sizeof(v)) && strcmp(v, "3") == 0, "F5 damage row: result 3");
    CHECK(Lines(dir, "collapse_events.csv") == 1 + 7, "F6 collapse_events.csv: header + 7 rows");
    CHECK(Field(dir, "collapse_phase.csv", 3, "5", 2, v, sizeof(v)) && strcmp(v, "3") == 0, "F7 phase row 3 -> 5");
    CHECK(Lines(dir, "collapse_deaths.csv") == 3, "F8 collapse_deaths.csv: header + 2 rows");
    CHECK(Lines(dir, "collapse_pieces.csv") >= 3, "F9 collapse_pieces.csv: snapshot at the phase change");
}

static void RunFix() {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 0 && C(C_FIXON) == 1, "X1 installed, probe off, fix on at launch");
    SitesScene(false);
    GateScenes(true, "a");
    PressI();
    CHECK(C(C_FIXON) == 0, "K1 I: fix off");
    GateScenes(false, "b");
    PressI();
    CHECK(C(C_FIXON) == 1, "K2 I again: on");
    GateScenes(true, "c");
}

static void RunOff() {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 0 && C(C_FIXON) == 0, "X1 installed, probe off, fix off (COLLAPSEFIX_OFF=1)");
    SitesScene(false);
    GateScenes(false, "a");
    PressI();
    CHECK(C(C_FIXON) == 1, "K1 I: fix on");
    GateScenes(true, "b");
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
    if (argc < 5) { printf("usage: collapsefix_test <game dll> <proxy> <tag> fix|off|probe|corrupt\n"); return 2; }
    const char* mode = argv[4];
    bool probe = strcmp(mode, "probe") == 0;
    bool off = strcmp(mode, "off") == 0;
    bool corrupt = strcmp(mode, "corrupt") == 0;
    SetEnvironmentVariableA("COLLAPSEFIX_PROBE", probe || corrupt ? "1" : nullptr);
    SetEnvironmentVariableA("COLLAPSEFIX_OFF", off ? "1" : nullptr);
    SetEnvironmentVariableA("COLLAPSEFIX_ON", nullptr);
    SetEnvironmentVariableA("ZOMBIEFIX_PROBE", nullptr);
    SetEnvironmentVariableA("WALLFIX_PROBE", nullptr);
    SetEnvironmentVariableA("SIEGEFIX_PROBE", nullptr);
    SetEnvironmentVariableA("FBRFIX_PROBE", nullptr);
    SetEnvironmentVariableA("ARTFIX_PROBE", nullptr);
    SetEnvironmentVariableA("GUARDFIX_PROBE", nullptr);
    SetEnvironmentVariableA("PATHFIX_CAPTURE", nullptr);

    HMODULE game = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!game) { printf("cannot map game dll: %lu\n", GetLastError()); return 1; }
    g_base = (uint8_t*)game;
    for (int s = 0; s < SITES; s++) {
        if (memcmp(g_base + kSites[s].rva, kSites[s].orig, kSites[s].origLen) != 0) { printf("site %X differs\n", kSites[s].rva); return 1; }
        if (kSites[s].stubLen) Patch(g_base + kSites[s].rva + kSites[s].origLen, kSites[s].stub, kSites[s].stubLen);
    }
    if (corrupt) {
        uint8_t b = 0x90;
        Patch(g_base + kSites[S_GATES].rva + 1, &b, 1);
    }
    static uint8_t before[SITES][10];
    for (int s = 0; s < SITES; s++) memcpy(before[s], g_base + kSites[s].rva, kSites[s].origLen);
    MakeBattle();
    HMODULE proxy = LoadLibraryA(argv[2]);
    if (!proxy) { printf("cannot load proxy: %lu\n", GetLastError()); return 1; }
    g_counters = (TestCounters)GetProcAddress(proxy, "_CollapseFixTestCounters@4");
    if (!g_counters) g_counters = (TestCounters)GetProcAddress(proxy, "CollapseFixTestCounters");
    g_keys = (TestKeys)GetProcAddress(proxy, "_CollapseFixTestKeys@4");
    if (!g_keys) g_keys = (TestKeys)GetProcAddress(proxy, "CollapseFixTestKeys");
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
