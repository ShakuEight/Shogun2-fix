// aimtest.cpp -- offline harness for the aimfix hook (fixes/aimfix/src/aimfix.cpp).
//
// Maps the real empire.retail.dll (no game process), loads the proxy, and enters the game's own
// FUN_1047fd90 (engine fire action update) on fake game objects: a fire action with its target
// record, a gun, a target unit with its soldier array. A return stub patched right after the hook's
// eight stolen bytes ends the call there, so only the hook and the stolen instructions run. The
// checks read what the hook left in the action's target record.
//
//   aimtest <game dll> <proxy> <tag> full|off|selfcheck
//
//   full       fix on: centre soldier chosen for artillery, everything else left alone; U toggles it
//   off        AIMFIX_OFF=1: the hook is installed but changes nothing until U switches it on
//   selfcheck  20 artillery targets whose unit link is broken switch the fix off for the session (U refused)
//
// Output: one PASS / FAIL line per check, then "RESULT <n> passed, <m> failed".

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

typedef HRESULT (WINAPI *DI8Create)(HINSTANCE, DWORD, const void*, void**, void*);
typedef void (__stdcall *TestCounters)(long*);
typedef void (__stdcall *TestKeys)(LONG);
typedef void (__fastcall *ActionUpdate)(void* action);

static uint8_t* g_base = nullptr;
static int g_pass = 0, g_fail = 0;

#define CHECK(cond, name) do { if (cond) { g_pass++; printf("PASS %s\n", name); } \
                               else { g_fail++; printf("FAIL %s\n", name); } } while (0)

static const uint32_t RVA_ACTION_UPDATE = 0x47FD90;   // FUN_1047fd90
static const uint32_t RVA_FIRE_ORDER = 0x45A2E0;      // FUN_1045a2e0
static const uint8_t ORIGINAL[8] = { 0x53, 0x56, 0x8B, 0xF1, 0x80, 0x7E, 0x7C, 0x00 };

static void Patch(uint8_t* at, const uint8_t* bytes, size_t n) {
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
}

// --- fake game objects ----------------------------------------------------------------------

struct Rec { float ox, oy, oz; uint32_t ent, obj, xform; };

static uint8_t* Zalloc(size_t n) { return (uint8_t*)calloc(1, n); }
static void Put32(uint8_t* p, uint32_t off, uint32_t v) { memcpy(p + off, &v, 4); }
static void PutF(uint8_t* p, uint32_t off, float v) { memcpy(p + off, &v, 4); }
static uint32_t Addr(const void* p) { return (uint32_t)(uintptr_t)p; }

static uint32_t* g_vtArtillery = nullptr;   // slot +0x1B0 = the engine fire-order method
static uint32_t* g_vtInfantry = nullptr;    // slot +0x1B0 = something else

struct Unit {
    uint8_t* unit;
    uint32_t* arr;
    uint8_t* soldiers[256];
    int n;
};

// A line `files` wide and `ranks` deep, 1.4 m apart, centred on (cx, cz), front facing -z.
static Unit MakeUnit(int files, int ranks, float cx, float cz) {
    Unit u = {};
    u.n = files * ranks;
    u.unit = Zalloc(0x40);
    u.arr = (uint32_t*)calloc(u.n, 4);
    for (int r = 0; r < ranks; r++) {
        for (int f = 0; f < files; f++) {
            int i = r * files + f;
            uint8_t* s = Zalloc(0x400);
            Put32(s, 0, Addr(g_vtInfantry));
            PutF(s, 0x48, cx + (f - (files - 1) / 2.0f) * 1.4f);
            PutF(s, 0x4C, 10.0f);
            PutF(s, 0x50, cz + (r - (ranks - 1) / 2.0f) * 1.4f);
            Put32(s, 0x3B4, Addr(u.unit));
            u.soldiers[i] = s;
            u.arr[i] = Addr(s);
        }
    }
    Put32(u.unit, 0x18, (uint32_t)u.n);
    Put32(u.unit, 0x1C, Addr(u.arr));
    return u;
}

// Index of the soldier nearest to the unit's centroid, computed independently of the proxy.
static int CentreIndex(const Unit& u) {
    double sx = 0, sz = 0;
    for (int i = 0; i < u.n; i++) {
        float x, z;
        memcpy(&x, u.soldiers[i] + 0x48, 4);
        memcpy(&z, u.soldiers[i] + 0x50, 4);
        sx += x; sz += z;
    }
    float cx = (float)(sx / u.n), cz = (float)(sz / u.n);
    int best = 0;
    float bd = 3.4e38f;
    for (int i = 0; i < u.n; i++) {
        float x, z;
        memcpy(&x, u.soldiers[i] + 0x48, 4);
        memcpy(&z, u.soldiers[i] + 0x50, 4);
        float d = (x - cx) * (x - cx) + (z - cz) * (z - cz);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static uint8_t* MakeGun(bool artillery) {
    uint8_t* g = Zalloc(0x800);
    Put32(g, 0, Addr(artillery ? g_vtArtillery : g_vtInfantry));
    PutF(g, 0x48, 0.0f);
    PutF(g, 0x50, -400.0f);
    return g;
}

static uint8_t* MakeAction(uint8_t* gun, uint32_t ent, uint32_t obj, uint8_t copied) {
    uint8_t* a = Zalloc(0x100);
    Rec r = { 0.0f, 1.2f, 0.0f, ent, obj, 0 };
    memcpy(a + 0x20, &r, sizeof(r));
    Put32(a, 0x60, Addr(gun));
    a[0x7C] = copied;
    return a;
}

static Rec RecOf(const uint8_t* action) {
    Rec r;
    memcpy(&r, action + 0x20, sizeof(r));
    return r;
}

static ActionUpdate g_update = nullptr;
static TestCounters g_counters = nullptr;
static TestKeys g_keys = nullptr;

// one U press through the harness export: down for 200 ms, then up (the key thread polls every 50 ms)
static void PressU() {
    g_keys(1);
    Sleep(200);
    g_keys(0);
    Sleep(200);
}

static void Counters(long* c) { memset(c, 0, 5 * sizeof(long)); g_counters(c); }

// --- scenarios ------------------------------------------------------------------------------

static void RunFull(bool fixOn) {
    const char* tag = fixOn ? "" : " (off)";
    char name[160];
    long c0[5], c1[5];
    Counters(c0);

    // F1: 40 x 5 line, the game picked the soldier at the right end of the front rank
    Unit u = MakeUnit(40, 5, 100.0f, 0.0f);
    int centre = CentreIndex(u);
    uint8_t* gun = MakeGun(true);
    uint8_t* act = MakeAction(gun, u.arr[39], 0, 0);
    g_update(act);
    Rec r = RecOf(act);
    sprintf(name, "F1 artillery, end soldier -> %s%s", fixOn ? "centre soldier" : "unchanged", tag);
    CHECK(r.ent == (fixOn ? u.arr[centre] : u.arr[39]), name);
    sprintf(name, "F2 aim offset and other record fields kept%s", tag);
    CHECK(r.ox == 0.0f && r.oy == 1.2f && r.oz == 0.0f && r.obj == 0 && r.xform == 0, name);
    float x, z;
    memcpy(&x, (uint8_t*)(uintptr_t)r.ent + 0x48, 4);
    memcpy(&z, (uint8_t*)(uintptr_t)r.ent + 0x50, 4);
    sprintf(name, "F3 aim point within 1.5 m of the unit centre (%.1f, %.1f)%s", x, z, tag);
    CHECK(!fixOn || (fabsf(x - 100.0f) <= 1.5f && fabsf(z) <= 1.5f), name);

    // F4: same action again, now marked as copied: the hook must not touch it
    Unit u2 = MakeUnit(20, 3, -50.0f, 30.0f);
    uint8_t* act2 = MakeAction(gun, u2.arr[0], 0, 1);
    g_update(act2);
    sprintf(name, "F4 record already handed to the gun ([+0x7C] = 1): unchanged%s", tag);
    CHECK(RecOf(act2).ent == u2.arr[0], name);

    // F5: an infantry shooter (class without the engine fire-order method)
    uint8_t* inf = MakeGun(false);
    uint8_t* act3 = MakeAction(inf, u2.arr[0], 0, 0);
    g_update(act3);
    sprintf(name, "F5 non-artillery shooter: unchanged%s", tag);
    CHECK(RecOf(act3).ent == u2.arr[0], name);

    // F6: artillery at an object target (building): record has an object, no change
    uint8_t* building = Zalloc(0x100);
    uint8_t* act4 = MakeAction(gun, 0, Addr(building), 0);
    g_update(act4);
    sprintf(name, "F6 artillery at a building (object target): unchanged%s", tag);
    CHECK(RecOf(act4).ent == 0 && RecOf(act4).obj == Addr(building), name);

    // F7: the game already picked the centre soldier
    int c2 = CentreIndex(u2);
    uint8_t* act5 = MakeAction(gun, u2.arr[c2], 0, 0);
    g_update(act5);
    sprintf(name, "F7 centre soldier already chosen: unchanged%s", tag);
    CHECK(RecOf(act5).ent == u2.arr[c2], name);

    // F8: a lone survivor
    Unit u3 = MakeUnit(1, 1, 5.0f, 5.0f);
    uint8_t* act6 = MakeAction(gun, u3.arr[0], 0, 0);
    g_update(act6);
    sprintf(name, "F8 one-man unit: unchanged%s", tag);
    CHECK(RecOf(act6).ent == u3.arr[0], name);

    // F9: a soldier whose unit pointer does not hold him: no change
    Unit u4 = MakeUnit(10, 2, 0.0f, 0.0f);
    Put32(u4.soldiers[3], 0x3B4, Addr(u.unit));   // points at another unit
    uint8_t* act7 = MakeAction(gun, u4.arr[3], 0, 0);
    g_update(act7);
    sprintf(name, "F9 soldier not in his unit's array: unchanged%s", tag);
    CHECK(RecOf(act7).ent == u4.arr[3], name);

    // F10: a column (deep, narrow): centre of mass in the middle of the column
    Unit u5 = MakeUnit(4, 40, 0.0f, 200.0f);
    int c5 = CentreIndex(u5);
    uint8_t* act8 = MakeAction(gun, u5.arr[1], 0, 0);   // front rank
    g_update(act8);
    sprintf(name, "F10 column, front soldier -> %s%s", fixOn ? "middle of the column" : "unchanged", tag);
    CHECK(RecOf(act8).ent == (fixOn ? u5.arr[c5] : u5.arr[1]), name);

    Counters(c1);
    sprintf(name, "F11 counters: changed +%ld kept +%ld not-soldier +%ld bad-unit +%ld (expect %s)%s",
            c1[1] - c0[1], c1[2] - c0[2], c1[3] - c0[3], c1[4] - c0[4], fixOn ? "2 2 1 1" : "0 0 0 0", tag);
    if (fixOn)
        CHECK(c1[1] - c0[1] == 2 && c1[2] - c0[2] == 2 && c1[3] - c0[3] == 1 && c1[4] - c0[4] == 1, name);
    else
        CHECK(c1[1] == c0[1] && c1[2] == c0[2] && c1[3] == c0[3] && c1[4] == c0[4], name);
    sprintf(name, "F12 fix state %s", fixOn ? "on" : "off");
    CHECK(c1[0] == (fixOn ? 1 : 0), name);
}

// K1..K3: U flips the fix state, the hook follows it, a second U restores the start state
static void RunKeys(bool startOn) {
    char name[160];
    long c[5];
    uint8_t* gun = MakeGun(true);
    PressU();
    Counters(c);
    sprintf(name, "K1 U: fix %s", startOn ? "on -> off" : "off -> on");
    CHECK(c[0] == (startOn ? 0 : 1), name);
    Unit u = MakeUnit(40, 5, 300.0f, 0.0f);
    uint8_t* act = MakeAction(gun, u.arr[39], 0, 0);
    g_update(act);
    sprintf(name, "K2 after U, end soldier -> %s", startOn ? "unchanged" : "centre soldier");
    CHECK(RecOf(act).ent == (startOn ? u.arr[39] : u.arr[CentreIndex(u)]), name);
    PressU();
    Counters(c);
    sprintf(name, "K3 second U: fix back %s", startOn ? "on" : "off");
    CHECK(c[0] == (startOn ? 1 : 0), name);
}

static void RunSelfcheck() {
    long c[5];
    Unit other = MakeUnit(10, 2, 0.0f, 0.0f);
    uint8_t* gun = MakeGun(true);
    for (int i = 0; i < 19; i++) {
        Unit u = MakeUnit(10, 2, 50.0f, 50.0f);
        Put32(u.soldiers[0], 0x3B4, Addr(other.unit));
        g_update(MakeAction(gun, u.arr[0], 0, 0));
    }
    Counters(c);
    CHECK(c[0] == 1 && c[4] == 19, "S1 19 broken unit links: still on");
    Unit u = MakeUnit(10, 2, 50.0f, 50.0f);
    Put32(u.soldiers[0], 0x3B4, Addr(other.unit));
    g_update(MakeAction(gun, u.arr[0], 0, 0));
    Counters(c);
    CHECK(c[0] == 0 && c[4] == 20, "S2 the 20th with no success before: fix switched off for the session");
    Unit good = MakeUnit(40, 5, 0.0f, 0.0f);
    uint8_t* act = MakeAction(gun, good.arr[39], 0, 0);
    g_update(act);
    CHECK(RecOf(act).ent == good.arr[39], "S3 after the switch-off a valid target is left alone");
    PressU();
    Counters(c);
    uint8_t* act2 = MakeAction(gun, good.arr[39], 0, 0);
    g_update(act2);
    CHECK(c[0] == 0 && RecOf(act2).ent == good.arr[39], "S4 U is refused once the self-check switched the fix off");
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc < 5) { printf("usage: aimtest <game dll> <proxy> <tag> full|off|selfcheck\n"); return 2; }
    const char* mode = argv[4];
    bool off = strcmp(mode, "off") == 0;
    bool selfcheck = strcmp(mode, "selfcheck") == 0;
    SetEnvironmentVariableA("AIMFIX_OFF", off ? "1" : nullptr);
    SetEnvironmentVariableA("AIMFIX_PROBE", nullptr);
    SetEnvironmentVariableA("PATHFIX_CAPTURE", nullptr);

    HMODULE game = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!game) { printf("cannot map game dll: %lu\n", GetLastError()); return 1; }
    g_base = (uint8_t*)game;
    if (memcmp(g_base + RVA_ACTION_UPDATE, ORIGINAL, 8) != 0) { printf("FUN_1047fd90 prologue differs\n"); return 1; }
    // end the call right after the stolen bytes: POP ESI; POP EBX; RET (the function's own epilogue)
    const uint8_t stub[3] = { 0x5E, 0x5B, 0xC3 };
    Patch(g_base + RVA_ACTION_UPDATE + 8, stub, sizeof(stub));

    g_vtArtillery = (uint32_t*)calloc(0x200, 4);
    g_vtInfantry = (uint32_t*)calloc(0x200, 4);
    g_vtArtillery[0x1B0 / 4] = Addr(g_base + RVA_FIRE_ORDER);
    g_vtInfantry[0x1B0 / 4] = Addr(g_base + 0x45A8A0);

    HMODULE proxy = LoadLibraryA(argv[2]);
    if (!proxy) { printf("cannot load proxy: %lu\n", GetLastError()); return 1; }
    DI8Create create = (DI8Create)GetProcAddress(proxy, "DirectInput8Create");
    g_counters = (TestCounters)GetProcAddress(proxy, "_AimfixTestCounters@4");
    if (!g_counters) g_counters = (TestCounters)GetProcAddress(proxy, "AimfixTestCounters");
    g_keys = (TestKeys)GetProcAddress(proxy, "_AimfixTestKeys@4");
    if (!g_keys) g_keys = (TestKeys)GetProcAddress(proxy, "AimfixTestKeys");
    if (!create || !g_counters || !g_keys) {
        printf("missing export (create %p counters %p keys %p)\n", create, g_counters, g_keys);
        return 1;
    }
    void* out = nullptr;
    static const GUID iidDI8A = {0xBF798030,0x483A,0x4DA2,{0xAA,0x99,0x5D,0x64,0xED,0x36,0x97,0x00}};
    create(GetModuleHandleA(nullptr), 0x0800, &iidDI8A, &out, nullptr);
    printf("TAG %s mode %s\n", argv[3], mode);

    uint8_t* site = g_base + RVA_ACTION_UPDATE;
    int32_t rel;
    memcpy(&rel, site + 1, 4);
    uint8_t* dest = site + 5 + rel;
    bool intoProxy = (uintptr_t)dest >= (uintptr_t)proxy && (uintptr_t)dest < (uintptr_t)proxy + 0x100000;
    CHECK(site[0] == 0xE9 && site[5] == 0x90 && site[6] == 0x90 && site[7] == 0x90 && intoProxy,
          "I1 FUN_1047fd90 entry patched: JMP into the proxy + 3 NOPs");
    CHECK(g_base[0x3A6850] == 0x83 && g_base[0x45A8A0] == 0xF3, "I2 probe hooks not installed without AIMFIX_PROBE");
    g_update = (ActionUpdate)(void*)site;

    if (selfcheck) RunSelfcheck();
    else { RunFull(!off); RunKeys(!off); }

    FreeLibrary(proxy);
    CHECK(memcmp(site, ORIGINAL, 8) == 0, "R1 after unload the original entry bytes are back");
    printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
