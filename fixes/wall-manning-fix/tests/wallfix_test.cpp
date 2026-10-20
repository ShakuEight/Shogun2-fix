// wallfix_test.cpp -- offline harness for the wall manning fix (fixes/wall-manning-fix/src/wallfix.cpp).
//
// Maps the real empire.retail.dll (no game process), patches return stubs right after the stolen
// bytes of the hooked sites, loads the proxy and enters those sites on fake game objects: a unit
// with soldiers and a behaviour stack, move parameters, a form-up action with slots, an A*
// request. The battle clock is faked through the same pointer path the proxy reads.
//
//   wallfix_test <game dll> <proxy> <tag> fix|off|maxdist|probe|corrupt
//
//   fix      defaults: only the staging test site patched; called from the wall task's own call
//            (FUN_101b0b60, return 0x101B0CAD) with the game answering "far", the fix answers
//            "close enough" (staging march skipped); another caller keeps the game's answer; a
//            game answer "close" stays; I switches the fix off and on
//   off      WALLFIX_OFF=1: the game's answer until I
//   maxdist  WALLFIX_MAX_DIST=50: skipped for a unit 30 m from its wall positions, kept for one
//            300 m away
//   probe    WALLFIX_PROBE=1: every site patched; a unit move, a move controller, a form-up
//            creator, a form-up action and its go-to-slot step, an order, an A* search and a
//            staging decision are logged (CSV files checked); the A* wrapper returns the game
//            function's own result
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

struct Site { uint32_t rva; int origLen; uint8_t orig[8]; uint8_t stub[16]; int stubLen; };
enum { S_ORDER = 0, S_MOVE, S_MOVECTL, S_FA, S_FB, S_FC, S_FD, S_FACT, S_FGO, S_ASTAR, S_STAGE, SITES };
static const Site kSites[SITES] = {
    { 0x254E80, 5, { 0x56, 0x8B, 0x74, 0x24, 0x08 }, { 0x5E, 0xC2, 0x10, 0x00 }, 4 },                       // pop esi; ret 0x10
    { 0x3DB410, 6, { 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x04 }, { 0x5E, 0xC2, 0x04, 0x00 }, 4 },                 // pop esi; ret 4
    { 0x3E1250, 6, { 0x83, 0xEC, 0x18, 0x57, 0x8B, 0xF9 }, { 0x5F, 0x83, 0xC4, 0x18, 0xC3 }, 5 },          // pop edi; add esp,0x18; ret
    { 0x1A53F0, 6, { 0x83, 0xEC, 0x14, 0x57, 0x8B, 0xF9 }, { 0x5F, 0x83, 0xC4, 0x14, 0xC3 }, 5 },          // pop edi; add esp,0x14; ret
    { 0x1A5500, 5, { 0x83, 0xEC, 0x18, 0x53, 0x55 }, { 0x5D, 0x5B, 0x83, 0xC4, 0x18, 0xC3 }, 6 },          // pop ebp; pop ebx; add esp,0x18; ret
    { 0x1A5600, 5, { 0x83, 0xEC, 0x14, 0x53, 0x56 }, { 0x5E, 0x5B, 0x83, 0xC4, 0x14, 0xC3 }, 6 },          // pop esi; pop ebx; add esp,0x14; ret
    { 0x1A56E0, 5, { 0x83, 0xEC, 0x18, 0x53, 0x56 }, { 0x5E, 0x5B, 0x83, 0xC4, 0x18, 0xC3 }, 6 },
    { 0x3BE5D0, 6, { 0x53, 0x56, 0x8B, 0x74, 0x24, 0x0C }, { 0x5E, 0x5B, 0xC2, 0x08, 0x00 }, 5 },          // pop esi; pop ebx; ret 8
    { 0x3E1780, 5, { 0x83, 0xEC, 0x08, 0x56, 0x57 }, { 0x5F, 0x5E, 0x83, 0xC4, 0x08, 0xC3 }, 6 },          // pop edi; pop esi; add esp,8; ret
    // mov eax,0x0F01; pop ebx; add esp,0x20; ret 0xC (the search "found a path", AL = 1)
    { 0x386D90, 8, { 0x83, 0xEC, 0x20, 0x53, 0x8B, 0x5C, 0x24, 0x30 }, { 0xB8, 0x01, 0x0F, 0x00, 0x00, 0x5B, 0x83, 0xC4, 0x20, 0xC2, 0x0C, 0x00 }, 12 },
    // the staging test: mov eax,[g_gameAnswer] is not possible in a fixed stub, so the stub reads
    // the answer from the task (+0x200 dword): mov eax,[edi+0x200]; pop edi; add esp,0x18; ret 4
    { 0x16AF90, 6, { 0x83, 0xEC, 0x18, 0x57, 0x8B, 0xF9 }, { 0x8B, 0x87, 0x00, 0x02, 0x00, 0x00, 0x5F, 0x83, 0xC4, 0x18, 0xC2, 0x04, 0x00 }, 13 },
};
static const uint32_t RVA_STAGE_CALL = 0x1B0CA8;   // the game's CALL FUN_1016af90 in FUN_101b0b60
static const uint32_t RVA_STAGE_RET = 0x1B0CAD;    // its return address: a RET is placed there
static uint32_t g_stageCall = 0;

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

// --- calls into the game code (thiscall: ECX = object, stack arguments pushed right to left) ----
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
    return r;   // the callee removed its stack arguments (RET n)
}

// [esp] = return, [esp+4] = box; ECX = task. Jumps to the game's own CALL in FUN_101b0b60, so the
// staging test sees its real return address; the RET placed there returns here with the box still
// on the stack (the caller removes it).
__declspec(naked) static void StageThunk() {
    __asm {
        push dword ptr [esp + 4]
        jmp dword ptr [g_stageCall]
    }
}

// The staging test for `task` (game answer = task +0x200) with `box`; through the wall task's
// call site, or called directly (another caller).
static uint32_t Stage(uint8_t* task, float* box, bool viaWallTask) {
    uint32_t T = Addr(task), B = Addr(box), fn = Addr(g_base) + kSites[S_STAGE].rva, r;
    if (viaWallTask) {
        __asm {
            mov ecx, T
            push B
            call StageThunk
            add esp, 4
            mov r, eax
        }
    } else {
        __asm {
            mov ecx, T
            push B
            call fn
            mov r, eax
        }
    }
    return r & 0xFF;
}

// --- fake unit: soldiers with positions, a behaviour stack with one task in a named state ------
static uint8_t* g_unit = nullptr;
static uint8_t* g_task = nullptr;
static char g_stateName[] = "MOVE_TO_BUILDING";
static void MakeUnit(int men, float x, float z) {
    g_unit = Zalloc(0x800);
    uint32_t* list = (uint32_t*)Zalloc(4 * men);
    for (int i = 0; i < men; i++) {
        uint8_t* e = Zalloc(0x600);
        PutF(e, 0x524, x + (float)i);
        PutF(e, 0x52C, z);
        list[i] = Addr(e);
    }
    Put32(g_unit, 0x18, (uint32_t)men);
    Put32(g_unit, 0x1C, Addr(list));
    // a state object whose vtable slot +0x10 is "MOV EAX,[table]; RET", [table] = the name
    static uint8_t code[8];
    static uint32_t table;
    table = Addr(g_stateName);
    code[0] = 0xA1;
    uint32_t t = Addr(&table);
    memcpy(code + 1, &t, 4);
    code[5] = 0xC3;
    uint8_t* vt = Zalloc(0x20);
    Put32(vt, 0x10, Addr(code));
    uint8_t* state = Zalloc(0x10);
    Put32(state, 0, Addr(vt));
    g_task = Zalloc(0x200);
    uint8_t* tvt = Zalloc(0x40);
    Put32(g_task, 0, Addr(tvt));
    Put32(g_task, 0x04, Addr(g_unit));
    Put32(g_task, 0x08, Addr(g_unit));
    Put32(g_task, 0x18 + 4, Addr(state));
    Put32(g_unit, 0x400, Addr(g_task));
    Put32(g_unit, 0x440, 1);
    Put32(g_unit, 0x454, 0);
}

static long C(int i) { long c[16] = {}; g_counters(c); return c[i]; }
enum { C_PROBE = 0, C_FIXON, C_EVENTS, C_ASTAR, C_SLOTS, C_STACKS, C_INSTALLED, C_WATCH, C_SKIPPED, C_KEPT, C_NEAR,
       C_OTHER, C_MAXDIST, C_MEN };

// A wall task: its unit at +0x7C (4 men around (ux, uz)), the game's answer at +0x200.
static uint8_t* WallTask(float ux, float uz, uint32_t gameAnswer) {
    MakeUnit(4, ux, uz);
    uint8_t* t = Zalloc(0x300);
    Put32(t, 0x7C, Addr(g_unit));
    Put32(t, 0x200, gameAnswer);
    return t;
}
static float g_box[4] = { 0.0f, 0.0f, 10.0f, 10.0f };   // wall positions: centre (5, 5)

static void PressI() {
    g_keys(1);
    Sleep(200);
    g_keys(0);
    Sleep(120);
}

static void SitesScene(bool probe) {
    char name[96];
    for (int s = 0; s < SITES; s++) {
        bool want = probe || s == S_STAGE;
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
    char path[MAX_PATH], line[4096];
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
    HANDLE h = FindFirstFileA("wallfix_runs\\*", &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    char best[MAX_PATH] = "";
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.' && strcmp(fd.cFileName, best) > 0)
            strcpy(best, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (!best[0]) return false;
    sprintf(out, "wallfix_runs\\%s\\", best);
    return true;
}

static void RunProbe(HMODULE proxy) {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 1, "P0 installed, probe on");
    SitesScene(true);
    SetTick(100);
    MakeUnit(4, 10.0f, 20.0f);
    // a unit move to (55, 7, 528) with a 33 m width, heading given
    uint8_t* p = Zalloc(0x40);
    PutF(p, 4, 55.0f); PutF(p, 8, 7.0f); PutF(p, 12, 528.0f);
    p[0x18] = 1; p[0x1A] = 0x34; p[0x1B] = 0x12;
    PutF(p, 0x20, 33.0f);
    CallThis(kSites[S_MOVE].rva, Addr(g_unit), 1, Addr(p));
    // its move controller: action with destination (55, 7, 528), [[action+8]] = unit
    uint8_t* act = Zalloc(0x80);
    uint32_t* back = (uint32_t*)Zalloc(4);
    *back = Addr(g_unit);
    Put32(act, 8, Addr(back));
    PutF(act, 0x2C, 55.0f); PutF(act, 0x30, 7.0f); PutF(act, 0x34, 528.0f);
    CallThis(kSites[S_MOVECTL].rva, Addr(act), 0);
    // a form-up creator (task) and a form-up go-to step with 4 slots on a line x = 90
    CallThis(kSites[S_FA].rva, Addr(g_task), 0);
    uint8_t* fu = Zalloc(0x80);
    uint8_t* slots = Zalloc(0x24 * 4);
    for (int i = 0; i < 4; i++) {
        PutF(slots + 0x24 * i, 4, 90.0f);
        PutF(slots + 0x24 * i, 8, 9.75f);
        PutF(slots + 0x24 * i, 12, 500.0f + 2.0f * i);
        Put32(slots + 0x24 * i, 0x20, 3);
    }
    Put32(fu, 4, Addr(g_unit));
    Put32(fu, 8, Addr(g_unit));
    Put32(fu, 0x44, Addr(slots));
    CallThis(kSites[S_FGO].rva, Addr(fu), 0);
    uint8_t* params = Zalloc(0x20);
    CallThis(kSites[S_FACT].rva, Addr(fu), 2, Addr(g_unit), Addr(params));
    uint8_t* src = Zalloc(0x40);
    CallThis(kSites[S_ORDER].rva, Addr(Zalloc(0x60)), 4, Addr(src), 0x1234, 1, 0);
    // an A* search: request from (1, 2, 3) to (4, 5, 6), w 1.2, mode 2, layer 1
    uint8_t* q = Zalloc(0x48);
    PutF(q, 4, 1.0f); PutF(q, 8, 2.0f); PutF(q, 12, 3.0f);
    PutF(q, 0x18, 4.0f); PutF(q, 0x1C, 5.0f); PutF(q, 0x20, 6.0f);
    PutF(q, 0x2C, 1.2f);
    Put32(q, 0x30, 2);
    Put32(q, 0x38, 1);
    uint8_t* search = Zalloc(0x200);
    Put32(search, 0xC8, 77);
    Put32(search, 0xD4, 9);
    uint8_t* wt = WallTask(400.0f, 400.0f, 1);
    CHECK(Stage(wt, g_box, true) == 0, "P6 staging decision (fix on): skipped");
    uint32_t r = CallThis(kSites[S_ASTAR].rva, Addr(search), 3, Addr(q), 0, 0);
    CHECK((r & 0xFF) == 1, "P1 A* wrapper: the game function ran and its result (1) came back");
    CHECK(C(C_ASTAR) == 1, "P2 one A* search logged");
    Sleep(200);   // the stack sampler (every 50 ms)
    CHECK(C(C_EVENTS) == 7, "P3 seven events: move, movectl, formup_a, formup_go, formup_act, order, staging");
    CHECK(C(C_SLOTS) == 4, "P4 four slots logged");
    CHECK(C(C_WATCH) >= 1 && C(C_STACKS) >= 1, "P5 the unit's behaviour stack watched and sampled");
    SetTick(103);
    Sleep(200);
    CHECK(C(C_MEN) >= 4, "P7 the staged unit's men sampled");
    FreeLibrary(proxy);
    char dir[MAX_PATH];
    bool have = LatestRun(dir);
    CHECK(have, "F1 a run folder wallfix_runs\\<stamp>\\ was written");
    if (!have) return;
    char v[256];
    CHECK(Lines(dir, "wall_events.csv") == 8, "F2 wall_events.csv: header + 7 events");
    CHECK(Field(dir, "wall_events.csv", 4, "staging", 13, v, sizeof(v)) && strcmp(v, "1") == 0, "F15 staging row: the game said far");
    CHECK(Field(dir, "wall_events.csv", 4, "staging", 14, v, sizeof(v)) && strcmp(v, "0") == 0, "F16 ... the fix answered close");
    CHECK(Field(dir, "wall_events.csv", 4, "staging", 5, v, sizeof(v)) && strcmp(v, "1B0CAD") == 0, "F17 ... from the wall task's call");
    CHECK(Lines(dir, "wall_slots.csv") == 5, "F3 wall_slots.csv: header + 4 slots");
    CHECK(Lines(dir, "wall_astar.csv") == 2, "F4 wall_astar.csv: header + 1 search");
    CHECK(Lines(dir, "wall_men.csv") >= 5, "F18 wall_men.csv: header + the 4 men at least once");
    CHECK(Field(dir, "wall_events.csv", 4, "move", 10, v, sizeof(v)) && strcmp(v, "55.00") == 0, "F5 move row: destination x 55");
    CHECK(Field(dir, "wall_events.csv", 4, "move", 12, v, sizeof(v)) && strcmp(v, "528.00") == 0, "F6 ... z 528");
    CHECK(Field(dir, "wall_events.csv", 4, "move", 14, v, sizeof(v)) && strcmp(v, "123401") == 0, "F7 ... heading 0x1234 given");
    CHECK(Field(dir, "wall_events.csv", 4, "move", 17, v, sizeof(v)) && strstr(v, "MOVE_TO_BUILDING") != nullptr, "F8 ... the stack names the task's state");
    CHECK(Field(dir, "wall_events.csv", 4, "formup_go", 13, v, sizeof(v)) && strcmp(v, "4") == 0, "F9 formup_go row: 4 men");
    CHECK(Field(dir, "wall_events.csv", 4, "formup_go", 14, v, sizeof(v)) && strcmp(v, "8") == 0, "F10 ... slot type 3 (bit 3)");
    CHECK(Field(dir, "wall_slots.csv", 4, "3", 8, v, sizeof(v)) && strcmp(v, "506.00") == 0, "F11 slot 3 at z 506");
    CHECK(Field(dir, "wall_astar.csv", 14, "2", 17, v, sizeof(v)) && strcmp(v, "77") == 0, "F12 A* row: mode 2, 77 nodes");
    CHECK(Field(dir, "wall_astar.csv", 14, "2", 18, v, sizeof(v)) && strcmp(v, "9") == 0, "F13 ... 9 portals");
    CHECK(Field(dir, "wall_astar.csv", 14, "2", 16, v, sizeof(v)) && strcmp(v, "1") == 0, "F14 ... result 1");
}

static void RunFix() {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 0 && C(C_FIXON) == 1, "X1 installed, probe off, fix on at launch");
    CHECK(C(C_MAXDIST) == 0, "X2 no distance limit by default");
    SitesScene(false);
    uint8_t* far1 = WallTask(400.0f, 400.0f, 1);
    CHECK(Stage(far1, g_box, true) == 0 && C(C_SKIPPED) == 1, "S1 wall task, game 'far': fix answers 'close', staging skipped");
    CHECK(Stage(far1, g_box, false) == 1 && C(C_OTHER) == 1 && C(C_SKIPPED) == 1, "S2 another caller: the game's 'far' stays");
    uint8_t* near1 = WallTask(5.0f, 5.0f, 0);
    CHECK(Stage(near1, g_box, true) == 0 && C(C_NEAR) == 1 && C(C_SKIPPED) == 1, "S3 game 'close' stays 'close'");
    PressI();
    CHECK(C(C_FIXON) == 0 && Stage(far1, g_box, true) == 1 && C(C_SKIPPED) == 1, "K1 I: fix off, the game's 'far' (staging march)");
    PressI();
    CHECK(C(C_FIXON) == 1 && Stage(far1, g_box, true) == 0 && C(C_SKIPPED) == 2, "K2 I again: skipped again");
}

static void RunOff() {
    CHECK(C(C_INSTALLED) == 1 && C(C_PROBE) == 0 && C(C_FIXON) == 0, "X1 installed, probe off, fix off (WALLFIX_OFF=1)");
    SitesScene(false);
    uint8_t* far1 = WallTask(400.0f, 400.0f, 1);
    CHECK(Stage(far1, g_box, true) == 1 && C(C_SKIPPED) == 0, "S1 fix off: the game's 'far' stays");
    PressI();
    CHECK(C(C_FIXON) == 1 && Stage(far1, g_box, true) == 0 && C(C_SKIPPED) == 1, "K1 I: fix on, staging skipped");
    PressI();
    CHECK(C(C_FIXON) == 0, "K2 I again: off");
}

static void RunMaxDist() {
    CHECK(C(C_FIXON) == 1 && C(C_MAXDIST) == 50, "X1 fix on, WALLFIX_MAX_DIST=50");
    uint8_t* mid = WallTask(5.0f + 30.0f, 5.0f, 1);
    CHECK(Stage(mid, g_box, true) == 0 && C(C_SKIPPED) == 1, "D1 unit 30 m from its wall positions: staging skipped");
    uint8_t* far1 = WallTask(5.0f + 300.0f, 5.0f, 1);
    CHECK(Stage(far1, g_box, true) == 1 && C(C_KEPT) == 1, "D2 unit 300 m away: staging march kept");
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
    if (argc < 5) { printf("usage: wallfix_test <game dll> <proxy> <tag> fix|off|maxdist|probe|corrupt\n"); return 2; }
    const char* mode = argv[4];
    bool probe = strcmp(mode, "probe") == 0;
    bool off = strcmp(mode, "off") == 0;
    bool corrupt = strcmp(mode, "corrupt") == 0;
    bool maxdist = strcmp(mode, "maxdist") == 0;
    SetEnvironmentVariableA("WALLFIX_PROBE", probe ? "1" : nullptr);
    SetEnvironmentVariableA("WALLFIX_OFF", off ? "1" : nullptr);
    SetEnvironmentVariableA("WALLFIX_MAX_DIST", maxdist ? "50" : nullptr);
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
        Patch(g_base + kSites[s].rva + kSites[s].origLen, kSites[s].stub, kSites[s].stubLen);
    }
    static const uint8_t callStage[5] = { 0xE8, 0xE3, 0xA2, 0xFB, 0xFF };   // CALL FUN_1016af90
    if (memcmp(g_base + RVA_STAGE_CALL, callStage, 5) != 0) { printf("staging call differs\n"); return 1; }
    static const uint8_t ret = 0xC3;
    Patch(g_base + RVA_STAGE_RET, &ret, 1);
    g_stageCall = Addr(g_base + RVA_STAGE_CALL);
    if (corrupt) {
        uint8_t b = 0x90;
        Patch(g_base + kSites[S_MOVE].rva + 1, &b, 1);
    }
    static uint8_t before[SITES][8];
    for (int s = 0; s < SITES; s++) memcpy(before[s], g_base + kSites[s].rva, kSites[s].origLen);
    MakeClock();
    HMODULE proxy = LoadLibraryA(argv[2]);
    if (!proxy) { printf("cannot load proxy: %lu\n", GetLastError()); return 1; }
    g_counters = (TestCounters)GetProcAddress(proxy, "_WallFixTestCounters@4");
    if (!g_counters) g_counters = (TestCounters)GetProcAddress(proxy, "WallFixTestCounters");
    g_keys = (TestKeys)GetProcAddress(proxy, "_WallFixTestKeys@4");
    if (!g_keys) g_keys = (TestKeys)GetProcAddress(proxy, "WallFixTestKeys");
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
        else if (maxdist) RunMaxDist();
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
