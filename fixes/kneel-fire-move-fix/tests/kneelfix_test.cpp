// kneelfix_test.cpp -- offline harness for the kneel fire move fix (fixes/kneel-fire-move-fix/src/kneelfix.cpp).
//
// Maps the real empire.retail.dll (no game process), loads the proxy and calls the eight redirected
// CALLs of the can-move test FUN_1015c3f0 through the displacement the proxy wrote into each
// instruction. The game's own FUN_1015c3f0 runs (and, through the fix, its code past the kneel
// step) on fake units: the active-ability set (+0xED4) is a fake object whose bit test answers
// for ability 0x39 from a byte; the battle phase is read through unit +0x14 -> +8 -> +0xB4 -> +0x14
// (phase 0: the game answers "can move" unless kneel fire is on; phase 5 with a top behaviour
// (unit +0xBA8 = 1, +0xBA0) whose vfunc +0xDC answers no: "cannot move" for a reason of its own).
//
//   kneelfix_test <game dll> <proxy> <tag> fix|off|probe|corrupt|corrupt_head
//
//   fix           defaults: every site redirected; a unit without kneel fire keeps the game's
//                 answer (yes in phase 0, no when its behaviour refuses); a kneeling unit in
//                 phase 0 is let move, a kneeling unit whose behaviour refuses is still refused;
//                 I off / on; the self-check (a bit test that changes its answer between the
//                 game's call and the fix's) switches the fix off and I is then refused
//   off           KNEELFIX_OFF=1: the game's answers only, until I
//   probe         KNEELFIX_PROBE=1: kneel_calls.csv rows for kneeling units (changes only, and
//                 every changed answer)
//   corrupt       one byte of a call site changed before the proxy loads: nothing installed
//   corrupt_head  one byte of FUN_1015c3f0 at the fix's entry point changed: nothing installed
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
typedef uint32_t (__fastcall *CanMoveFn)(uint32_t unit);

static uint8_t* g_base = nullptr;
static int g_pass = 0, g_fail = 0;
static TestCounters g_counters = nullptr;
static TestKeys g_keys = nullptr;

#define CHECK(cond, name) do { if (cond) { g_pass++; printf("PASS %s\n", name); } \
                               else { g_fail++; printf("FAIL %s\n", name); } } while (0)

// The eight CALL FUN_1015c3f0 sites (docs/re/prologues_kneelfix.txt).
struct Site { uint32_t rva; uint8_t orig[5]; };
static const int SITES = 8;
static const Site kSites[SITES] = {
    { 0x4008EA, { 0xE8, 0x01, 0xBB, 0xD5, 0xFF } },
    { 0x400993, { 0xE8, 0x58, 0xBA, 0xD5, 0xFF } },
    { 0x400ED3, { 0xE8, 0x18, 0xB5, 0xD5, 0xFF } },
    { 0x401D4F, { 0xE8, 0x9C, 0xA6, 0xD5, 0xFF } },
    { 0x402469, { 0xE8, 0x82, 0x9F, 0xD5, 0xFF } },
    { 0x43682B, { 0xE8, 0xC0, 0x5B, 0xD2, 0xFF } },
    { 0x436872, { 0xE8, 0x79, 0x5B, 0xD2, 0xFF } },
    { 0x48C817, { 0xE8, 0xD4, 0xFB, 0xCC, 0xFF } },
};
static const uint32_t RVA_RESUME = 0x15C40A;   // FUN_1015c3f0+0x1A, the fix's entry point

// counters exported by the proxy (KneelFixTestCounters)
enum { C_PROBE = 0, C_FIXON, C_INSTALLED, C_CALLS, C_KNEEL_NO, C_CHANGED, C_CHECKED, C_MISMATCH, C_SELFOFF, C_PATCHED,
       C_ROWS, C_COUNT };
static long C(int i) { long c[C_COUNT + 8] = {}; g_counters(c); return c[i]; }

static void Patch(uint8_t* at, const void* bytes, size_t n) {
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
}

static uint8_t* Zalloc(size_t n) { return (uint8_t*)calloc(1, n); }
static void Put32(uint8_t* p, uint32_t off, uint32_t v) { memcpy(p + off, &v, 4); }
static uint32_t Addr(const void* p) { return (uint32_t)(uintptr_t)p; }

static uint8_t* Code(const uint8_t* bytes, size_t n) {
    uint8_t* c = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(c, bytes, n);
    return c;
}

// Bit test, __thiscall (ECX = the set, one stack argument): bit 0x39 -> byte [ECX+4], else 0.
//   mov eax,[esp+4]; cmp eax,0x39; jne +6; mov al,[ecx+4]; ret 4; xor al,al; ret 4
static const uint8_t kBitTest[] = { 0x8B, 0x44, 0x24, 0x04, 0x83, 0xF8, 0x39, 0x75, 0x06, 0x8A, 0x41, 0x04,
                                    0xC2, 0x04, 0x00, 0x32, 0xC0, 0xC2, 0x04, 0x00 };
// The same, flipping the byte after each bit-0x39 answer (an inconsistent set for the self-check).
//   ... mov al,[ecx+4]; xor byte ptr [ecx+4],1; ret 4; xor al,al; ret 4
static const uint8_t kBitFlip[] = { 0x8B, 0x44, 0x24, 0x04, 0x83, 0xF8, 0x39, 0x75, 0x0A, 0x8A, 0x41, 0x04,
                                    0x80, 0x71, 0x04, 0x01, 0xC2, 0x04, 0x00, 0x32, 0xC0, 0xC2, 0x04, 0x00 };
static const uint8_t kRetNo[] = { 0x32, 0xC0, 0xC3 };   // xor al,al; ret (top behaviour vfunc +0xDC: cannot move)

static uint8_t* g_bitTest = nullptr;
static uint8_t* g_bitFlip = nullptr;
static uint8_t* g_retNo = nullptr;

// Fake battle unit: ability set at +0xED4 (object: vtable, kneel byte at +4), phase chain at +0x14,
// behaviour stack +0xB9C.. with the count at +0xBA8.
struct Fake { uint8_t* u; uint8_t* phase; };
static Fake MakeUnit(bool kneel, uint32_t phase, bool behaviourRefuses, bool flip = false) {
    Fake f = {};
    f.u = Zalloc(0x1000);
    // the set is embedded at +0xED4: the game calls [[unit+0xED4]+4] with ECX = unit+0xED4
    uint8_t* vt = Zalloc(0x20);
    Put32(vt, 4, Addr(flip ? g_bitFlip : g_bitTest));
    Put32(f.u, 0xED4, Addr(vt));
    f.u[0xED4 + 4] = kneel ? 1 : 0;
    uint8_t* a = Zalloc(0x40);
    uint8_t* b = Zalloc(0x100);
    f.phase = Zalloc(0x40);
    Put32(f.u, 0x14, Addr(a));
    Put32(a, 8, Addr(b));
    Put32(b, 0xB4, Addr(f.phase));
    Put32(f.phase, 0x14, phase);
    if (behaviourRefuses) {
        uint8_t* tvt = Zalloc(0x200);
        Put32(tvt, 0xDC, Addr(g_retNo));
        uint8_t* task = Zalloc(0x10);
        Put32(task, 0, Addr(tvt));
        Put32(f.u, 0xBA8, 1);
        Put32(f.u, 0xB9C + 4, Addr(task));
    }
    return f;
}

static uint32_t Ask(int s, const Fake& f) {
    uint32_t at = Addr(g_base) + kSites[s].rva;
    int32_t rel;
    memcpy(&rel, g_base + kSites[s].rva + 1, 4);
    CanMoveFn fn = (CanMoveFn)(uintptr_t)(at + 5 + rel);
    return fn(Addr(f.u)) & 0xFF;
}
static bool AllSites(const Fake& f, uint32_t want) {
    for (int s = 0; s < SITES; s++) if (Ask(s, f) != want) return false;
    return true;
}
static bool Redirected(int s) { return memcmp(g_base + kSites[s].rva, kSites[s].orig, 5) != 0; }

static void PressI() {
    g_keys(1);
    Sleep(200);
    g_keys(0);
    Sleep(120);
}

static void RunFix() {
    CHECK(C(C_INSTALLED) == 1 && C(C_FIXON) == 1 && C(C_PROBE) == 0, "I1 installed, fix on, probe off");
    bool all = true;
    for (int s = 0; s < SITES; s++) all &= Redirected(s);
    CHECK(all && C(C_PATCHED) == SITES, "I2 all eight call sites redirected");
    Fake free0 = MakeUnit(false, 0, false);
    Fake kneel0 = MakeUnit(true, 0, false);
    Fake busy = MakeUnit(false, 5, true);
    Fake kneelBusy = MakeUnit(true, 5, true);
    CHECK(AllSites(free0, 1), "F1 no kneel fire, phase 0: can move at every site (the game's answer)");
    CHECK(C(C_KNEEL_NO) == 0 && C(C_CHANGED) == 0, "F2 ... nothing counted");
    CHECK(AllSites(busy, 0), "F3 no kneel fire, behaviour refuses: cannot move (the game's answer)");
    CHECK(C(C_CHECKED) == SITES && C(C_MISMATCH) == 0, "F4 ... self-check compared every call, no mismatch");
    CHECK(AllSites(kneel0, 1), "F5 kneel fire on, phase 0: can move at every site (the fix)");
    CHECK(C(C_KNEEL_NO) == SITES && C(C_CHANGED) == SITES, "F6 ... eight refusals by the game, eight changed answers");
    CHECK(kneel0.u[0xED4 + 4] == 1, "F7 ... kneel fire still on");
    CHECK(AllSites(kneelBusy, 0), "F8 kneel fire on, behaviour refuses: still cannot move");
    CHECK(C(C_KNEEL_NO) == 2 * SITES && C(C_CHANGED) == SITES, "F9 ... refused by the game, not changed");
    PressI();
    CHECK(C(C_FIXON) == 0, "K1 I: fix off");
    CHECK(AllSites(kneel0, 0), "K2 fix off: a kneeling unit is refused again (the game's answer)");
    CHECK(AllSites(free0, 1), "K3 fix off: a free unit still moves");
    PressI();
    CHECK(C(C_FIXON) == 1 && AllSites(kneel0, 1), "K4 I: fix on again, the kneeling unit moves");
    Fake flip = MakeUnit(true, 0, false, true);
    uint32_t a = Ask(0, flip);   // the game sees kneel on (no); the fix then sees it off and its test says yes
    CHECK(a == 0 && C(C_MISMATCH) == 1 && C(C_SELFOFF) == 1 && C(C_FIXON) == 0,
          "S1 self-check: inconsistent answers switch the fix off (the game's answer returned)");
    CHECK(AllSites(kneel0, 0), "S2 switched off: the kneeling unit gets the game's answer");
    PressI();
    CHECK(C(C_FIXON) == 0, "S3 I is refused after the self-check");
    all = true;
    for (int s = 0; s < SITES; s++) all &= Redirected(s);
    CHECK(all, "S4 the sites stay redirected (the wrappers pass the game's answer through)");
}

static void RunOff() {
    CHECK(C(C_INSTALLED) == 1 && C(C_FIXON) == 0, "O1 installed, fix off (KNEELFIX_OFF=1)");
    Fake kneel0 = MakeUnit(true, 0, false);
    Fake free0 = MakeUnit(false, 0, false);
    CHECK(AllSites(kneel0, 0), "O2 kneel fire on: refused at every site (the game's answer)");
    CHECK(AllSites(free0, 1), "O3 no kneel fire: can move");
    CHECK(C(C_KNEEL_NO) == SITES && C(C_CHANGED) == 0 && C(C_CHECKED) == 0, "O4 counted, nothing changed, no self-check");
    PressI();
    CHECK(C(C_FIXON) == 1 && AllSites(kneel0, 1), "O5 I: fix on, the kneeling unit moves");
}

static int Lines(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    int n = 0, ch;
    while ((ch = fgetc(f)) != EOF) if (ch == '\n') n++;
    fclose(f);
    return n;
}

static bool LatestRun(char* out) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("kneelfix_runs\\*", &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    char best[MAX_PATH] = "";
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.' && strcmp(fd.cFileName, best) > 0)
            strcpy(best, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (!best[0]) return false;
    sprintf(out, "kneelfix_runs\\%s\\kneel_calls.csv", best);
    return true;
}

static void RunProbe(HMODULE proxy) {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 1 && C(C_FIXON) == 1, "P1 installed, probe on, fix on");
    Fake kneel0 = MakeUnit(true, 0, false);
    Fake free0 = MakeUnit(false, 0, false);
    Ask(0, kneel0);   // changed answer: a row
    Ask(0, kneel0);   // changed again: a row
    Ask(1, kneel0);   // another site: a row
    Ask(0, free0);    // no kneel fire: no row
    CHECK(C(C_ROWS) == 3, "P2 three rows: every changed answer of a kneeling unit, none for a free unit");
    FreeLibrary(proxy);
    char path[MAX_PATH];
    bool have = LatestRun(path);
    CHECK(have, "P3 a run folder kneelfix_runs\\<stamp>\\ was written");
    if (have) CHECK(Lines(path) == 4, "P4 kneel_calls.csv: header + three rows");
}

static void RunCorrupt() {
    CHECK(C(C_INSTALLED) == 0 && C(C_PATCHED) == 0, "X1 nothing installed");
    Fake kneel0 = MakeUnit(true, 0, false);
    CHECK(Ask(0, kneel0) == 0, "X2 a kneeling unit is refused (the game's answer)");
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc < 5) { printf("usage: kneelfix_test <game dll> <proxy> <tag> fix|off|probe|corrupt|corrupt_head\n"); return 2; }
    const char* mode = argv[4];
    bool probe = strcmp(mode, "probe") == 0;
    bool off = strcmp(mode, "off") == 0;
    bool corrupt = strcmp(mode, "corrupt") == 0;
    bool corruptHead = strcmp(mode, "corrupt_head") == 0;
    SetEnvironmentVariableA("KNEELFIX_PROBE", probe ? "1" : nullptr);
    SetEnvironmentVariableA("KNEELFIX_OFF", off ? "1" : nullptr);
    const char* otherProbes[] = { "GUARDFIX_PROBE", "ZOMBIEFIX_PROBE", "GROUPFIX_PROBE", "COLLAPSEFIX_PROBE", "WALLFIX_PROBE",
                                  "SIEGEFIX_PROBE", "FBRFIX_PROBE", "ARTFIX_PROBE", "PATHFIX_CAPTURE" };
    for (const char* v : otherProbes) SetEnvironmentVariableA(v, nullptr);

    HMODULE game = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!game) { printf("cannot map game dll: %lu\n", GetLastError()); return 1; }
    g_base = (uint8_t*)game;
    for (int s = 0; s < SITES; s++)
        if (memcmp(g_base + kSites[s].rva, kSites[s].orig, 5) != 0) { printf("site %X differs\n", kSites[s].rva); return 1; }
    g_bitTest = Code(kBitTest, sizeof(kBitTest));
    g_bitFlip = Code(kBitFlip, sizeof(kBitFlip));
    g_retNo = Code(kRetNo, sizeof(kRetNo));
    if (corrupt) {
        uint8_t b = 0x02;
        Patch(g_base + kSites[3].rva + 1, &b, 1);
    }
    if (corruptHead) {
        uint8_t b = 0x47;   // MOV EAX,[ESI+0x14] -> MOV EAX,[EDI+0x14]
        Patch(g_base + RVA_RESUME + 1, &b, 1);
    }
    static uint8_t before[SITES][5];
    for (int s = 0; s < SITES; s++) memcpy(before[s], g_base + kSites[s].rva, 5);
    HMODULE proxy = LoadLibraryA(argv[2]);
    if (!proxy) { printf("cannot load proxy: %lu\n", GetLastError()); return 1; }
    g_counters = (TestCounters)GetProcAddress(proxy, "_KneelFixTestCounters@4");
    if (!g_counters) g_counters = (TestCounters)GetProcAddress(proxy, "KneelFixTestCounters");
    g_keys = (TestKeys)GetProcAddress(proxy, "_KneelFixTestKeys@4");
    if (!g_keys) g_keys = (TestKeys)GetProcAddress(proxy, "KneelFixTestKeys");
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
        else if (corrupt || corruptHead) RunCorrupt();
        else RunFix();
        FreeLibrary(proxy);
    }
    bool restored = true;
    for (int s = 0; s < SITES; s++) restored &= memcmp(g_base + kSites[s].rva, before[s], 5) == 0;
    CHECK(restored, "U1 after unload every site holds the bytes it had before the proxy loaded");
    printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
