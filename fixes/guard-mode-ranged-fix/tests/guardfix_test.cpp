// guardfix_test.cpp -- offline harness for the guard mode ranged fix (fixes/guard-mode-ranged-fix/src/guardfix.cpp).
//
// Maps the real empire.retail.dll (no game process), loads the proxy and calls the seven redirected
// missile attack task tests through the displacement the proxy wrote into each CALL instruction,
// on fake battle units (unit, group, four men) and fake missile attack tasks. The out-of-range test
// FUN_101aefc0 runs as it is (it reads task +0x2D, +0x170, +0x171); the target test FUN_101b2460
// is replaced in the harness's own mapping by a stub that returns task byte +0x30, so a scene sets
// the game's answer. The battle clock is faked through the pointer path the proxy reads.
//
//   guardfix_test <game dll> <proxy> <tag> fix|off|probe|corrupt
//
//   fix      defaults: the seven calls redirected and fire at will's choice hooked, the transition
//            site untouched; a guard missile unit's fire at will leaves shattered enemies out of its
//            choice (another enemy in range, or none); a guard unit's
//            "intercept" is left to the game for a target never in range and refused once the
//            target was in range; shattered targets count as invalid for a guard unit, a
//            broken one not; an intercept under way is stopped once the target was in range;
//            units not in guard mode and the naval missile task are untouched; I off / on; the
//            self-check switches the fix off after 10 guard bytes that are not 0 / 1 and I is
//            then refused
//   off      GUARDFIX_OFF=1: the game's answers only, until I
//   probe    GUARDFIX_PROBE=1: the transition site patched too; a missile task transition logged
//            with its target, call rows on answer changes, positions and order queues sampled
//   corrupt  one byte of a call site changed before the proxy loads: nothing installed
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
typedef uint32_t (__fastcall *TestFn)(uint32_t task);

static uint8_t* g_base = nullptr;
static int g_pass = 0, g_fail = 0;
static TestCounters g_counters = nullptr;
static TestKeys g_keys = nullptr;

#define CHECK(cond, name) do { if (cond) { g_pass++; printf("PASS %s\n", name); } \
                               else { g_fail++; printf("FAIL %s\n", name); } } while (0)

// The seven CALL sites (docs/re/prologues_guardfix.txt) and the transition probe site.
struct Site { uint32_t rva; uint8_t orig[5]; };
enum { S_OOR_FIRE = 0, S_OOR_UNFORMED, S_OOR_TEST, S_TGT_FIRE, S_TGT_UNFORMED, S_TGT_TEST, S_TGT_INTERCEPT, CALLS,
       S_TRANS = CALLS, S_FAW, S_POST, S_UTASK, SITES };
static const Site kSites[SITES] = {
    { 0x0FA12E, { 0xE8, 0x8D, 0x4E, 0x0B, 0x00 } },
    { 0x0FA1FE, { 0xE8, 0xBD, 0x4D, 0x0B, 0x00 } },
    { 0x0FA32E, { 0xE8, 0x8D, 0x4C, 0x0B, 0x00 } },
    { 0x0FA11B, { 0xE8, 0x40, 0x83, 0x0B, 0x00 } },
    { 0x0FA1EB, { 0xE8, 0x70, 0x82, 0x0B, 0x00 } },
    { 0x0FA31F, { 0xE8, 0x3C, 0x81, 0x0B, 0x00 } },
    { 0x0F808B, { 0xE8, 0xD0, 0xA3, 0x0B, 0x00 } },
    { 0x1374D0B, { 0x89, 0x7E, 0x04, 0x8B, 0xCF } },
    { 0x1752D0, { 0x83, 0xEC, 0x58, 0x8B, 0x4C } },     // entry hook, 7 stolen bytes (then 0x24, 0x60)
    { 0x3DAA80, { 0x56, 0x8B, 0xF1, 0x8B, 0x46 } },     // probe hook, 6 stolen bytes (the sixth is 0x04)
    { 0x12D660, { 0x56, 0xFF, 0x74, 0x24, 0x0C } },     // probe hook (never entered by the harness)
};
// the attack command post: after PUSH ESI; MOV ESI,ECX; MOV EAX,[ESI+4]: pop esi; ret 4
static const uint32_t RVA_POST_STUB = 0x3DAA86;
static const uint8_t kPostStub[] = { 0x5E, 0xC2, 0x04, 0x00 };
// fire at will's choice: after SUB ESP,0x58; MOV ECX,[ESP+0x60] (the list): the list's first entry or 0;
// add esp,0x58; ret (cdecl): mov eax,[ecx+0x2800]; test eax,eax; jz +2; mov eax,[ecx]; add esp,0x58; ret
static const uint32_t RVA_FAW_STUB = 0x1752D7;
static const uint8_t kFawStub[] = { 0x8B, 0x81, 0x00, 0x28, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x02, 0x8B, 0x01,
                                    0x83, 0xC4, 0x58, 0xC3 };
static const uint32_t RVA_TARGET_TEST = 0x1B2460;
static const uint8_t kTargetStub[] = { 0x8A, 0x41, 0x30, 0xC3 };   // mov al,[ecx+0x30]; ret
static const uint32_t RVA_TRANS_STUB = 0x1374D10;                  // after the stolen bytes: ret
static const uint32_t RVA_MISSILE_TASK_VT = 0x15C0F24, RVA_NAVAL_TASK_VT = 0x15C1418;

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

// Where the CALL at a site now goes.
static uint32_t CallTarget(int s) {
    uint32_t at = Addr(g_base) + kSites[s].rva;
    int32_t rel;
    memcpy(&rel, g_base + kSites[s].rva + 1, 4);
    return at + 5 + rel;
}
static bool Redirected(int s) { return memcmp(g_base + kSites[s].rva, kSites[s].orig, 5) != 0; }

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

// --- fake battle unit: unit (morale level +0xCE8, guard +0xEB1, main group +0xB9C), group (men
// +0x18 / +0x1C, its unit +0x510), men (unit +0x214, group +0x3B4, position +0x48) ---------------
struct Fake { uint8_t* u; uint8_t* g; uint8_t* s[4]; };
static Fake MakeUnit(float x, float z, bool guard, uint32_t level = 3) {
    Fake f = {};
    f.u = Zalloc(0x2400);
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
    Put32(f.g, 0x510, Addr(f.u));
    Put32(f.u, 0xB9C, Addr(f.g));
    Put32(f.u, 0xCE8, level);
    f.u[0xEB1] = guard ? 1 : 0;
    return f;
}

// A missile attack task of unit `f` against `target` (vtable, group at +4, target at +0x24, a
// named state in its machine context at +0x18).
static uint8_t* MakeTask(Fake& f, Fake& target, uint32_t vtRva = RVA_MISSILE_TASK_VT) {
    uint8_t* t = Zalloc(0x200);
    Put32(t, 0, Addr(g_base) + vtRva);
    Put32(t, 0x04, Addr(f.g));
    Put32(t, 0x18 + 4, Addr(MakeState("PERFORM_CAN_SHOOT_TEST_STATE")));
    Put32(t, 0x24, Addr(target.u));
    return t;
}

// The game's own answers: out of range (FUN_101aefc0: +0x2D clear and the cached can-shoot test,
// +0x170 done / +0x171 passed, failed) and the target test stub (+0x30).
static void OutOfRange(uint8_t* t, bool oor) { t[0x2D] = 0; t[0x170] = 1; t[0x171] = oor ? 0 : 1; }
static void TargetInvalid(uint8_t* t, bool invalid) { t[0x30] = invalid ? 1 : 0; }

static uint32_t Ask(int s, uint8_t* task) { return ((TestFn)(uintptr_t)CallTarget(s))(Addr(task)) & 0xFF; }

static long C(int i) { long c[32] = {}; g_counters(c); return c[i]; }
enum { C_PROBE = 0, C_FIXON, C_INSTALLED, C_GUARD_YES, C_NOCHASE, C_FRESH, C_SHATTERED, C_STOPPED, C_BADGUARD,
       C_SELFOFF, C_MARKS, C_CALLS, C_TRANS, C_TRANS_LOGGED, C_CALL_ROWS, C_UNITS, C_PATCHED, C_FAW, C_IGNORED,
       C_POSTS };

// thiscall into the game code: ECX = self, one stack argument, callee pops it
static void CallThis1(uint32_t rva, uint32_t self, uint32_t a1) {
    uint32_t fn = Addr(g_base) + rva;
    __asm {
        push a1
        mov ecx, self
        call fn
    }
}

// thiscall into the game code: ECX = self, two stack arguments, callee pops them
static void CallThis2(uint32_t rva, uint32_t self, uint32_t a1, uint32_t a2) {
    uint32_t fn = Addr(g_base) + rva;
    __asm {
        push a2
        push a1
        mov ecx, self
        call fn
    }
}

// Fire at will's choice for `shooter` among `n` enemies (the harness stub returns the list's
// first entry): which enemy, and how many stay in the list.
static uint32_t g_lastCount = 0;
static uint32_t Choose(Fake& shooter, Fake** enemies, int n) {
    uint8_t* list = Zalloc(0x2804);
    for (int i = 0; i < n; i++) Put32(list, 4 * i, Addr(enemies[i]->u));
    Put32(list, 0x2800, n);
    uint32_t fn = Addr(g_base) + kSites[S_FAW].rva, u = Addr(shooter.u), l = Addr(list), r;
    __asm {
        push 0
        push 0
        push 0
        push l
        push u
        call fn
        add esp, 20
        mov r, eax
    }
    g_lastCount = Get32(list, 0x2800);
    return r;
}

static void PressI() {
    g_keys(1);
    Sleep(200);
    g_keys(0);
    Sleep(120);
}

static void SitesScene(bool probe) {
    char name[96];
    for (int s = 0; s < CALLS; s++) {
        uint32_t to = CallTarget(s);
        bool inProxy = to < Addr(g_base) || to >= Addr(g_base) + 0x01D88000;
        snprintf(name, sizeof(name), "I%d call RVA 0x%X redirected into the proxy", s + 1, kSites[s].rva);
        CHECK(Redirected(s) && inProxy, name);
    }
    CHECK((g_base[kSites[S_TRANS].rva] == 0xE9) == probe,
          probe ? "I8 transition site patched (probe)" : "I8 transition site untouched (no probe)");
    CHECK(g_base[kSites[S_FAW].rva] == 0xE9, "I10 fire at will's choice hooked");
    CHECK((g_base[kSites[S_POST].rva] == 0xE9) == probe,
          probe ? "I11 attack command post hooked (probe)" : "I11 attack command post untouched (no probe)");
    CHECK((g_base[kSites[S_UTASK].rva] == 0xE9) == probe,
          probe ? "I12 unit attack task constructor hooked (probe)" : "I12 unit attack task constructor untouched (no probe)");
    CHECK(C(C_PATCHED) == CALLS, "I9 seven calls counted as redirected");
}

// The fix's scenes; `on` = the fix is expected on. Fresh units each time (the in-range memory is
// per unit and target).
static void Scenes(bool on, const char* tag) {
    char n[128];
    Fake a = MakeUnit(10.0f, 10.0f, true);        // guard, the shooter
    Fake e = MakeUnit(80.0f, 10.0f, false);       // its target
    uint8_t* t = MakeTask(a, e);
    long fresh0 = C(C_FRESH), no0 = C(C_NOCHASE), sh0 = C(C_SHATTERED), st0 = C(C_STOPPED);
    // L1: target never in range: the order walks into range as in the game
    OutOfRange(t, true);
    snprintf(n, sizeof(n), "%s L1 guard, target never in range: intercept left to the game", tag);
    CHECK(Ask(S_OOR_TEST, t) == 1 && C(C_FRESH) == fresh0 + (on ? 1 : 0), n);
    // L2: in FIRE the target leaves the range: no chase
    snprintf(n, sizeof(n), "%s L2 guard, FIRE, target out of range: %s", tag, on ? "intercept refused" : "game's answer");
    CHECK(Ask(S_OOR_FIRE, t) == (on ? 0u : 1u) && C(C_NOCHASE) == no0 + (on ? 1 : 0), n);
    snprintf(n, sizeof(n), "%s L3 FIRE_UNFORMED likewise", tag);
    CHECK(Ask(S_OOR_UNFORMED, t) == (on ? 0u : 1u), n);
    // L4: the test state after the unit had the target in range: refused too
    snprintf(n, sizeof(n), "%s L4 guard, can-shoot test after the target was in range: %s", tag, on ? "refused" : "game's answer");
    CHECK(Ask(S_OOR_TEST, t) == (on ? 0u : 1u), n);
    // L5: in range: the game's answer (no intercept)
    OutOfRange(t, false);
    snprintf(n, sizeof(n), "%s L5 target in range: no intercept either way", tag);
    CHECK(Ask(S_OOR_FIRE, t) == 0 && Ask(S_OOR_TEST, t) == 0, n);
    // L6: the order's own no-intercept byte: the game's answer
    OutOfRange(t, true);
    t[0x2D] = 1;
    snprintf(n, sizeof(n), "%s L6 order with +0x2D set: no intercept from the game", tag);
    CHECK(Ask(S_OOR_TEST, t) == 0, n);
    // L7: not in guard mode: untouched
    Fake b = MakeUnit(10.0f, 30.0f, false);
    uint8_t* tb = MakeTask(b, e);
    OutOfRange(tb, true);
    long y0 = C(C_GUARD_YES);
    snprintf(n, sizeof(n), "%s L7 unit not in guard mode: intercept as the game says", tag);
    CHECK(Ask(S_OOR_FIRE, tb) == 1 && Ask(S_OOR_TEST, tb) == 1 && C(C_GUARD_YES) == y0, n);
    // T1..: the target test
    TargetInvalid(t, false);
    Put32(e.u, 0xCE8, 7);
    snprintf(n, sizeof(n), "%s T1 guard, shattered target in FIRE: %s", tag, on ? "invalid (task ends)" : "game's answer");
    CHECK(Ask(S_TGT_FIRE, t) == (on ? 1u : 0u) && C(C_SHATTERED) == sh0 + (on ? 1 : 0), n);
    snprintf(n, sizeof(n), "%s T2 FIRE_UNFORMED, can-shoot test, INTERCEPT likewise", tag);
    CHECK(Ask(S_TGT_UNFORMED, t) == (on ? 1u : 0u) && Ask(S_TGT_TEST, t) == (on ? 1u : 0u) &&
          Ask(S_TGT_INTERCEPT, t) == (on ? 1u : 0u), n);
    Put32(e.u, 0xCE8, 6);
    snprintf(n, sizeof(n), "%s T3 broken (level 6) target: still valid", tag);
    CHECK(Ask(S_TGT_FIRE, t) == 0 && Ask(S_TGT_TEST, t) == 0, n);
    snprintf(n, sizeof(n), "%s T4 INTERCEPT under way, target was in range: %s", tag, on ? "stopped" : "game's answer");
    CHECK(Ask(S_TGT_INTERCEPT, t) == (on ? 1u : 0u) && C(C_STOPPED) == st0 + (on ? 1 : 0), n);
    Fake e2 = MakeUnit(150.0f, 10.0f, false);
    uint8_t* t2 = MakeTask(a, e2);
    TargetInvalid(t2, false);
    snprintf(n, sizeof(n), "%s T5 INTERCEPT towards a target never in range: left to the game", tag);
    CHECK(Ask(S_TGT_INTERCEPT, t2) == 0, n);
    TargetInvalid(t, true);
    snprintf(n, sizeof(n), "%s T6 target already invalid for the game: stays invalid", tag);
    CHECK(Ask(S_TGT_FIRE, t) == 1, n);
    TargetInvalid(t, false);
    Put32(e.u, 0xCE8, 7);
    TargetInvalid(tb, false);
    snprintf(n, sizeof(n), "%s T7 unit not in guard mode, shattered target: valid as the game says", tag);
    CHECK(Ask(S_TGT_FIRE, tb) == 0 && Ask(S_TGT_INTERCEPT, tb) == 0, n);
    uint8_t* tn = MakeTask(a, e, RVA_NAVAL_TASK_VT);
    TargetInvalid(tn, false);
    snprintf(n, sizeof(n), "%s T8 naval missile task in the shared INTERCEPT update: untouched", tag);
    CHECK(Ask(S_TGT_INTERCEPT, tn) == 0, n);
    // A1..: fire at will's choice (a and b are missile units by now)
    long ig0 = C(C_IGNORED), fw0 = C(C_FAW);
    Put32(e.u, 0xCE8, 7);
    Put32(e2.u, 0xCE8, 3);
    Fake* two[2] = { &e, &e2 };
    snprintf(n, sizeof(n), "%s A1 guard missile unit, shattered enemy first in the list: %s", tag,
             on ? "left out, the other enemy taken" : "the game's choice");
    CHECK(Choose(a, two, 2) == (on ? Addr(e2.u) : Addr(e.u)) && g_lastCount == (on ? 1u : 2u) &&
          C(C_IGNORED) == ig0 + (on ? 1 : 0) && C(C_FAW) == fw0 + 1, n);
    Fake* one[1] = { &e };
    snprintf(n, sizeof(n), "%s A2 only a shattered enemy in range: %s", tag, on ? "no target (hold fire)" : "taken");
    CHECK(Choose(a, one, 1) == (on ? 0u : Addr(e.u)) && g_lastCount == (on ? 0u : 1u), n);
    Put32(e.u, 0xCE8, 6);
    snprintf(n, sizeof(n), "%s A3 broken (level 6) enemy: stays in the list", tag);
    CHECK(Choose(a, two, 2) == Addr(e.u) && g_lastCount == 2, n);
    Put32(e.u, 0xCE8, 7);
    snprintf(n, sizeof(n), "%s A4 unit not in guard mode, shattered enemy: stays", tag);
    CHECK(Choose(b, two, 2) == Addr(e.u) && g_lastCount == 2, n);
    Fake m = MakeUnit(10.0f, 50.0f, true);   // guard, never in a missile attack task
    snprintf(n, sizeof(n), "%s A5 guard unit not known as a missile unit: list untouched", tag);
    CHECK(Choose(m, two, 2) == Addr(e.u) && g_lastCount == 2, n);
    snprintf(n, sizeof(n), "%s A6 every choice counted", tag);
    CHECK(C(C_FAW) == fw0 + 5 && C(C_IGNORED) == ig0 + (on ? 2 : 0), n);
}

static void SelfCheckScene() {
    Fake a = MakeUnit(10.0f, 10.0f, true);
    Fake e = MakeUnit(80.0f, 10.0f, false, 7);
    uint8_t* t = MakeTask(a, e);
    TargetInvalid(t, false);
    a.u[0xEB1] = 5;
    for (int i = 0; i < 9; i++) Ask(S_TGT_FIRE, t);
    CHECK(C(C_BADGUARD) == 9 && C(C_SELFOFF) == 0 && C(C_FIXON) == 1, "Q1 nine guard bytes that are not 0 / 1: counted, fix still on");
    CHECK(Ask(S_TGT_FIRE, t) == 0 && C(C_SELFOFF) == 1 && C(C_FIXON) == 0, "Q2 the tenth: fix switched off for the session");
    a.u[0xEB1] = 1;
    CHECK(Ask(S_TGT_FIRE, t) == 0, "Q3 off: a guard unit's shattered target left to the game");
    PressI();
    CHECK(C(C_FIXON) == 0, "Q4 I after the self-check: refused, stays off");
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
    HANDLE h = FindFirstFileA("guardfix_runs\\*", &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    char best[MAX_PATH] = "";
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.' && strcmp(fd.cFileName, best) > 0)
            strcpy(best, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (!best[0]) return false;
    sprintf(out, "guardfix_runs\\%s\\", best);
    return true;
}

static void Transition(uint8_t* ctx, uint8_t* oldState, uint8_t* next) {
    Put32(ctx, 4, Addr(oldState));
    uint32_t c = Addr(ctx), nx = Addr(next);
    void* site = g_base + kSites[S_TRANS].rva;
    __asm {
        push esi
        push edi
        mov esi, c
        mov edi, nx
        call site
        pop edi
        pop esi
    }
}

static void RunProbe(HMODULE proxy) {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 1 && C(C_FIXON) == 1, "P0 installed, probe on, fix on");
    SitesScene(true);
    SetTick(0);
    Fake a = MakeUnit(10.0f, 10.0f, true);
    Fake e = MakeUnit(70.0f, 10.0f, false, 4);
    uint8_t* t = MakeTask(a, e);
    uint8_t* fire = MakeState("FIRE_STATE");
    Transition(t + 0x18, (uint8_t*)Get32(t, 0x18 + 4), fire);
    CHECK(Get32(t, 0x18 + 4) == Addr(fire), "T1 the stolen instructions ran: the context holds the new state");
    CHECK(C(C_TRANS) == 1 && C(C_TRANS_LOGGED) == 1 && C(C_UNITS) == 2, "T2 missile task transition logged, shooter and target registered");
    // call rows: one per change of answer
    OutOfRange(t, true);
    Ask(S_OOR_FIRE, t);   // guard: refused (logged: answer differs from the game)
    Ask(S_OOR_FIRE, t);   // logged again: the fix changed the answer
    OutOfRange(t, false);
    Ask(S_OOR_FIRE, t);   // in range: a change
    Ask(S_OOR_FIRE, t);   // no change: not logged
    CHECK(C(C_CALL_ROWS) == 3, "C1 call rows: every changed answer and every answer the fix changed");
    uint32_t cmd[4] = { Addr(e.u), 0, 0, 0x10001 };
    CallThis1(kSites[S_POST].rva, Addr(a.g), Addr(cmd));
    CHECK(C(C_POSTS) == 1, "C2 attack command post logged");
    SetTick(100);
    Sleep(300);
    SetTick(106);
    Sleep(300);
    FreeLibrary(proxy);
    char dir[MAX_PATH];
    bool have = LatestRun(dir);
    CHECK(have, "F1 a run folder guardfix_runs\\<stamp>\\ was written");
    if (!have) return;
    char v[256], key[16];
    sprintf(key, "%X", Addr(t + 0x18));
    CHECK(Lines(dir, "guard_states.csv") == 2, "F2 guard_states.csv: header + the transition");
    CHECK(Field(dir, "guard_states.csv", 4, key, 2, v, sizeof(v)) && strcmp(v, "T") == 0, "F3 task machine: kind T");
    CHECK(Field(dir, "guard_states.csv", 4, key, 3, v, sizeof(v)) && strcmp(v, "15C0F24") == 0, "F4 ... the missile attack task");
    CHECK(Field(dir, "guard_states.csv", 4, key, 7, v, sizeof(v)) && strcmp(v, "1") == 0, "F5 ... guard 1");
    CHECK(Field(dir, "guard_states.csv", 4, key, 13, v, sizeof(v)) && strcmp(v, "FIRE_STATE") == 0, "F6 ... new state named");
    CHECK(Field(dir, "guard_states.csv", 4, key, 15, v, sizeof(v)) && strcmp(v, "4") == 0, "F7 ... target level 4");
    CHECK(Field(dir, "guard_states.csv", 4, key, 19, v, sizeof(v)) && strcmp(v, "60.0") == 0, "F8 ... distance 60 m");
    CHECK(Lines(dir, "guard_calls.csv") == 4, "F9 guard_calls.csv: header + 3 rows");
    CHECK(Field(dir, "guard_calls.csv", 10, "1", 11, v, sizeof(v)) && strcmp(v, "0") == 0, "F10 a row: game 1, answer 0");
    CHECK(Lines(dir, "guard_pos.csv") >= 3, "F11 guard_pos.csv: both units sampled");
    CHECK(Lines(dir, "guard_orders.csv") >= 3, "F12 guard_orders.csv: both units' (empty) order queues");
    CHECK(Lines(dir, "guard_posts.csv") == 2, "F13 guard_posts.csv: header + the post");
    CHECK(Field(dir, "guard_posts.csv", 10, "1", 12, v, sizeof(v)) && strcmp(v, "4") == 0, "F14 ... guard unit, target level 4");
}

static void RunFix() {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 0 && C(C_FIXON) == 1, "X1 installed, probe off, fix on at launch");
    SitesScene(false);
    Scenes(true, "on");
    PressI();
    CHECK(C(C_FIXON) == 0, "K1 I: fix off");
    Scenes(false, "off");
    PressI();
    CHECK(C(C_FIXON) == 1, "K2 I again: on");
    Scenes(true, "on2");
    SelfCheckScene();
}

static void RunOff() {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 0 && C(C_FIXON) == 0, "X1 installed, probe off, fix off (GUARDFIX_OFF=1)");
    SitesScene(false);
    Scenes(false, "off");
    PressI();
    CHECK(C(C_FIXON) == 1, "K1 I: fix on");
    Scenes(true, "on");
}

static void RunCorrupt() {
    CHECK(C(C_INSTALLED) == 0, "X1 a changed byte in a call site: not installed");
    bool none = true;
    for (int s = 0; s < CALLS; s++) none &= s == S_OOR_TEST || !Redirected(s);
    CHECK(none && C(C_PATCHED) == 0 && g_base[kSites[S_FAW].rva] == 0x83 && g_base[kSites[S_POST].rva] == 0x56,
          "X2 no call redirected, no hook");
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc < 5) { printf("usage: guardfix_test <game dll> <proxy> <tag> fix|off|probe|corrupt\n"); return 2; }
    const char* mode = argv[4];
    bool probe = strcmp(mode, "probe") == 0;
    bool off = strcmp(mode, "off") == 0;
    bool corrupt = strcmp(mode, "corrupt") == 0;
    SetEnvironmentVariableA("GUARDFIX_PROBE", probe ? "1" : nullptr);
    SetEnvironmentVariableA("GUARDFIX_OFF", off ? "1" : nullptr);
    SetEnvironmentVariableA("ZOMBIEFIX_PROBE", nullptr);
    SetEnvironmentVariableA("GROUPFIX_PROBE", nullptr);
    SetEnvironmentVariableA("COLLAPSEFIX_PROBE", nullptr);
    SetEnvironmentVariableA("WALLFIX_PROBE", nullptr);
    SetEnvironmentVariableA("SIEGEFIX_PROBE", nullptr);
    SetEnvironmentVariableA("FBRFIX_PROBE", nullptr);
    SetEnvironmentVariableA("ARTFIX_PROBE", nullptr);
    SetEnvironmentVariableA("PATHFIX_CAPTURE", nullptr);

    HMODULE game = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!game) { printf("cannot map game dll: %lu\n", GetLastError()); return 1; }
    g_base = (uint8_t*)game;
    for (int s = 0; s < SITES; s++)
        if (memcmp(g_base + kSites[s].rva, kSites[s].orig, 5) != 0) { printf("site %X differs\n", kSites[s].rva); return 1; }
    Patch(g_base + RVA_TARGET_TEST, kTargetStub, sizeof(kTargetStub));
    Patch(g_base + RVA_FAW_STUB, kFawStub, sizeof(kFawStub));
    Patch(g_base + RVA_POST_STUB, kPostStub, sizeof(kPostStub));
    uint8_t ret = 0xC3;
    Patch(g_base + RVA_TRANS_STUB, &ret, 1);
    if (corrupt) {
        uint8_t b = 0x8E;
        Patch(g_base + kSites[S_OOR_TEST].rva + 1, &b, 1);
    }
    static uint8_t before[SITES][5];
    for (int s = 0; s < SITES; s++) memcpy(before[s], g_base + kSites[s].rva, 5);
    MakeClock();
    HMODULE proxy = LoadLibraryA(argv[2]);
    if (!proxy) { printf("cannot load proxy: %lu\n", GetLastError()); return 1; }
    g_counters = (TestCounters)GetProcAddress(proxy, "_GuardFixTestCounters@4");
    if (!g_counters) g_counters = (TestCounters)GetProcAddress(proxy, "GuardFixTestCounters");
    g_keys = (TestKeys)GetProcAddress(proxy, "_GuardFixTestKeys@4");
    if (!g_keys) g_keys = (TestKeys)GetProcAddress(proxy, "GuardFixTestKeys");
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
    for (int s = 0; s < SITES; s++) restored &= memcmp(g_base + kSites[s].rva, before[s], 5) == 0;
    CHECK(restored, "U1 after unload every site holds the bytes it had before the proxy loaded");
    printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
