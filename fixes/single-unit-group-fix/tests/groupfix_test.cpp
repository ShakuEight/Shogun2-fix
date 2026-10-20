// groupfix_test.cpp -- offline harness for the single unit group fix
// (fixes/single-unit-group-fix/src/groupfix.cpp).
//
// Maps the real empire.retail.dll (no game process), loads the proxy and runs the game's own
// "group of the selection" function (FUN_10f89a40) on a fake selection manager whose virtual
// group-create (+0x20) and card refresh (+0x24) are harness functions. Slot -1 keeps the function
// away from the number-slot bookkeeping (game containers). The probe-only sites are stubbed right
// after their stolen bytes (they return at once) and entered directly.
//
//   groupfix_test <game dll> <proxy> <tag> fix|off|probe|corrupt
//
//   fix      defaults: the count immediate is 0, the dev log hook patched, no probe site; one
//            selected unit is grouped, two are, none is not; I off: one unit is refused again
//            (the game's 1), two still grouped; I on again
//   off      GROUPFIX_OFF=1: the game's 1 until I, then 0
//   probe    GROUPFIX_PROBE=1: every site patched; the groupings and each probe site counted and
//            logged (CSV checked)
//   corrupt  one byte of the fix site changed before the proxy loads: nothing installed
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

static const uint32_t RVA_GROUP_SEL = 0xF89A40;   // FUN_10f89a40
static const uint32_t RVA_IMM = 0xF89A46;         // the immediate of CMP dword [ESI+0x28],1
static const uint32_t RVA_SELMGR = 0x01BBEAD8;    // [0x11bbead8]

struct Site { uint32_t rva; int origLen; uint8_t orig[10]; uint8_t stub[16]; int stubLen; bool probeOnly; };
enum { S_FIX = 0, S_MADE, S_TOGGLE, S_USE, S_LOCK, S_FINAL, S_REMOVE, S_ADD, S_DESTROY, S_FRAMES, S_EVCREATE,
       S_EVSEL, S_BTNKEY, S_CARDKEY, S_FIX2, S_FRAMESIZE, S_SETSTATE, S_SETVIS, S_BTNCTOR, S_FIX3, SITES };
static const Site kSites[SITES] = {
    // the fix site (immediate only) and the dev log hook: the game's code runs on
    { 0xF89A43, 6, { 0x83, 0x7E, 0x28, 0x01, 0x76, 0x3E }, { 0 }, 0, false },
    { 0xF89A49, 8, { 0x8B, 0x06, 0x53, 0x57, 0x8B, 0x7C, 0x24, 0x10 }, { 0 }, 0, false },
    // G: after PUSH ESI; MOV ESI,ECX; CMP: pop esi; ret
    { 0xF9DE40, 7, { 0x56, 0x8B, 0xF1, 0x80, 0x7E, 0x20, 0x00 }, { 0x5E, 0xC3 }, 2, true },
    // Ctrl+number: after SUB ESP,0x10; CMP: add esp,0x10; ret 4
    { 0xF91E40, 10, { 0x83, 0xEC, 0x10, 0x80, 0x3D, 0x4F, 0xE8, 0xBB, 0x11, 0x01 }, { 0x83, 0xC4, 0x10, 0xC2, 0x04, 0x00 }, 6, true },
    // lock button: after MOV EAX,[abs]: ret 4
    { 0x1211D20, 5, { 0xA1, 0x80, 0x81, 0xBC, 0x11 }, { 0xC2, 0x04, 0x00 }, 3, true },
    // group finalise: after SUB ESP,0x34; PUSH ESI; MOV ESI,ECX: pop esi; add esp,0x34; ret
    { 0x1FFA70, 6, { 0x83, 0xEC, 0x34, 0x56, 0x8B, 0xF1 }, { 0x5E, 0x83, 0xC4, 0x34, 0xC3 }, 5, true },
    // unit remove: after SUB ESP,0xC; PUSH EBX; MOV EBX,[ESP+0x14]: pop ebx; add esp,0xC; ret 4
    { 0x472ED0, 8, { 0x83, 0xEC, 0x0C, 0x53, 0x8B, 0x5C, 0x24, 0x14 }, { 0x5B, 0x83, 0xC4, 0x0C, 0xC2, 0x04, 0x00 }, 7, true },
    // unit add: after PUSH ESI; MOV ESI,[ESP+8]: pop esi; ret 4
    { 0x41DCC0, 5, { 0x56, 0x8B, 0x74, 0x24, 0x08 }, { 0x5E, 0xC2, 0x04, 0x00 }, 4, true },
    // group destroy: after PUSH EBX; PUSH EBP; MOV EBX,ECX; PUSH ESI: pop esi; pop ebp; pop ebx; xor al,al; ret 4
    { 0x169170, 5, { 0x53, 0x55, 0x8B, 0xD9, 0x56 }, { 0x5E, 0x5D, 0x5B, 0x32, 0xC0, 0xC2, 0x04, 0x00 }, 8, true },
    // group frames: after SUB ESP,0x34; PUSH EBX; PUSH ESI: pop esi; pop ebx; add esp,0x34; ret
    { 0x10E7280, 5, { 0x83, 0xEC, 0x34, 0x53, 0x56 }, { 0x5E, 0x5B, 0x83, 0xC4, 0x34, 0xC3 }, 6, true },
    // UI event CreateGroup: after SUB ESP,0x48; PUSH ESI; PUSH 0: add esp,4; pop esi; add esp,0x48; ret 8
    { 0xF93FD0, 6, { 0x83, 0xEC, 0x48, 0x56, 0x6A, 0x00 }, { 0x83, 0xC4, 0x04, 0x5E, 0x83, 0xC4, 0x48, 0xC2, 0x08, 0x00 }, 10, true },
    // UI event GroupSelectionChanged: after SUB ESP,0x24; PUSH 0: add esp,0x28; ret 8
    { 0xF8DE60, 5, { 0x83, 0xEC, 0x24, 0x6A, 0x00 }, { 0x83, 0xC4, 0x28, 0xC2, 0x08, 0x00 }, 6, true },
    // button shortcut: after SUB ESP,8; CMP: add esp,8; ret 0xC
    { 0x1065720, 8, { 0x83, 0xEC, 0x08, 0x80, 0x7C, 0x24, 0x14, 0x00 }, { 0x83, 0xC4, 0x08, 0xC2, 0x0C, 0x00 }, 6, true },
    // card manager shortcut: after SUB ESP,0x10; PUSH EBX; XOR EBX,EBX: pop ebx; add esp,0x10; ret 8
    { 0xFA37E0, 6, { 0x83, 0xEC, 0x10, 0x53, 0x33, 0xDB }, { 0x5B, 0x83, 0xC4, 0x10, 0xC2, 0x08, 0x00 }, 7, true },
    // the frame's count test (immediate only) and the frame placement entry (the fix hook): game code runs
    { 0x122347F, 7, { 0x83, 0x7D, 0x04, 0x01, 0x8B, 0x4B, 0x04 }, { 0 }, 0, false },
    { 0x1223470, 5, { 0x83, 0xEC, 0x50, 0x53, 0x55 }, { 0 }, 0, false },
    // component state set: after SUB ESP,0x18; PUSH EBX; PUSH EBP: pop ebp; pop ebx; add esp,0x18; ret 4
    { 0xF9BF20, 5, { 0x83, 0xEC, 0x18, 0x53, 0x55 }, { 0x5D, 0x5B, 0x83, 0xC4, 0x18, 0xC2, 0x04, 0x00 }, 8, true },
    // component visible: after SUB ESP,0xC; PUSH EBX; MOV EBX,[ESP+0x14]: pop ebx; add esp,0xC; ret 4
    // (also keeps the frame function's hide call harmless)
    { 0xF9C360, 8, { 0x83, 0xEC, 0x0C, 0x53, 0x8B, 0x5C, 0x24, 0x14 }, { 0x5B, 0x83, 0xC4, 0x0C, 0xC2, 0x04, 0x00 }, 7, true },
    // group button constructor: after SUB ESP,0x30; PUSH ESI; PUSH EDI: mov eax,ecx; pop edi; pop esi; add esp,0x30; ret 4
    { 0x1216B20, 5, { 0x83, 0xEC, 0x30, 0x56, 0x57 }, { 0x8B, 0xC1, 0x5F, 0x5E, 0x83, 0xC4, 0x30, 0xC2, 0x04, 0x00 }, 10, true },
    // the group button state test (immediate only): CMP dword [ESP+0x38],1; JBE
    { 0x1101A36, 7, { 0x83, 0x7C, 0x24, 0x38, 0x01, 0x76, 0x21 }, { 0 }, 0, false },
};
static const uint32_t RVA_IMM3 = 0x1101A3A;       // the immediate of the group button state test
static const uint32_t RVA_IMM2 = 0x1223482;       // the immediate of CMP dword [EBP+4],1 in FUN_11223470
static const uint32_t RVA_FRAME_DRAW = 0x1223497; // FUN_11223470's drawing path (the JA target): harness stub there

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
static bool Hooked(int s) { return g_base[kSites[s].rva] == 0xE9; }
static bool IsImmSite(int s) { return s == S_FIX || s == S_FIX2 || s == S_FIX3; }
static uint8_t Imm() { return g_base[RVA_IMM]; }
static uint8_t Imm2() { return g_base[RVA_IMM2]; }
static uint8_t Imm3() { return g_base[RVA_IMM3]; }
static bool Imms(int v) { return Imm() == v && Imm2() == v && Imm3() == v; }

// --- fake selection manager: vtable +0x20 group create, +0x24 card refresh; selected count +0x28 --
static uint8_t* g_mgr = nullptr;
static int g_creates = 0, g_refreshes = 0;
static uint32_t g_lastSel = 0, g_lastSlot = 0;
static uint32_t __fastcall FakeCreate(void* self, void*, uint32_t sel, uint32_t slot) {
    if (self == g_mgr) { g_creates++; g_lastSel = sel; g_lastSlot = slot; }
    return 0;
}
static uint32_t __fastcall FakeRefresh(void* self, void*, uint32_t, uint32_t) {
    if (self == g_mgr) g_refreshes++;
    return 0;
}
static void MakeManager() {
    uint32_t* vt = (uint32_t*)Zalloc(4 * 32);
    vt[0x20 / 4] = Addr((void*)FakeCreate);
    vt[0x24 / 4] = Addr((void*)FakeRefresh);
    g_mgr = Zalloc(0x200);
    Put32(g_mgr, 0x00, Addr(vt));
    uint32_t* units = (uint32_t*)Zalloc(4 * 8);
    for (int i = 0; i < 8; i++) units[i] = Addr(Zalloc(0x40));
    Put32(g_mgr, 0x2C, Addr(units));
    uint32_t p = Addr(g_mgr);
    Patch(g_base + RVA_SELMGR, &p, 4);
}

// thiscall into the game code: ECX = self, stack arguments pushed right to left, callee pops them
static uint32_t CallThis(uint32_t rva, uint32_t self, int n, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
    uint32_t fn = Addr(g_base) + rva, r;
    uint32_t args[3] = { a1, a2, a3 };
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
enum { C_PROBE = 0, C_FIXON, C_INSTALLED, C_CREATED, C_SINGLES, C_TOGGLE, C_USE, C_LOCK, C_FINAL, C_REMOVE, C_ADD,
       C_DESTROY, C_ROWS, C_FRAMES, C_EVCREATE, C_EVSEL, C_BTNKEY, C_CARDKEY, C_UIROWS, C_FRAMES1, C_STATE, C_VIS,
       C_BTNCTOR };

static void PressI() {
    g_keys(1);
    Sleep(200);
    g_keys(0);
    Sleep(120);
}

// The game's group of the selection with n units selected (slot -1: no number slot).
static bool Group(uint32_t n) {
    Put32(g_mgr, 0x28, n);
    int c0 = g_creates, r0 = g_refreshes;
    CallThis(RVA_GROUP_SEL, Addr(g_mgr), 1, 0xFFFFFFFFu);
    bool made = g_creates == c0 + 1 && g_refreshes == r0 + 1;
    if (made && (g_lastSel != Addr(g_mgr) + 0x24 || g_lastSlot != 0xFFFFFFFFu)) made = false;
    return made;
}

static void SitesScene(bool probe, bool fixOn) {
    char name[96];
    snprintf(name, sizeof(name), "I1 the three count immediates are %d", fixOn ? 0 : 1);
    CHECK(Imms(fixOn ? 0 : 1), name);
    CHECK(g_base[kSites[S_FIX].rva] == 0x83 && g_base[kSites[S_FIX2].rva] == 0x83 && g_base[kSites[S_FIX3].rva] == 0x83,
          "I2 the fix sites themselves are not hooked (immediate patches only)");
    for (int s = S_MADE; s < SITES; s++) {
        if (IsImmSite(s)) continue;
        bool want = probe || !kSites[s].probeOnly;
        snprintf(name, sizeof(name), "I%d site RVA 0x%X %s", s + 2, kSites[s].rva, want ? "patched" : "untouched");
        CHECK(Hooked(s) == want, name);
    }
}

static void GroupScenes(bool fixOn, const char* tag) {
    char name[128];
    long s0 = C(C_SINGLES), m0 = C(C_CREATED);
    snprintf(name, sizeof(name), "G1%s no unit selected: no group", tag);
    CHECK(!Group(0), name);
    bool one = Group(1);
    if (fixOn) {
        snprintf(name, sizeof(name), "G2%s fix on, one unit selected: the game makes the group (create + refresh)", tag);
        CHECK(one && C(C_SINGLES) == s0 + 1, name);
    } else {
        snprintf(name, sizeof(name), "G2%s fix off, one unit selected: refused as in the game", tag);
        CHECK(!one && C(C_SINGLES) == s0, name);
    }
    snprintf(name, sizeof(name), "G3%s two units selected: grouped either way, not counted as a single", tag);
    CHECK(Group(2) && C(C_SINGLES) == s0 + (fixOn ? 1 : 0), name);
    snprintf(name, sizeof(name), "G4%s dev log hook counted every group made", tag);
    CHECK(C(C_CREATED) == m0 + (fixOn ? 2 : 1), name);
}

// --- the group frame: FUN_11223470 (ECX = frame, units list, two-row flag) runs the game's own
// count test; the drawing path is a stub that records the flag it got --------------------------
static volatile uint32_t g_drawn = 0, g_drawFlag = 0xFFFFFFFFu;
static uint8_t* g_frame = nullptr;
static void MakeFrameStub() {
    uint8_t st[32];
    int k = 0;
    uint32_t a1 = Addr((void*)&g_drawFlag), a2 = Addr((void*)&g_drawn);
    st[k++] = 0x8B; st[k++] = 0x44; st[k++] = 0x24; st[k++] = 0x60;         // mov eax,[esp+0x60] (arg2)
    st[k++] = 0xA3; memcpy(st + k, &a1, 4); k += 4;                          // mov [g_drawFlag],eax
    st[k++] = 0xC7; st[k++] = 0x05; memcpy(st + k, &a2, 4); k += 4;         // mov dword [g_drawn],1
    st[k++] = 0x01; st[k++] = 0; st[k++] = 0; st[k++] = 0;
    st[k++] = 0x5D; st[k++] = 0x5B;                                          // pop ebp; pop ebx
    st[k++] = 0x83; st[k++] = 0xC4; st[k++] = 0x50;                          // add esp,0x50
    st[k++] = 0xC2; st[k++] = 0x08; st[k++] = 0x00;                          // ret 8
    Patch(g_base + RVA_FRAME_DRAW, st, k);
    g_frame = Zalloc(0x100);
    Put32(g_frame, 4, Addr(Zalloc(0x200)));   // the frame's component (hidden through the stubbed visible call)
}
// One frame placement for a group of n units with the two-row flag; true when drawing was reached.
static bool Frame(uint32_t n, uint32_t flag) {
    uint8_t* list = Zalloc(0x20);
    Put32(list, 4, n);
    Put32(list, 8, Addr(Zalloc(0x20)));
    g_drawn = 0;
    g_drawFlag = 0xFFFFFFFFu;
    CallThis(0x1223470, Addr(g_frame), 2, Addr(list), flag);
    return g_drawn != 0;
}
static void FrameScenes(bool fixOn, const char* tag) {
    char name[128];
    long f0 = C(C_FRAMES1);
    snprintf(name, sizeof(name), "R1%s frame of an empty group: hidden", tag);
    CHECK(!Frame(0, 1), name);
    bool one = Frame(1, 1);
    if (fixOn) {
        snprintf(name, sizeof(name), "R2%s fix on, one-unit group, two-row flag: frame drawn, single-row branch forced", tag);
        CHECK(one && (g_drawFlag & 0xFF) == 0 && C(C_FRAMES1) == f0 + 1, name);
    } else {
        snprintf(name, sizeof(name), "R2%s fix off, one-unit group: frame hidden as in the game", tag);
        CHECK(!one && C(C_FRAMES1) == f0, name);
    }
    snprintf(name, sizeof(name), "R3%s two-unit group, two-row flag: drawn, flag untouched", tag);
    CHECK(Frame(2, 1) && (g_drawFlag & 0xFF) == 1, name);
    snprintf(name, sizeof(name), "R4%s three-unit group, single row: drawn, flag untouched", tag);
    CHECK(Frame(3, 0) && (g_drawFlag & 0xFF) == 0, name);
}

static bool LatestRun(char* out) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("groupfix_runs\\*", &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    char best[MAX_PATH] = "";
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.' && strcmp(fd.cFileName, best) > 0)
            strcpy(best, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (!best[0]) return false;
    sprintf(out, "groupfix_runs\\%s\\", best);
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
    static char line[0x2000];
    snprintf(path, sizeof(path), "%s%s", dir, name);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    bool found = false, header = true;
    while (fgets(line, sizeof(line), f) && !found) {
        if (header) { header = false; continue; }
        char* cols[32];
        int n = 0;
        cols[n++] = line;
        for (char* p = line; *p && n < 32; p++)
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
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 1 && C(C_FIXON) == 1, "P0 installed, probe on, fix on");
    SitesScene(true, true);
    GroupScenes(true, "a");
    Put32(g_mgr, 0x28, 1);
    CallThis(kSites[S_TOGGLE].rva, Addr(g_mgr), 0);
    CHECK(C(C_TOGGLE) == 1, "W1 G shortcut counted");
    CallThis(kSites[S_USE].rva, Addr(g_mgr), 1, 3);
    CHECK(C(C_USE) == 1, "W2 Ctrl+number counted");
    CallThis(kSites[S_LOCK].rva, Addr(Zalloc(0x40)), 1, 0);
    CHECK(C(C_LOCK) == 1, "W3 lock button counted");
    uint8_t* grp = Zalloc(0x100);
    Put32(grp, 0x20, 1);
    uint8_t* unit = Zalloc(0x40);
    CallThis(kSites[S_FINAL].rva, Addr(grp), 0);
    CHECK(C(C_FINAL) == 1, "W4 engine group finalise counted");
    CallThis(kSites[S_ADD].rva, Addr(grp), 1, Addr(unit));
    CHECK(C(C_ADD) == 1, "W5 engine unit add counted");
    CallThis(kSites[S_REMOVE].rva, Addr(grp), 1, Addr(unit));
    CHECK(C(C_REMOVE) == 1, "W6 engine unit remove counted");
    uint32_t r = CallThis(kSites[S_DESTROY].rva, Addr(Zalloc(0x200)), 1, 7);
    CHECK(C(C_DESTROY) == 1 && (r & 0xFF) == 0, "W7 engine group destroy counted, the game's result came back");
    uint8_t* panel = Zalloc(0x100);
    Put32(panel, 0x70, 4);
    CallThis(kSites[S_FRAMES].rva, Addr(panel), 0);
    CHECK(C(C_FRAMES) == 1, "W8 group frame rebuild counted");
    CallThis(kSites[S_EVCREATE].rva, Addr(Zalloc(0x40)), 2, 1, 2);
    CHECK(C(C_EVCREATE) == 1, "W9 UI event CreateGroup counted");
    CallThis(kSites[S_EVSEL].rva, Addr(Zalloc(0x40)), 2, 1, 2);
    CHECK(C(C_EVSEL) == 1, "W10 UI event GroupSelectionChanged counted");
    uint8_t* other = Zalloc(0x40);
    Put32(other, 0, Addr(Zalloc(0x100)));
    CallThis(kSites[S_BTNKEY].rva, Addr(other), 3, 0, 0, 1);
    CHECK(C(C_BTNKEY) == 0, "W11 another button's shortcut: not logged");
    uint8_t* gbtn = Zalloc(0x40);
    Put32(gbtn, 0, Addr(g_base) + 0x01725208);
    CallThis(kSites[S_BTNKEY].rva, Addr(gbtn), 3, 0, 0, 1);
    CHECK(C(C_BTNKEY) == 1, "W12 the group button's shortcut: logged");
    CallThis(kSites[S_CARDKEY].rva, Addr(Zalloc(0x40)), 2, 5, 6);
    CHECK(C(C_CARDKEY) == 1, "W13 card manager shortcut counted");
    uint8_t* comp = Zalloc(0x200);
    uint8_t* gb = Zalloc(0x80);
    CallThis(kSites[S_BTNCTOR].rva, Addr(gb), 1, Addr(comp));
    CHECK(C(C_BTNCTOR) == 1, "W14 group button constructor counted (its component noted)");
    // a state name string object: +0 length, +8 text
    uint8_t* str = Zalloc(0x20);
    static char inactive[] = "inactive";
    Put32(str, 0, 8);
    Put32(str, 8, Addr(inactive));
    CallThis(kSites[S_SETSTATE].rva, Addr(Zalloc(0x200)), 1, Addr(str));
    CHECK(C(C_STATE) == 0, "W15 another component state: not logged");
    CallThis(kSites[S_SETSTATE].rva, Addr(comp), 1, Addr(str));
    CHECK(C(C_STATE) == 1, "W16 the group button state: logged");
    CallThis(kSites[S_SETVIS].rva, Addr(comp), 1, 1);
    CHECK(C(C_VIS) == 1, "W17 the group button visibility: logged");
    FrameScenes(true, "p");
    // a UI group of one unit in slot 3: the sampler logs it
    uint8_t* ug = Zalloc(0x40);
    Put32(ug, 0x0C, 1);
    Put32(ug, 0x14, 3);
    uint32_t* ugs = (uint32_t*)Zalloc(8);
    ugs[0] = Addr(ug);
    Put32(g_mgr, 0x44, Addr(ugs));
    Put32(g_mgr, 0x40, 1);
    Sleep(300);
    CHECK(C(C_UIROWS) >= 1, "S1 sampler: UI group rows written");
    FreeLibrary(proxy);
    char dir[MAX_PATH];
    bool have = LatestRun(dir);
    CHECK(have, "F1 a run folder groupfix_runs\\<stamp>\\ was written");
    if (!have) return;
    char v[256], key[16];
    CHECK(Lines(dir, "group_events.csv") >= 1 + 2 + 16 + 1, "F2 group_events.csv: header + 2 groups made + 16 probe rows + sampler rows");
    CHECK(Field(dir, "group_events.csv", 2, "group_button_state_inactive", 7, v, sizeof(v)), "F2a group_button_state_inactive row");
    CHECK(Field(dir, "group_events.csv", 2, "key_G_button", 6, v, sizeof(v)) && strcmp(v, "1") == 0,
          "F2b key_G_button row: the click flag 1");
    {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%sgroup_events.csv", dir);
        FILE* f = fopen(path, "r");
        bool found = false;
        static char line[0x2000];
        while (f && fgets(line, sizeof(line), f)) found |= strstr(line, ",ui_groups,") && strstr(line, "n1 s3]");
        if (f) fclose(f);
        CHECK(found, "F2c ui_groups row lists the group of one in slot 3");
    }
    CHECK(Field(dir, "group_events.csv", 2, "group_made", 7, v, sizeof(v)) && strcmp(v, "1") == 0,
          "F3 first group_made row: one unit selected");
    CHECK(Field(dir, "group_events.csv", 2, "key_ctrl_number", 5, v, sizeof(v)) && strcmp(v, "3") == 0,
          "F4 key_ctrl_number row: slot 3");
    sprintf(key, "%X", Addr(unit));
    CHECK(Field(dir, "group_events.csv", 2, "engine_unit_add", 5, v, sizeof(v)) && strcmp(v, key) == 0,
          "F5 engine_unit_add row: the unit");
    CHECK(Field(dir, "group_events.csv", 2, "engine_group_finalise", 8, v, sizeof(v)) && strcmp(v, "1") == 0,
          "F6 engine_group_finalise row: the group's unit count 1");
}

static void RunFix() {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 0 && C(C_FIXON) == 1, "X1 installed, probe off, fix on at launch");
    SitesScene(false, true);
    GroupScenes(true, "a");
    FrameScenes(true, "a");
    PressI();
    CHECK(C(C_FIXON) == 0 && Imms(1), "K1 I: fix off, the game's immediates back");
    GroupScenes(false, "b");
    FrameScenes(false, "b");
    PressI();
    CHECK(C(C_FIXON) == 1 && Imms(0), "K2 I again: on");
    GroupScenes(true, "c");
    FrameScenes(true, "c");
}

static void RunOff() {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 0 && C(C_FIXON) == 0, "X1 installed, probe off, fix off (GROUPFIX_OFF=1)");
    SitesScene(false, false);
    GroupScenes(false, "a");
    FrameScenes(false, "a");
    PressI();
    CHECK(C(C_FIXON) == 1 && Imms(0), "K1 I: fix on");
    GroupScenes(true, "b");
    FrameScenes(true, "b");
}

static void RunCorrupt() {
    CHECK(C(C_INSTALLED) == 0, "X1 a changed byte in the fix site: not installed");
    bool none = true;
    for (int s = S_MADE; s < SITES; s++) none &= IsImmSite(s) || !Hooked(s);
    CHECK(none && Imm() == 2 && Imm2() == 1 && Imm3() == 1, "X2 no site patched, the (corrupted) immediate and the others untouched");
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc < 5) { printf("usage: groupfix_test <game dll> <proxy> <tag> fix|off|probe|corrupt\n"); return 2; }
    const char* mode = argv[4];
    bool probe = strcmp(mode, "probe") == 0;
    bool off = strcmp(mode, "off") == 0;
    bool corrupt = strcmp(mode, "corrupt") == 0;
    SetEnvironmentVariableA("GROUPFIX_PROBE", probe || corrupt ? "1" : nullptr);
    SetEnvironmentVariableA("GROUPFIX_OFF", off ? "1" : nullptr);
    SetEnvironmentVariableA("GROUPFIX_ON", nullptr);
    SetEnvironmentVariableA("COLLAPSEFIX_PROBE", nullptr);
    SetEnvironmentVariableA("ZOMBIEFIX_PROBE", nullptr);
    SetEnvironmentVariableA("WALLFIX_PROBE", nullptr);
    SetEnvironmentVariableA("SIEGEFIX_PROBE", nullptr);
    SetEnvironmentVariableA("FBRFIX_PROBE", nullptr);
    SetEnvironmentVariableA("ARTFIX_PROBE", nullptr);
    SetEnvironmentVariableA("PATHFIX_CAPTURE", nullptr);

    HMODULE game = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!game) { printf("cannot map game dll: %lu\n", GetLastError()); return 1; }
    g_base = (uint8_t*)game;
    for (int s = 0; s < SITES; s++) {
        uint8_t want[10];
        memcpy(want, kSites[s].orig, kSites[s].origLen);
        int ro = s == S_USE ? 5 : s == S_LOCK ? 1 : -1;     // absolute address inside the bytes: rebased
        if (ro >= 0) {
            uint32_t a;
            memcpy(&a, want + ro, 4);
            a += Addr(g_base) - 0x10000000u;
            memcpy(want + ro, &a, 4);
        }
        if (memcmp(g_base + kSites[s].rva, want, kSites[s].origLen) != 0) { printf("site %X differs\n", kSites[s].rva); return 1; }
        if (kSites[s].stubLen) Patch(g_base + kSites[s].rva + kSites[s].origLen, kSites[s].stub, kSites[s].stubLen);
    }
    if (corrupt) {
        uint8_t b = 0x02;
        Patch(g_base + kSites[S_FIX].rva + 3, &b, 1);
    }
    static uint8_t before[SITES][10];
    for (int s = 0; s < SITES; s++) memcpy(before[s], g_base + kSites[s].rva, kSites[s].origLen);
    MakeManager();
    MakeFrameStub();
    HMODULE proxy = LoadLibraryA(argv[2]);
    if (!proxy) { printf("cannot load proxy: %lu\n", GetLastError()); return 1; }
    g_counters = (TestCounters)GetProcAddress(proxy, "_GroupFixTestCounters@4");
    if (!g_counters) g_counters = (TestCounters)GetProcAddress(proxy, "GroupFixTestCounters");
    g_keys = (TestKeys)GetProcAddress(proxy, "_GroupFixTestKeys@4");
    if (!g_keys) g_keys = (TestKeys)GetProcAddress(proxy, "GroupFixTestKeys");
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
    CHECK(restored, "U1 after unload every site holds the bytes it had before the proxy loaded");
    printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
