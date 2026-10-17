// Offline harness for v41 (2026-10-16): variant G only, I toggles off <-> G, no recording in play.
// Runs full / geo / crash / pkey set PATHFIX_CAPTURE=1 before the proxy loads, so v40's capture
// checks still apply. The real I key now toggles off <-> G (G section entry, "off again", F1, the
// crash run); the C and RC parts G is built from are selected with the test-only export
// PathfixTestVariant (former first and second I presses). New runs:
//   play  no PATHFIX_CAPTURE: no run folder, no CSV, log in the exe folder; I -> G acts (lead the
//         turn LD2-style point, release G1-style advance), P and O are not read, I -> off vanilla
//   pkey  capture on: P finishes the capture, then I still switches G on and G still acts
// --- v40 header ---
// Offline harness for v40 (Phase 24c). As v39 except G6: a narrow run that reaches the path
// end now puts the anchor 2 m BEFORE the end (last node t 0.75, the end never "reached"), and
// the row reason is end_gap.
// --- v39 header ---
// Offline harness for v39 (Phase 24b). As v38 plus the anchor hold: G9 a later laying call
// whose vanilla point (leader + 11 m) lies behind the anchor applied before on the same path
// is held at that anchor (the real advance lands on the same node and fraction as G1); G10 a
// vanilla point past the anchor ends the hold; T14 counts the hold; T15 sees the "hold" row.
// --- v38 header ---
// Offline harness for v38 (Phase 24). As v37 plus section G (variant G = RC + release
// beyond the choke, the third I press): a fake unit path of 0xA0-byte nodes (next +0x08,
// widths +0x74 / +0x78, follow points +0x80 / +0x88) is walked by the orders hook, and the
// REAL FUN_103670a0 is then entered through the real call site in FUN_101bc880 (the bytes
// at 0x1BC975 become PUSH 1; PUSH EDX; NOPs; CALL FUN_103670a0; RET, so the hook sees the
// return address 0x1BC988 and the real advance walks the fake nodes). G1 the distance is
// rewritten and the cursor lands past the narrow run + block depth + margin; G2 a point on
// wide ground is left alone; G3 no plan (consumed) -> vanilla; G4 a different cursor than
// planned -> vanilla; G5 another call site -> vanilla; G6 a run at the path end -> past
// the end (t > 1, the game's own release); G7 a path without a wide node -> vanilla; G8
// fix off -> vanilla; T14 counters and release.csv; U3 the entry restored after unload.
// --- v37 header ---
// Offline harness for v37 (Phase 23). As v36 plus SP1 / SP2: the soldier sample rows
// carry the speed-law fields (spd_max .. f308, read from the fake soldiers' blocks at
// +0xF0 / +0x430 / +0x308 / +0x384) and the order rows carry leader_idx / ready /
// speed_mode from the fake controller (+0x30 / +0x0C / +0x24).
// --- v36 header ---
// Offline harness for v36 (Phase 22). As v35 with: (1) the game-time clock: the fake
// controllers carry the engine's tick counter chain [[ctrl+0x14]+8]+0x50, every fix
// window wait is Advance(ms) on that counter (wall ms * 3.6 / 100 ticks) instead of
// Sleep, GT1/GT2 read the clock through PathfixTestGameNow, GT3/GT4 show that wall
// time alone no longer ages the no-damping apex and the stuck clock, GT5/GT6 the
// counters and the log line; (2) variant RC (mode 9) on the second I press: the RQ
// section runs under RC, RC1 checks that lead the turn adjusts the waypoint there.
// --- v35 header ---
// Offline harness for v35 (Phase 21b). As v34 plus the unit key: RQ13 a man without
// an apex call of his own gets a lane from the unit's busy corner ahead of him,
// RQ14 the lane is kept 2 s later without any call of his own, RQ15 a corner he
// has passed gives no lane, RQ16 the own key still works; T12 counters.
// --- v34 header ---
// Offline harness for v34 (Phase 21: I cycles off -> C -> R -> off). Section RQ
// enters the radius patch at 0x103df4d1 with FUN_103df3f0's frame (request Q at
// ESP+0x58, w at +0x2C, end at +0x18, EBX = soldier, [ESP+0xF] for the replayed
// CMP) and checks w: no corner / small queue / fix in C -> unchanged; queue of
// 10 -> lane by rank (w0, w0 + 1.5, cap 4.2); an overfull lane -> the adjacent
// lane only; bad frame -> unchanged; stuck -> w0; the stolen CMP replayed.
// The avoidance section AV of v33 is gone (L is not on the I cycle).
// --- v33 header ---
// Offline harness for v33 (Phase 20: I cycles off -> C -> L -> off). Section AV
// checks the look-ahead avoidance of variant L against the harness's own copy
// of the rule (ExpAvoidDeg): stalled man ahead, one side blocked, same-way
// mover ignored, oncoming mover passed clockwise, enclosed man left alone,
// obstacle beyond the slot ignored, side hold, wall memory, a man not
// following a path untouched, another unit's man avoided, two threads, and
// no avoidance with the fix off. The v31 tip-lane section TP is gone.
// --- v31 header ---
// Offline harness for v31 (Phase 19: I cycles off -> C -> F -> off; section TP checks
// F's tip lanes against the harness's own copy of the rule (ExpectTip), plus
// properties: waypoint on the man's circle, outer rank -> larger radius, the
// narrow end cap of the same tip handled, past the sector -> game point, no
// queue -> C rules).
// --- v30 header ---
// Offline harness for v30 (Phase 18b: I cycles off -> A -> C -> D -> off; section FC
// checks the variant C press between A (modes 1 / 2) and D).
// --- v28 header ---
// Offline harness for v28 (Phase 17c: gather only for men with no apex call since
// the order; GD5 = a man who rounded ANOTHER corner since the order).
// --- v27 header ---
// Offline harness for v27 (Phase 17b: I cycles off -> C -> D -> off). C = lead the
// turn + no crowd damping within 30 m; D = C + gather at the queue on goal
// calls (section GD; the unit's move order is noted through the test export
// PathfixTestOrder).
// --- v25 header ---
// Offline harness for v25 (Phase 16b: v23 + no crowd damping near corners). Section
// ST now checks that the integrator hook clears the damping byte e+0xD6 for
// corner men (and only for them) and never changes the displacement D.
// --- v24 header ---
// Offline harness for v24 (Phase 16: v23 + lookahead steering on the integrator
// FUN_10367600). Section ST calls the integrator entry with ECX = soldier (the
// body is stubbed after the stolen prologue: MOV ESP,EBP; POP EBP; RET) and
// checks the steered displacement e+0xD8 / +0xE0 against the harness's own
// implementation (ExpectSteer): blocked ahead -> turned, free -> untouched,
// outside the zone, unlisted soldier, default radius, stale apex, fix off.
// --- v23 header ---
// Offline harness for v23 (Phase 15: mode 1 = lead the turn). Section LD checks
// the lead points against the harness's own implementation (ExpectLead):
// outside the zone, free point, occupied point (next radius), stored turning
// sign with the sector clamp, beyond the far edge, radius cut by a short lane.
// --- v22 header ---
// Offline harness for v22 (Phase 14: mode 1 = lateral rank, 4 m lane). Section LR
// checks the lateral-rank waypoints against the harness's own implementation
// (stored apexes let expire first, then every man fed the same corner); the
// gate / edge / other-end cases run with a lone group (game point back); the
// turn limit and stuck release are tested in mode 2, where v22 applies them.
// --- v21 header ---
// Offline harness for v21 (Phase 13: v20 + turn limit + stuck release). The v20 checks
// run with the soldiers 60 m from the corner (z = -60; the turn limit cannot
// bind there) and every legacy call nudges the soldier 0.6 m (no stuck
// release); section V21 tests both guards.
// --- v20 header ---
// Offline harness for v20 (Phase 12d: both candidates from the REAL game helpers FUN_10393000 /
// FUN_103a0080 in the mapped image, every side-object type): map the real empire.retail.dll
// WITHOUT running it, load the v20 proxy, install the hooks, and drive them
// with fabricated data. Build: build.bat next to this file
// (cl /O1 /W3 ... /link /SAFESEH:NO /LARGEADDRESSAWARE user32.lib).
//
// In THIS PROCESS'S PRIVATE MAPPING ONLY, these bodies are replaced so the
// real code never walks the fake data:
//   FUN_1039bb40 + 6: ADD ESP,0x18C; RET 0xC
//   FUN_101a9750 + 6: MOV ESP,EBP; POP EBP; RET 4
//   FUN_101bc880 + 6: ADD ESP,0xC8; RET 4
//   FUN_103dbbf0 + 5: POP EDI; POP ESI; POP EBX; RET 4
//   FUN_103dafb0 + 6: POP ESI; RET 4
//   0x103df6d9 (the JNZ after the waypoint copy): SETNZ AL; MOVZX EAX,AL;
//       JMP [g_backTarget]  -> back into this harness
// The fix patch point 0x103df6c0 itself is the proxy's: the harness enters it
// with a frame laid out like FUN_103df3f0's after CALL FUN_10394e80 returns
// (EAX = buffer = ESP+0x44, [ESP+0x14] = apex portal, EBX = soldier), and the
// real game bytes 0x103df6c7..0x103df6d8 then copy the buffer to
// soldier+0x454, so the test reads the result where the game would.
// Keys are pressed through PathfixTestKeys, never as real keystrokes.
//
// Usage: hooktest20.exe <empire.retail.dll> <proxy dll> <tag> full|crash
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>

typedef HRESULT (WINAPI *DI8Create)(HINSTANCE, DWORD, const void*, void**, void*);
typedef void (__stdcall *TestKeys)(LONG);
typedef void (__stdcall *TestOrder)(uint32_t);
typedef void (__fastcall *FnBuild)(void* ecx, void* edx, uint32_t p, uint32_t head, uint32_t a3);
typedef void (__fastcall *FnOne)(void* ecx, void* edx, uint32_t a1);
typedef void (__fastcall *FnXform)(void* ecx, void* edx, void* out, const void* in);

static uint8_t* g_base = nullptr;
static TestKeys g_keys = nullptr;
static TestOrder g_order = nullptr;
typedef void (__stdcall* TestVariant)(LONG);
static TestVariant g_variant = nullptr;          // v41: harness-only variant select (0 off, 1 C, 2 RC, 3 G)
static void Variant(LONG v) { g_variant(v); Sleep(100); }
typedef DWORD (__stdcall* TestGameNow)();
static TestGameNow g_gnow = nullptr;
static uint8_t* g_clk = nullptr;               // v36: fake clock object, tick at +0x50
static const uint32_t GT_TICK0 = 36000;        // 3600 game s at the start
static double g_gtTicks = 0.0;
// v36: game time instead of Sleep for every fix window: wall ms * 3.6 game ms (the DLL's
// tested window lengths), 100 ms per tick.
static void Advance(DWORD wallMs) {
    g_gtTicks += wallMs * 3.6 / 100.0;
    uint32_t t = GT_TICK0 + (uint32_t)g_gtTicks;
    memcpy(g_clk + 0x50, &t, 4);
}
static int g_fail = 0, g_pass = 0;

#define CHECK(cond, ...) do { if (cond) { g_pass++; printf("  PASS "); } else { g_fail++; printf("  FAIL "); } \
                              printf(__VA_ARGS__); printf("\n"); } while (0)

static int CallBuild(void* ecx, uint32_t p, uint32_t head) {
    __try { ((FnBuild)(g_base + 0x39BB40))(ecx, 0, p, head, 0xA3A3A3A3); return 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
}
static int CallOne(uint32_t rva, void* ecx, uint32_t a1) {
    __try { ((FnOne)(g_base + rva))(ecx, 0, a1); return 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
}
static int CallCursor(void* ctrl) { return CallOne(0x1A9750, ctrl, 0xC0C0C0C0); }
static int CallOrders(void* ctrl) { return CallOne(0x1BC880, ctrl, 0); }

// v38: FUN_103670a0 is __thiscall(cursor* ecx, float distance, char flag). The call site
// 0x1BC975..0x1BC982 in FUN_101bc880 is patched to PUSH 1; PUSH EDX; NOPs so that the real
// CALL at 0x1BC983 runs with the harness's cursor in ECX and the distance bits in EDX, and
// a RET at 0x1BC988 brings control back. The hook sees the real return address.
typedef void (__fastcall *FnThunk)(void* ecx, uint32_t distBits);
static int CallAdvance(float* cur, float d) {
    uint32_t bits;
    memcpy(&bits, &d, 4);
    __try { ((FnThunk)(g_base + 0x1BC975))(cur, bits); return 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
}
typedef void (__fastcall *FnAdvDirect)(void* ecx, void* edx, float d, char flag);
static int CallAdvanceDirect(float* cur, float d) {      // another return address: the hook must stay out
    __try { ((FnAdvDirect)(g_base + 0x3670A0))(cur, 0, d, 1); return 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
}
static uint8_t* MakeNodes(uint8_t** N, int n, const float* len, const float* wid) {   // chained list, follow points on the x axis
    float x = 0.0f;
    for (int i = 0; i < n; i++) {
        N[i] = (uint8_t*)VirtualAlloc(nullptr, 0x100, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
        memcpy(N[i] + 0x74, &wid[i], 4); memcpy(N[i] + 0x78, &wid[i], 4);
        float f[4] = { x, 0.0f, x + len[i], 0.0f };
        memcpy(N[i] + 0x80, f, 16);
        x += len[i];
    }
    for (int i = 0; i < n; i++) { uint32_t nx = i + 1 < n ? (uint32_t)(uintptr_t)N[i + 1] : 0; memcpy(N[i] + 0x08, &nx, 4); }
    return N[0];
}

// ---- entering the patch point with FUN_103df3f0's frame ------------------
extern "C" {
uint32_t g_saveEsp = 0;
uint32_t g_patchSite = 0;
uint32_t g_backTarget = 0;
uint32_t g_argSoldier = 0;
uint32_t g_argPortal = 0;
int32_t  g_eaxDelta = 0;          // != 0: EAX deliberately not ESP+0x44 (frame self-check)
uint32_t g_init[5] = {};          // buffer as FUN_10394e80 would have filled it
}

extern "C" __declspec(naked) void BackFromPatch() {
    __asm {
        mov esp, g_saveEsp
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}

// Returns the stub's SETNZ result: 1 when byte [soldier+0x471] != 1.
extern "C" __declspec(naked) uint32_t EnterPatch() {
    __asm {
        push ebx
        push esi
        push edi
        push ebp
        mov g_saveEsp, esp
        sub esp, 0x200
        and esp, 0xFFFFFFF0
        mov eax, g_argPortal
        mov [esp+0x14], eax
        lea edi, [esp+0x44]
        lea esi, g_init
        mov ecx, 5
        rep movsd
        lea eax, [esp+0x44]
        add eax, g_eaxDelta
        mov ebx, g_argSoldier
        jmp dword ptr [g_patchSite]
    }
}

// ---- v34: entering the radius patch at 0x103df4d1 with FUN_103df3f0's frame
extern "C" {
uint32_t g_saveEsp2 = 0;
uint32_t g_radiusSite = 0;
uint32_t g_backRadius = 0;
uint32_t g_argSoldier2 = 0;
uint32_t g_req[20] = {};          // request Q (0x50 bytes) copied to hook-point ESP+0x58
uint32_t g_cmpByte = 1;           // [ESP+0xF] for the stolen CMP byte [ESP+0xF],1
uint32_t g_cmpZ = 0;              // ZF after the replayed CMP
uint32_t g_wOut = 0;              // [ESP+0x84] = Q+0x2C after the patch
}

extern "C" __declspec(naked) void BackFromRadius() {
    __asm {
        setz al
        movzx eax, al
        mov g_cmpZ, eax
        mov ecx, [esp+0x84]
        mov g_wOut, ecx
        mov esp, g_saveEsp2
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}

extern "C" __declspec(naked) void EnterRadius() {
    __asm {
        push ebx
        push esi
        push edi
        push ebp
        mov g_saveEsp2, esp
        sub esp, 0x200
        and esp, 0xFFFFFFF0
        lea edi, [esp+0x58]
        lea esi, g_req
        mov ecx, 20
        rep movsd
        mov eax, g_cmpByte
        mov byte ptr [esp+0x0F], al
        mov ebx, g_argSoldier2
        jmp dword ptr [g_radiusSite]
    }
}

// Returns w after the patch; frame end = (ex, ez).
static float RunRadius(uint8_t* soldier, float w0, float ex, float ez) {
    memset(g_req, 0, sizeof(g_req));
    float one = 1.0f;
    memcpy((uint8_t*)g_req + 0x18, &ex, 4);
    memcpy((uint8_t*)g_req + 0x1C, &one, 4);
    memcpy((uint8_t*)g_req + 0x20, &ez, 4);
    memcpy((uint8_t*)g_req + 0x2C, &w0, 4);
    g_argSoldier2 = (uint32_t)(uintptr_t)soldier;
    g_wOut = 0;
    __try { EnterRadius(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1.0f; }
    float w;
    memcpy(&w, &g_wOut, 4);
    return w;
}

static int RunPatch(uint8_t* soldier, uint8_t* portal, float x, float y, float z, uint32_t* jnzTaken) {
    g_argSoldier = (uint32_t)(uintptr_t)soldier;
    g_argPortal = (uint32_t)(uintptr_t)portal;
    memcpy(&g_init[0], &x, 4);
    memcpy(&g_init[1], &y, 4);
    memcpy(&g_init[2], &z, 4);
    g_init[3] = 0xBEEF1234;       // u16 heading + pad: must pass through unchanged
    g_init[4] = 0x3F400000;       // float 0.75: must pass through unchanged
    __try { *jnzTaken = EnterPatch(); return 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
}

static void Patch(uint8_t* at, const uint8_t* bytes, size_t n) {
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
}

static void Press(int k) {   // k = key function: 0 fix (I), 1 lane, 2 marker (O), 3 more, 4 finish (P)
    g_keys(1 << k);
    Sleep(250);
    g_keys(0);
    Sleep(300);
}

static void PutF(uint8_t* p, int off, float v) { memcpy(p + off, &v, 4); }
static void PutU(uint8_t* p, int off, uint32_t v) { memcpy(p + off, &v, 4); }
static float GetF(const uint8_t* p, int off) { float f; memcpy(&f, p + off, 4); return f; }
static uint32_t GetU(const uint8_t* p, int off) { uint32_t u; memcpy(&u, p + off, 4); return u; }
static bool Near(float a, float b) { return fabsf(a - b) <= 1e-4f; }

// MEM_TOP_DOWN: linked /LARGEADDRESSAWARE, the fakes then sit above 2 GB as
// in the game (Shogun2.exe is large-address-aware).
static uint8_t* Alloc(size_t n) {
    return (uint8_t*)VirtualAlloc(nullptr, n, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
}

static void FileSize(const char* dir, const char* name) {
    char path[MAX_PATH];
    sprintf(path, "%s%s", dir, name);
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &a)) printf("  %s %lu bytes\n", name, a.nFileSizeLow);
    else printf("  %s absent\n", name);
}

// v18 side object: +0x08 type, +0x10 link, +0x20 vertex record E; E: +0x08
// next record T, +0x0C / +0x10 position A, +0x24 / +0x28 offset vector mA,
// +0x30 clamp limit (0.0 as live); T: position B, offset vector mB.
// v20: types 1..3 take their vectors from {x, z, owner} at +0x38 (left) and
// +0x44 (right) (FUN_1037f370; owner 0 = used as stored); types >= 4 use the
// edge direction E+0x14 / +0x18 (left A + d*r, right B - d*r).
struct Portal { uint8_t* P; uint8_t* E; uint8_t* T; };
static Portal MakePortal(float ax, float az, float max_, float maz, float bx, float bz, float mbx, float mbz,
                         uint32_t type, uint32_t link) {
    Portal p;
    p.P = Alloc(0x100); p.E = Alloc(0x100); p.T = Alloc(0x100);
    PutF(p.P, 0x38, max_); PutF(p.P, 0x3C, maz); PutU(p.P, 0x40, 0);
    PutF(p.P, 0x44, mbx); PutF(p.P, 0x48, mbz); PutU(p.P, 0x4C, 0);
    float dx = bx - ax, dz = bz - az, l = sqrtf(dx * dx + dz * dz);
    PutF(p.E, 0x14, dx / l); PutF(p.E, 0x18, dz / l);
    PutU(p.P, 0x08, type);
    PutU(p.P, 0x10, link);
    PutU(p.P, 0x20, (uint32_t)(uintptr_t)p.E);
    PutU(p.E, 0x08, (uint32_t)(uintptr_t)p.T);
    PutF(p.E, 0x0C, ax); PutF(p.E, 0x10, az);
    PutF(p.E, 0x24, max_); PutF(p.E, 0x28, maz);
    PutF(p.E, 0x30, 0.0f);
    PutF(p.T, 0x0C, bx); PutF(p.T, 0x10, bz);
    PutF(p.T, 0x24, mbx); PutF(p.T, 0x28, mbz);
    return p;
}

// Fix soldier: position, slot, path result R with R+0x50 = 2 * r.
static void SetFixSoldier(uint8_t* s, uint8_t* R, float px, float pz, float dx, float dz) {
    PutF(s, 0x48, px); PutF(s, 0x4C, 1.0f); PutF(s, 0x50, pz);
    PutF(s, 0x420, dx); PutF(s, 0x424, 1.0f); PutF(s, 0x428, dz);
    PutU(s, 0x3CC, (uint32_t)(uintptr_t)R);
    PutU(s, 0x2F0, (uint32_t)(uintptr_t)s);
    s[0x471] = 0;
}

// Runs one apex call and checks the copied waypoint.
static bool g_wiggle = true;   // legacy calls: alternate z -20 / -20.6 (moves 0.6 m per call)
static void Apex(const char* tag, uint8_t* s, Portal& p, float wx, float wz, float ex, float ez) {
    uint32_t jnz = 0xFFFFFFFF;
    if (g_wiggle) PutF(s, 0x50, GetF(s, 0x50) == -20.0f ? -20.6f : -20.0f);
    memset(s + 0x454, 0xCC, 0x14);
    int fault = RunPatch(s, p.P, wx, 2.5f, wz, &jnz);
    float ox = GetF(s, 0x454), oy = GetF(s, 0x458), oz = GetF(s, 0x45C);
    bool rest = GetU(s, 0x460) == 0xBEEF1234 && GetU(s, 0x464) == 0x3F400000 && Near(oy, 2.5f);
    CHECK(!fault && rest && Near(ox, ex) && Near(oz, ez) && jnz == 1,
          "%s: in (%.4f, %.4f) -> out (%.4f, %.4f), expect (%.4f, %.4f); y/heading/float kept %d; JNZ %u",
          tag, wx, wz, ox, oz, ex, ez, rest ? 1 : 0, jnz);
    Sleep(120);   // rows are limited to 10 Hz per soldier
}

// Harness's own lateral rank: members = the listed soldiers (all fed apex P at
// corner V within the last 1.5 s) standing within 30 m of V, plus self.
static void ExpectLateral(uint8_t** S, const int* mem, int nm, int self, float Vx, float Vz, float Px, float Pz,
                          float L, float* ex, float* ez) {
    double dx = 0, dz = 0;
    int idx[16];
    int m = 0;
    idx[m++] = self;
    for (int i = 0; i < nm; i++) {
        if (mem[i] == self) continue;
        double x = GetF(S[mem[i]], 0x48), z = GetF(S[mem[i]], 0x50);
        if (sqrt((x - Vx) * (x - Vx) + (z - Vz) * (z - Vz)) > 30.0) continue;
        idx[m++] = mem[i];
    }
    for (int j = 0; j < m; j++) {
        double x = GetF(S[idx[j]], 0x48), z = GetF(S[idx[j]], 0x50);
        double ux = Px - x, uz = Pz - z, l = sqrt(ux * ux + uz * uz);
        dx += ux / l; dz += uz / l;
    }
    double dl = sqrt(dx * dx + dz * dz);
    dx /= dl; dz /= dl;
    double ux = -dz, uz = dx;
    if ((Px - Vx) * ux + (Pz - Vz) * uz < 0) { ux = -ux; uz = -uz; }
    double lam0 = (GetF(S[self], 0x48) - Vx) * ux + (GetF(S[self], 0x50) - Vz) * uz;
    int below = 0;
    for (int j = 1; j < m; j++) {
        double lj = (GetF(S[idx[j]], 0x48) - Vx) * ux + (GetF(S[idx[j]], 0x50) - Vz) * uz;
        if (lj < lam0) below++;
    }
    double q = m > 1 ? (double)below / (m - 1) : 0.0;
    double Lc = L < 4.0 ? L : 4.0;
    *ex = (float)(Px + ux * q * Lc);
    *ez = (float)(Pz + uz * q * Lc);
}

// Harness's own v23 rule. others: the unit's other men (positions read from
// their soldier blocks). sgnIn: 0 = first call (sign from the geometry), else
// the stored sign. Returns 0 adjusted, 1 outside zone, 2 past sector.
static int ExpectLead(uint8_t** S, int ns, int self, double Vx, double Vz, double Px, double Pz,
                      double mx, double mz, double L, double sgnIn, float* ex, float* ez, double* sgnOut) {
    const double PI = 3.14159265358979;
    double X = GetF(S[self], 0x48), Z = GetF(S[self], 0x50);
    double rx = X - Vx, rz = Z - Vz, dist = sqrt(rx * rx + rz * rz);
    *ex = (float)Px; *ez = (float)Pz;
    if (dist > 6.0) return 1;
    double bx = Px - Vx, bz = Pz - Vz, rP = sqrt(bx * bx + bz * bz);
    double beta = atan2(bz, bx);
    double relth = atan2(rz, rx) - beta;
    while (relth > PI) relth -= 2 * PI;
    while (relth < -PI) relth += 2 * PI;
    double sgn = sgnIn;
    if (sgn == 0) {
        double ax = Px - X, az = Pz - Z;
        if (sqrt(ax * ax + az * az) > 0.3) sgn = (ax * (Vz - Z) - az * (Vx - X)) > 0 ? 1.0 : -1.0;
        else sgn = relth > 0 ? -1.0 : 1.0;
    }
    *sgnOut = sgn;
    double ml = sqrt(mx * mx + mz * mz);
    double half = (90.0 + (ml > 1 ? acos(1.0 / ml) * 180.0 / PI : 0.0) - 15.0) * PI / 180.0;
    if (sgn * relth > half) return 2;
    double tgt = relth + sgn * 120.0 * PI / 180.0;
    if (tgt > half) tgt = half;
    if (tgt < -half) tgt = -half;
    double th = beta + tgt;
    double rspan = L < 4.0 ? L : 4.0;
    double rho = dist < rP ? rP : (dist > rP + rspan ? rP + rspan : dist);
    const double steps[5] = { 0, 1, -1, 2, -2 };
    double useR = -1, firstR = -1;
    for (int i = 0; i < 5; i++) {
        double rr = rho + steps[i] * 0.7;
        if (rr < rP - 1e-4 || rr > rP + rspan + 1e-4) continue;
        if (firstR < 0) firstR = rr;
        double wx = Vx + rr * cos(th), wz = Vz + rr * sin(th);
        bool occ = false;
        for (int j = 0; j < ns; j++) {
            if (j == self) continue;
            double ox = GetF(S[j], 0x48), oz = GetF(S[j], 0x50);
            if (sqrt((ox - Vx) * (ox - Vx) + (oz - Vz) * (oz - Vz)) > rP + rspan + 2.0) continue;
            if (sqrt((ox - wx) * (ox - wx) + (oz - wz) * (oz - wz)) < 0.7) { occ = true; break; }
        }
        if (!occ) { useR = rr; break; }
    }
    if (useR < 0) useR = firstR < 0 ? rho : firstR;
    *ex = (float)(Vx + useR * cos(th));
    *ez = (float)(Vz + useR * sin(th));
    return 0;
}

typedef void (__fastcall *FnInteg)(void* ecx);
static int CallInteg(void* e) {
    __try { ((FnInteg)(g_base + 0x367600))(e); return 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
}

// Harness's own v24 steering: same candidates, horizon 2.5 m, bias 0.3,
// neighbours = the listed soldiers S[] other than self.
static void ExpectSteer(uint8_t** S, int ns, uint8_t* self, double Vx, double Vz, double R, float* ex, float* ez) {
    const double PI = 3.14159265358979;
    const double ang[11] = { 0, 15, -15, 30, -30, 45, -45, 60, -60, 75, -75 };
    double px = GetF(self, 0x48), pz = GetF(self, 0x50);
    double Dx = GetF(self, 0xD8), Dz = GetF(self, 0xE0);
    double L = sqrt(Dx * Dx + Dz * Dz);
    double ux = Dx / L, uz = Dz / L;
    double ox = px - Vx, oz = pz - Vz, od = sqrt(ox * ox + oz * oz);
    double qx = ox / od, qz = oz / od, dp = qx * ux + qz * uz;
    qx -= dp * ux; qz -= dp * uz;
    int best = 0;
    double bestS = -1e30;
    for (int a = 0; a < 11; a++) {
        double c = cos(ang[a] * PI / 180.0), sn = sin(ang[a] * PI / 180.0);
        double cx = ux * c - uz * sn, cz = ux * sn + uz * c;
        double fr = 2.5;
        for (int j = 0; j < ns; j++) {
            if (S[j] == self) continue;
            double rx = GetF(S[j], 0x48) - px, rz = GetF(S[j], 0x50) - pz;
            if (rx * rx + rz * rz >= (2.5 + R) * (2.5 + R)) continue;
            double sp = rx * cx + rz * cz;
            if (sp <= 0) continue;
            double l = fabs(cx * rz - cz * rx);
            if (l >= R) continue;
            double d = sp - sqrt(R * R - l * l);
            if (d < fr) fr = d;
        }
        if (fr < 0) fr = 0;
        double sc = fr * c + 0.3 * (cx * qx + cz * qz);
        if (sc > bestS + 1e-6) { bestS = sc; best = a; }
    }
    double c = cos(ang[best] * PI / 180.0), sn = sin(ang[best] * PI / 180.0);
    *ex = (float)((ux * c - uz * sn) * L);
    *ez = (float)((ux * sn + uz * c) * L);
}

static void Steer(const char* tag, uint8_t* e, float ex, float ez) {
    int fault = CallInteg(e);
    float ox = GetF(e, 0xD8), oz = GetF(e, 0xE0);
    CHECK(!fault && fabsf(ox - ex) <= 1e-3f && fabsf(oz - ez) <= 1e-3f,
          "%s: D -> (%.4f, %.4f), expect (%.4f, %.4f)", tag, ox, oz, ex, ez);
}

// v27: goal call (portal 0) with the slot (sx, sz) as the game's waypoint;
// the buffer must come back as (ex, ez).
static void Goal(const char* tag, uint8_t* s, float sx, float sz, float ex, float ez) {
    uint32_t jnz = 0;
    memset(s + 0x454, 0xCC, 0x14);
    int fault = RunPatch(s, nullptr, sx, 2.5f, sz, &jnz);
    float ox = GetF(s, 0x454), oz = GetF(s, 0x45C);
    CHECK(!fault && fabsf(ox - ex) <= 1e-3f && fabsf(oz - ez) <= 1e-3f,
          "%s: slot (%.4f, %.4f) -> out (%.4f, %.4f), expect (%.4f, %.4f)", tag, sx, sz, ox, oz, ex, ez);
    Sleep(120);
}

// v25: D must stay, and the damping byte e+0xD6 must end as `want`.
static void NoDamp(const char* tag, uint8_t* e, int want) {
    e[0xD6] = 1;
    float dx = GetF(e, 0xD8), dz = GetF(e, 0xE0);
    int fault = CallInteg(e);
    CHECK(!fault && GetF(e, 0xD8) == dx && GetF(e, 0xE0) == dz && e[0xD6] == want,
          "%s: D kept (%.4f, %.4f), e+0xD6 = %d (want %d)", tag, GetF(e, 0xD8), GetF(e, 0xE0), e[0xD6], want);
}

static void Feed(uint8_t* s, Portal& p, float wx, float wz) {   // apex call, no check
    uint32_t jnz = 0;
    RunPatch(s, p.P, wx, 2.5f, wz, &jnz);
    Sleep(110);
}

// ---- v33 L helpers --------------------------------------------------------
static double QpcMs() {
    static LARGE_INTEGER f = {};
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}
// Movers: positions follow x0 + v * (t - tRef), so every snapshot the DLL
// takes sees a consistent velocity.
struct MvH { uint8_t* s; double x0, z0, vx, vz, tRef; };
static MvH g_mv[16];
static int g_nmv = 0;
static void MvClear() { g_nmv = 0; }
static void MvAdd(uint8_t* p, double x, double z, double vx, double vz, double tRef) {
    g_mv[g_nmv++] = { p, x, z, vx, vz, tRef };
}
static void Advance() {
    double t = QpcMs();
    for (int i = 0; i < g_nmv; i++) {
        double dt = (t - g_mv[i].tRef) / 1000.0;
        PutF(g_mv[i].s, 0x48, (float)(g_mv[i].x0 + g_mv[i].vx * dt));
        PutF(g_mv[i].s, 0x50, (float)(g_mv[i].z0 + g_mv[i].vz * dt));
    }
}
// Three unit snapshots 130 ms apart (W, W2: listed soldiers that follow no path).
static void Prime(uint8_t* W, uint8_t* W2, int n = 3) {
    for (int k = 0; k < n; k++) {
        Advance();
        CallInteg(W);
        if (W2) CallInteg(W2);
        Sleep(130);
    }
}
static void Follow(uint8_t* me, float sx, float sz) {   // a goal call: the man follows a path
    uint32_t jnz = 0;
    RunPatch(me, nullptr, sx, 2.5f, sz, &jnz);
}

// Harness copy of the L rule for a man with D = (0.6, 0) (6 m/s along +x),
// radii 0.43 (R = 0.86). Obstacles absolute (x, z) with their true velocity.
struct AvObH { double x, z, vx, vz; };
static int ExpAvoidDeg(double px, double pz, double H, const AvObH* ob, int n, int sideFilter, int wallSide,
                       int prefSide, double* f0out) {
    const int deg[13] = { 0, 10, -10, 20, -20, 30, -30, 45, -45, 60, -60, 75, -75 };
    const double PI = 3.14159265358979, R = 0.86, s = 6.0, minFree = 0.86;
    double f[13];
    for (int k = 0; k < 13; k++) {
        double cx = cos(deg[k] * PI / 180.0), cz = sin(deg[k] * PI / 180.0);
        double fr = H;
        for (int j = 0; j < n; j++) {
            double ax = ob[j].x - px, az = ob[j].z - pz;
            double sp = sqrt(ob[j].vx * ob[j].vx + ob[j].vz * ob[j].vz);
            double dd = sqrt(ax * ax + az * az);
            if (dd > H + R + sp * H / s) continue;
            bool mover;
            if (sp < 0.8) mover = false;
            else if (ob[j].vx >= 0.70710678 * sp && sp >= 3.0) continue;   // same way: ignored
            else mover = true;
            double d;
            if (!mover) {
                double spr = ax * cx + az * cz;
                if (dd < R) { if (spr > 0) fr = 0; continue; }
                if (spr <= 0) continue;
                double l = cx * az - cz * ax;
                if (l * l >= R * R) continue;
                d = spr - sqrt(R * R - l * l);
            } else {
                double wx = s * cx - ob[j].vx, wz = s * cz - ob[j].vz;
                double a = wx * wx + wz * wz, pw = ax * wx + az * wz, cc = dd * dd - R * R;
                if (a < 1e-6) continue;
                if (cc < 0) { if (pw > 0) fr = 0; continue; }
                if (pw <= 0) continue;
                double disc = pw * pw - a * cc;
                if (disc < 0) continue;
                d = s * (pw - sqrt(disc)) / a;
            }
            if (d < 0) d = 0;
            if (d < fr) fr = d;
        }
        f[k] = fr;
    }
    *f0out = f[0];
    if (f[0] >= H - 1e-4) return 0;
    auto pick = [&](int filt) {
        int best = 0;
        double bc = (H - f[0]) * (H - f[0]);
        for (int k = 1; k < 13; k++) {
            int side = deg[k] > 0 ? 1 : -1;
            if (wallSide && side == wallSide) continue;
            if (filt && side != filt) continue;
            if (f[k] < minFree) continue;
            double c = H * H + f[k] * f[k] - 2 * H * f[k] * cos(deg[k] * PI / 180.0);
            if (side != prefSide) c += 1e-3;
            if (c < bc - 1e-6) { bc = c; best = k; }
        }
        return best;
    };
    if (sideFilter && sideFilter == wallSide) sideFilter = 0;
    int best = pick(sideFilter);
    if (sideFilter && (best == 0 || f[best] < 0.5)) best = pick(0);
    return deg[best];
}
static void CheckAv(const char* tag, int fault, uint8_t* me, int deg) {
    const double PI = 3.14159265358979;
    double ex = 0.6 * cos(deg * PI / 180.0), ez = 0.6 * sin(deg * PI / 180.0);
    double ox = GetF(me, 0xD8), oz = GetF(me, 0xE0);
    CHECK(!fault && fabs(ox - ex) <= 1e-3 && fabs(oz - ez) <= 1e-3,
          "%s: D -> (%.4f, %.4f), expect %+d deg (%.4f, %.4f)", tag, ox, oz, deg, ex, ez);
}
static uint8_t* g_thrMan[2];
static volatile LONG g_thrFaults = 0, g_thrCalls = 0;
static DWORD WINAPI IntegThread(LPVOID p) {
    uint8_t* me = g_thrMan[(int)(intptr_t)p];
    double t0 = QpcMs();
    while (QpcMs() - t0 < 300.0) {
        PutF(me, 0xD8, 0.6f); PutF(me, 0xE0, 0.0f);
        if (CallInteg(me)) InterlockedIncrement(&g_thrFaults);
        InterlockedIncrement(&g_thrCalls);
        Sleep(1);
    }
    return 0;
}

// v31: harness copy of the F rule. Members: the listed soldiers (all fed apex
// P at corner V within the last 1.5 s) within 30 m of V, plus self. Returns
// the rank q; *R the lane radius; (ex, ez) the expected waypoint; *past when
// the man is beyond the free sector (game point expected).
static double ExpectTip(uint8_t** S, const int* mem, int nm, int self, double Vx, double Vz, double Px, double Pz,
                        double mx, double mz, double L, double qFixed, double sgnFixed,
                        float* ex, float* ez, double* Rout, bool* past) {
    const double PI = 3.14159265358979;
    double X = GetF(S[self], 0x48), Z = GetF(S[self], 0x50);
    double q = qFixed, sgn = sgnFixed;
    if (q < 0) {
        int idx[16], m = 0;
        idx[m++] = self;
        for (int i = 0; i < nm; i++) {
            if (mem[i] == self) continue;
            double x = GetF(S[mem[i]], 0x48), z = GetF(S[mem[i]], 0x50);
            if (sqrt((x - Vx) * (x - Vx) + (z - Vz) * (z - Vz)) > 30.0) continue;
            idx[m++] = mem[i];
        }
        double dx = 0, dz = 0;
        for (int j = 0; j < m; j++) {
            double x = GetF(S[idx[j]], 0x48), z = GetF(S[idx[j]], 0x50);
            double ux = Px - x, uz = Pz - z, l = sqrt(ux * ux + uz * uz);
            dx += ux / l; dz += uz / l;
        }
        double dl = sqrt(dx * dx + dz * dz);
        dx /= dl; dz /= dl;
        double ux = -dz, uz = dx;
        if ((Px - Vx) * ux + (Pz - Vz) * uz < 0) { ux = -ux; uz = -uz; }
        double lam0 = (X - Vx) * ux + (Z - Vz) * uz;
        int below = 0;
        for (int j = 1; j < m; j++) {
            double lj = (GetF(S[idx[j]], 0x48) - Vx) * ux + (GetF(S[idx[j]], 0x50) - Vz) * uz;
            if (lj < lam0) below++;
        }
        q = m > 1 ? (double)below / (m - 1) : 0.0;
    }
    if (sgn == 0) {
        double ax = Px - X, az = Pz - Z;
        double c = ax * (Vz - Z) - az * (Vx - X);
        sgn = c > 0 ? 1.0 : -1.0;
    }
    double rP = sqrt((Px - Vx) * (Px - Vx) + (Pz - Vz) * (Pz - Vz));
    double beta = atan2(Pz - Vz, Px - Vx);
    double mlen = sqrt(mx * mx + mz * mz);
    double half = (90.0 + (mlen > 1.0 ? acos(1.0 / mlen) * 180.0 / PI : 0.0) - 15.0) * PI / 180.0;
    double W = L < 4.0 ? L : 4.0;
    double R = rP + q * W;
    double rx = X - Vx, rz = Z - Vz, d = sqrt(rx * rx + rz * rz);
    double relth = atan2(rz, rx) - beta;
    while (relth > PI) relth -= 2 * PI;
    while (relth < -PI) relth += 2 * PI;
    *Rout = R;
    *past = sgn * relth > half;
    double f = (d - R) / 3.0;
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    double delta = (60.0 + (30.0 - 60.0) * f) * PI / 180.0;
    double tang = d > R ? acos(R / d) : 0.0;
    double tgt = relth + sgn * (tang + delta);
    if (tgt > half) tgt = half;
    if (tgt < -half) tgt = -half;
    *ex = (float)(Vx + R * cos(beta + tgt));
    *ez = (float)(Vz + R * sin(beta + tgt));
    return q;
}

// v42 park release (the same checks in the full run and in the release equivalence run). The
// leader is soldier [ctrl+0x30] = 2 of the unit [ctrl+4]; arrived = bytes +0x384 == 1, +0x308 == 1.
// Path M: nodes 0..1 wide (36 m, 20 m long), 2..3 narrow (5.5 m, 8 m), 4..5 wide (20 m): run [40, 56)
// from node 0. Ten men at 36 m: depth 0.85 m, so the park cap is D 11 + 0.85 + 3 = 14.85 m.
static void ParkSection(uint8_t* C, uint8_t* leader) {
    uint8_t* M[6];
    const float ml[6] = { 20, 20, 8, 8, 20, 20 };
    const float mw[6] = { 36, 36, 5.5f, 5.5f, 36, 36 };
    MakeNodes(M, 6, ml, mw);
    float cur[2];
    auto run = [&](uint8_t* node, float t) {
        PutU(C, 0x34, (uint32_t)(uintptr_t)node); PutF(C, 0x38, t);
        CallOrders(C);
        uint32_t u = (uint32_t)(uintptr_t)node;
        memcpy(cur, &u, 4); cur[1] = t;
        CallAdvance(cur, 11.0f);
    };
    auto at = [&](uint8_t* node) { return GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)node; };
    leader[0x384] = 1; leader[0x308] = 0;
    // PR1: cursor 10 m into node 1 (walk from there: run [20, 36)), P 21 in the run -> anchor 39.85 = node 4 t 0.1925
    run(M[1], 0.5f);
    CHECK(at(M[4]) && fabsf(cur[1] - 0.1925f) < 0.01f,
          "PR1 park setup: anchor past the narrow run (node %s t %.4f)", at(M[4]) ? "4" : "other", cur[1]);
    // PR2: leader arrived, cursor 6 m into node 4: 34 m of path left (> 14.85) -> vanilla P 17 = node 4 t 0.85
    leader[0x308] = 1;
    run(M[4], 0.3f);
    CHECK(at(M[4]) && fabsf(cur[1] - 0.85f) < 1e-3f,
          "PR2 leader arrived, 34 m of path left -> no park release, vanilla (node 4 t %.4f)", cur[1]);
    // PR3: cursor 7 m into node 5: 13 m left (<= 14.85), P 18 short of the end 20 -> front 0.5 m past the end
    run(M[5], 0.35f);
    CHECK(at(M[5]) && cur[1] >= 1.0f,
          "PR3 leader arrived, 13 m left, no run ahead -> front past the path end (node %s t %.4f >= 1)",
          at(M[5]) ? "5" : "other", cur[1]);
    // PR4: the leader moves again (not arrived): the release stays latched
    leader[0x308] = 0;
    run(M[5], 0.4f);
    CHECK(at(M[5]) && cur[1] >= 1.0f, "PR4 latched: leader moving again -> front still past the end (t %.4f)", cur[1]);
    // PR5: a new order (leader index 0 at the laying call) clears the latch and the anchor -> vanilla P 19 = t 0.95
    leader[0x308] = 1;
    PutU(C, 0x30, 0);
    run(M[5], 0.4f);
    PutU(C, 0x30, 2);
    CHECK(at(M[5]) && fabsf(cur[1] - 0.95f) < 1e-3f, "PR5 new order -> latch cleared, vanilla (t %.4f)", cur[1]);
    // PR6: leader arrived but no anchor on this path since the order -> vanilla
    run(M[5], 0.4f);
    CHECK(at(M[5]) && fabsf(cur[1] - 0.95f) < 1e-3f, "PR6 leader arrived, no anchor on this path -> vanilla (t %.4f)", cur[1]);
    // PR7: a second narrow run still ahead. Path Q: wide 20, narrow 8, wide 4, narrow 2, wide 9 (run 1 [20, 28),
    // run 2 [32, 34)). Anchor from run 1: 28 + 3.85 = 31.85 = node 2 t 0.9625; then the leader arrived, cursor 2 m
    // into node 2: 13 m left, run 2 at [4, 6) ahead -> no park release, vanilla P 13 = node 4 t 0.7778
    uint8_t* Q[5];
    const float ql[5] = { 20, 8, 4, 2, 9 };
    const float qw[5] = { 36, 5.5f, 36, 5.5f, 36 };
    MakeNodes(Q, 5, ql, qw);
    leader[0x308] = 0;
    run(Q[0], 0.5f);
    CHECK(at(Q[2]) && fabsf(cur[1] - 0.9625f) < 0.01f, "PR7a anchor past the first run (node %s t %.4f)",
          at(Q[2]) ? "2" : "other", cur[1]);
    leader[0x308] = 1;
    run(Q[2], 0.5f);
    CHECK(at(Q[4]) && fabsf(cur[1] - 0.7778f) < 1e-3f,
          "PR7 a narrow run still ahead -> no park release, vanilla (node %s t %.4f)", at(Q[4]) ? "4" : "other", cur[1]);
    leader[0x308] = 0;
    PutU(C, 0x34, 0); PutF(C, 0x38, 0.0f);
}

static void ReadText(const char* path, char* text, size_t cap) {
    size_t n = 0;
    FILE* f = fopen(path, "rb");
    if (f) { n = fread(text, 1, cap - 1, f); fclose(f); }
    text[n] = 0;
}

static long TotalsValue(const char* text, const char* name) {
    char key[96];
    sprintf(key, "\n%s,", name);
    const char* p = strstr(text, key);
    return p ? atol(p + strlen(key)) : -1;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc < 5) { printf("usage: hooktest41 <game dll> <proxy> <tag> full|crash|geo|play|pkey|release\n"); return 2; }
    bool crash = strcmp(argv[4], "crash") == 0;
    bool geo = strcmp(argv[4], "geo") == 0;
    bool play = strcmp(argv[4], "play") == 0;     // v41: no recording (v42: no P pressed)
    bool pkey = strcmp(argv[4], "pkey") == 0;     // v41: keys after P
    bool rec = strcmp(argv[4], "rec") == 0;       // v42: play, P toggles a recording
    bool release = strcmp(argv[4], "release") == 0;   // public release: G always on, equivalence with dev G
    SetEnvironmentVariableA("PATHFIX_CAPTURE", (play || rec || release) ? nullptr : "1");
    HMODULE game = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!game) { printf("cannot map game dll: %lu\n", GetLastError()); return 1; }
    g_base = (uint8_t*)game;

    const uint8_t stubBuild[]   = { 0x81, 0xC4, 0x8C, 0x01, 0x00, 0x00, 0xC2, 0x0C, 0x00 };
    const uint8_t stubCursor[]  = { 0x8B, 0xE5, 0x5D, 0xC2, 0x04, 0x00 };
    const uint8_t stubOrders[]  = { 0x81, 0xC4, 0xC8, 0x00, 0x00, 0x00, 0xC2, 0x04, 0x00 };
    const uint8_t stubGoto[]    = { 0x5F, 0x5E, 0x5B, 0xC2, 0x04, 0x00 };
    const uint8_t stubOffmesh[] = { 0x5E, 0xC2, 0x04, 0x00 };
    Patch(g_base + 0x39BB40 + 6, stubBuild, sizeof(stubBuild));
    Patch(g_base + 0x1A9750 + 6, stubCursor, sizeof(stubCursor));
    Patch(g_base + 0x1BC880 + 6, stubOrders, sizeof(stubOrders));
    // v38: the advance call site must be the analysed bytes (PUSH EAX; PUSH ECX; LEA ECX,[ESP+0xA0];
    // MOVSS [ESP],XMM0; CALL), then PUSH 1; PUSH EDX; NOPs replace the first 14 and a RET follows the CALL
    const uint8_t siteSeq[] = { 0x50, 0x51, 0x8D, 0x8C, 0x24, 0xA0, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x11, 0x04, 0x24, 0xE8 };
    if (memcmp(g_base + 0x1BC975, siteSeq, sizeof(siteSeq)) != 0) { printf("advance call site bytes differ\n"); return 1; }
    const uint8_t thunk[14] = { 0x6A, 0x01, 0x52, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
    Patch(g_base + 0x1BC975, thunk, sizeof(thunk));
    const uint8_t ret1[1] = { 0xC3 };
    Patch(g_base + 0x1BC988, ret1, 1);
    const uint8_t advPro[] = { 0x83, 0xEC, 0x10, 0x56, 0x57 };
    if (memcmp(g_base + 0x3670A0, advPro, 5) != 0) { printf("advance prologue differs\n"); return 1; }
    Patch(g_base + 0x3DBBF0 + 5, stubGoto, sizeof(stubGoto));
    Patch(g_base + 0x3DAFB0 + 6, stubOffmesh, sizeof(stubOffmesh));
    const uint8_t stubInteg[]   = { 0x8B, 0xE5, 0x5D, 0xC3 };     // MOV ESP,EBP; POP EBP; RET
    const uint8_t integPro[]    = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0 };
    if (memcmp(g_base + 0x367600, integPro, 6) != 0) { printf("integrator prologue differs\n"); return 1; }
    Patch(g_base + 0x367600 + 6, stubInteg, sizeof(stubInteg));
    // the instructions between the patch and the JNZ must be the analysed ones
    const uint8_t copySeq[] = { 0x8D, 0x8B, 0x54, 0x04, 0x00, 0x00, 0x0F, 0x10, 0x00, 0x0F, 0x11, 0x01,
                                0x8B, 0x40, 0x10, 0x89, 0x41, 0x10, 0x75, 0x74 };
    if (memcmp(g_base + 0x3DF6C7, copySeq, sizeof(copySeq)) != 0) { printf("copy sequence differs\n"); return 1; }
    g_backTarget = (uint32_t)(uintptr_t)&BackFromPatch;
    uint8_t stubJnz[12] = { 0x0F, 0x95, 0xC0, 0x0F, 0xB6, 0xC0, 0xFF, 0x25 };
    uint32_t bt = (uint32_t)(uintptr_t)&g_backTarget;
    memcpy(stubJnz + 8, &bt, 4);
    Patch(g_base + 0x3DF6D9, stubJnz, sizeof(stubJnz));
    g_patchSite = (uint32_t)(uintptr_t)(g_base + 0x3DF6C0);
    // v34: the radius patch point and what follows it must be the analysed bytes
    const uint8_t radSeq[] = { 0x80, 0x7C, 0x24, 0x0F, 0x01, 0x74, 0x1F, 0x8B, 0x43, 0x28, 0x8D, 0x4C };
    if (memcmp(g_base + 0x3DF4D1, radSeq, sizeof(radSeq)) != 0) { printf("radius patch bytes differ\n"); return 1; }
    g_backRadius = (uint32_t)(uintptr_t)&BackFromRadius;
    uint8_t stubRad[6] = { 0xFF, 0x25 };
    uint32_t br = (uint32_t)(uintptr_t)&g_backRadius;
    memcpy(stubRad + 2, &br, 4);
    Patch(g_base + 0x3DF4D6, stubRad, sizeof(stubRad));
    g_radiusSite = (uint32_t)(uintptr_t)(g_base + 0x3DF4D1);

    HMODULE proxy = LoadLibraryA(argv[2]);
    if (!proxy) { printf("cannot load proxy: %lu\n", GetLastError()); return 1; }
    DI8Create create = (DI8Create)GetProcAddress(proxy, "DirectInput8Create");
    g_keys = (TestKeys)GetProcAddress(proxy, "_PathfixTestKeys@4");
    if (!g_keys) g_keys = (TestKeys)GetProcAddress(proxy, "PathfixTestKeys");
    g_order = (TestOrder)GetProcAddress(proxy, "_PathfixTestOrder@4");
    if (!g_order) g_order = (TestOrder)GetProcAddress(proxy, "PathfixTestOrder");
    g_gnow = (TestGameNow)GetProcAddress(proxy, "_PathfixTestGameNow@0");
    if (!g_gnow) g_gnow = (TestGameNow)GetProcAddress(proxy, "PathfixTestGameNow");
    g_variant = (TestVariant)GetProcAddress(proxy, "_PathfixTestVariant@4");
    if (!g_variant) g_variant = (TestVariant)GetProcAddress(proxy, "PathfixTestVariant");
    if (!g_variant && !release) { printf("PathfixTestVariant export missing\n"); return 2; }
    if (!g_order && !release) { printf("PathfixTestOrder export missing\n"); return 2; }
    if (!create || (!g_keys && !release)) { printf("missing export (create %p keys %p)\n", create, g_keys); return 1; }
    void* out = nullptr;
    static const GUID iidDI8A = {0xBF798030,0x483A,0x4DA2,{0xAA,0x99,0x5D,0x64,0xED,0x36,0x97,0x00}};
    create(GetModuleHandleA(nullptr), 0x0800, &iidDI8A, &out, nullptr);
    printf("TAG %s mode %s\n", argv[3], argv[4]);
    printf("game 0x%08X proxy 0x%08X; patch site bytes %02X %02X (E9 = patched)\n", (uint32_t)(uintptr_t)g_base,
           (uint32_t)(uintptr_t)proxy, g_base[0x3DF6C0], g_base[0x3DF6C1]);
    CHECK(g_base[0x3DF6C0] == 0xE9 && g_base[0x3DF6C5] == 0x90 && g_base[0x3DF6C6] == 0x90,
          "patch at 0x3DF6C0 is JMP + 2 NOPs");

    char exeDir[MAX_PATH], runDir[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
    *(strrchr(exeDir, '\\') + 1) = 0;
    char latest[MAX_PATH];
    sprintf(latest, "%spathfix_runs\\latest.txt", exeDir);
    FILE* lf = fopen(latest, "r");
    if (lf) {
        char name[64] = "";
        fgets(name, sizeof(name), lf);
        fclose(lf);
        char* nl = strpbrk(name, "\r\n");
        if (nl) *nl = 0;
        sprintf(runDir, "%spathfix_runs\\%s\\", exeDir, name);
    }
    printf("run folder %s\n", runDir);

    // ---- fake unit of 10 soldiers; slots spread 9 m along the portal (x)
    const int NS = 10;
    uint8_t* U = Alloc(0x1000);
    uint8_t* C = Alloc(0x1000);
    uint8_t* Pb = Alloc(0x1000);
    uint8_t* Arr = Alloc(0x100);
    uint8_t* R = Alloc(0x100);
    PutF(R, 0x50, 1.1f);              // r = 0.55
    PutU(R, 0x0C, 0xC0441D00);        // corridor value seen in soldiers.csv
    uint8_t* S[NS];
    for (int i = 0; i < NS; i++) {
        S[i] = Alloc(0x800);
        SetFixSoldier(S[i], R, -1.5f + i, -20.0f, -1.5f + i, 5.0f);
        PutU(Arr, 4 * i, (uint32_t)(uintptr_t)S[i]);
    }
    PutU(U, 0x18, NS);
    PutU(U, 0x1C, (uint32_t)(uintptr_t)Arr);
    PutU(C, 0x04, (uint32_t)(uintptr_t)U);
    PutU(C, 0x18, (uint32_t)(uintptr_t)Pb);
    PutU(C, 0x30, 2);                 // v37: leader index
    C[0x0C] = 1;                      // v37: all ready
    PutU(C, 0x24, 1);                 // v37: speed mode run
    for (int i = 0; i < NS; i++) {    // v37: speed-law fields with recognisable values
        PutF(S[i], 0x110, 3.5f); PutF(S[i], 0x130, 1.25f); PutF(S[i], 0x150, 7.5f); PutF(S[i], 0x1AC, 1.2f);
        PutF(S[i], 0x1BC, 0.9f); PutF(S[i], 0x438, 3.5f); PutF(S[i], 0x43C, 3.14286f); S[i][0x440] = 0;
        PutF(S[i], 0x46C, 0.7f); S[i][0xF0] = 1; S[i][0x198] = 0; S[i][0x384] = 1; S[i][0x308] = 0;
    }
    PutU(Pb, 0xA8, 0);
    // v33: a second unit of 2 soldiers (T0 obstacle, T1 snapshot driver), parked far away
    const int NT = 2;
    uint8_t* U2 = Alloc(0x1000);
    uint8_t* C2 = Alloc(0x1000);
    uint8_t* Pb2 = Alloc(0x1000);
    uint8_t* Arr2 = Alloc(0x100);
    uint8_t* T[NT];
    for (int i = 0; i < NT; i++) {
        T[i] = Alloc(0x800);
        SetFixSoldier(T[i], R, 400.0f + 3 * i, 400.0f, 400.0f + 3 * i, 420.0f);
        PutU(Arr2, 4 * i, (uint32_t)(uintptr_t)T[i]);
    }
    PutU(U2, 0x18, NT);
    PutU(U2, 0x1C, (uint32_t)(uintptr_t)Arr2);
    PutU(C2, 0x04, (uint32_t)(uintptr_t)U2);
    PutU(C2, 0x18, (uint32_t)(uintptr_t)Pb2);
    PutU(Pb2, 0xA8, 0);
    uint8_t* Lone = Alloc(0x800);     // a soldier no FUN_101bc880 hit ever listed
    SetFixSoldier(Lone, R, 1.0f, -20.0f, 1.0f, 5.0f);

    // Building corners at A = (0, 0) and B = (10, 0), both right angles
    // (|m| = 1.414): pA = (r, -r), pB = (10 - r, -r); lane along x at
    // z = -r, L = 10 - 2r = 8.9.
    const float r = 0.55f;
    Portal pA = MakePortal(0, 0, 1, -1, 10, 0, -1, -1, 0, 0);
    Portal pLink = MakePortal(0, 0, 1, -1, 10, 0, -1, -1, 0, 0x1234560);
    Portal pGate = MakePortal(0, 0, 1, -1, 10, 0, -1, -1, 2, 0);          // gate piece, vectors at +0x38 / +0x44
    Portal pEdge = MakePortal(0, 0, 1, -1, 10, 0, -1, -1, 7, 0);          // type >= 4: edge direction
    Portal pNarrow = MakePortal(0, 0, 1, -1, 1.0f, 0, -1, -1, 0, 0);       // pB left of pA: crossed
    Portal pOther = MakePortal(0, 0, 1, -1, 10, 0, 0, 0, 0, 0);            // |mB| = 0
    const float L = 10.0f - 2 * r;
    const float zl = -r;

    // v36: the engine's tick counter hangs off the controller: [[ctrl+0x14]+8]+0x50. Both
    // controllers share one clock object; Advance() moves it.
    g_clk = Alloc(0x100);
    uint8_t* P1 = Alloc(0x100);
    PutU(P1, 0x08, (uint32_t)(uintptr_t)g_clk);
    PutU(C, 0x14, (uint32_t)(uintptr_t)P1);
    PutU(C2, 0x14, (uint32_t)(uintptr_t)P1);
    PutU(g_clk, 0x50, GT_TICK0);
    CallCursor(C);    // window opens; soldier sample with s_corridor
    CallOrders(C);    // soldier -> unit table, batch 1
    CallOrders(C2);   // v33: second unit
    if (!release) CHECK(g_gnow != nullptr && g_gnow() == GT_TICK0 * 100,
          "GT1 the fix clock reads the controller's tick counter: %lu ms = tick %u * 100", g_gnow ? g_gnow() : 0, GT_TICK0);
    Advance(1000);
    if (!release) CHECK(g_gnow != nullptr && g_gnow() == (GT_TICK0 + 36) * 100,
          "GT2 1 wall s equivalent = 36 ticks = 3600 game ms (%lu)", g_gnow ? g_gnow() : 0);
    printf("unit 0x%08X, soldiers S0 0x%08X .. S9 0x%08X, portal 0x%08X\n", (uint32_t)(uintptr_t)U,
           (uint32_t)(uintptr_t)S[0], (uint32_t)(uintptr_t)S[9], (uint32_t)(uintptr_t)pA.P);

    if (crash) {
        Apex("K1 off apex", S[0], pA, r, zl, r, zl);
        Press(0);   // v41: I -> G
        Apex("K2 G on, S0 20 m from the corner: game point", S[0], pA, r, zl, r, zl);
        Sleep(300);
        printf("K3 crashing now: FUN_1036e710 with ECX = 0x10\n");
        float in[3] = { 1, 2, 3 }, res[3];
        ((FnXform)(g_base + 0x36E710))((void*)0x10, 0, res, in);   // no handler: unhandled
        printf("K4 NOT REACHED\n");
        return 3;
    }

    if (geo) {
        // 25 of the first 200 apex calls match neither candidate (> 10 %):
        // the fix must disable itself, with the alarm, while still OFF.
        for (int i = 0; i < 25; i++) Apex("G apex mismatch, off", S[i % NS], pA, 3.0f, zl, 3.0f, zl);
        Sleep(400);
        Press(0);   // I: refused
        Press(1);   // lane key: refused
        Apex("G2 after FIX DISABLED, F6 pressed: untouched", S[6], pA, r, zl, r, zl);
        FreeLibrary(proxy);
        char logPath[MAX_PATH + 32];
        sprintf(logPath, "%spathfix_log.txt", runDir);
        static char text[1 << 20];
        ReadText(logPath, text, sizeof(text));
        CHECK(strstr(text, "FIX DISABLED: apex match") != nullptr, "G3 log: FIX DISABLED (apex match)");
        CHECK(strstr(text, "ALARM sounded") != nullptr, "G4 log: alarm");
        CHECK(strstr(text, "I refused") != nullptr && strstr(text, "Lane key refused") != nullptr, "G5 log: I and lane key refused");
        printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 4 : 0;
    }

    if (play || pkey || rec) {
        // shared: G release on a fake path (as G1) and a lead-the-turn point (as LD2)
        auto gCheck = [&](const char* tag, bool expectRelease) {
            uint8_t* N[10];
            const float gl[10] = { 20, 20, 20, 8, 8, 8, 8, 20, 20, 20 };
            const float gw[10] = { 36, 36, 36, 5.5f, 5.5f, 5.5f, 5.5f, 36, 36, 36 };
            MakeNodes(N, 10, gl, gw);
            float cur[2];
            uint32_t u = (uint32_t)(uintptr_t)N[2];
            memcpy(cur, &u, 4); cur[1] = 0.5f;
            PutU(C, 0x34, u); PutF(C, 0x38, 0.5f);
            CallOrders(C);
            CallAdvance(cur, 11.0f);
            bool released = GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)N[7] && fabsf(cur[1] - 0.1925f) < 0.01f;
            bool vanilla = GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)N[3] && fabsf(cur[1] - 0.125f) < 1e-3f;
            CHECK(expectRelease ? released : vanilla, "%s (node %s t %.4f)", tag,
                  released ? "7" : vanilla ? "3" : "other", cur[1]);
            PutU(C, 0x34, 0); PutF(C, 0x38, 0.0f);
            Advance(1000);   // a game second: the next order is a new laying call, no hold carried over
        };
        auto leadCheck = [&](const char* tag) {
            g_wiggle = false;
            for (int i = 0; i < NS; i++) PutF(S[i], 0x50, -20.0f);
            PutF(S[1], 0x48, -2.0f); PutF(S[1], 0x50, -1.0f);
            float ex, ez;
            double sg = 0;
            int st = ExpectLead(S, NS, 1, 0, 0, r, zl, 1, -1, L, 0, &ex, &ez, &sg);
            CHECK(st == 0, "%s: expectation computed (status %d)", tag, st);
            Apex(tag, S[1], pA, r, zl, ex, ez);
            PutF(S[1], 0x48, -1.5f + 1); PutF(S[1], 0x50, -20.0f);
            g_wiggle = true;
            Advance(5000);   // let the stored turning sign and apex freshness expire
        };
        char logPath[MAX_PATH + 32];
        static char text[1 << 20];
        if (rec) {
            // v42: P starts a recording into a new run folder, P stops it; again; then P and the game closes.
            auto latestDir = [&](char* out) {
                out[0] = 0;
                FILE* f = fopen(latest, "r");
                if (!f) return;
                char name[64] = "";
                fgets(name, sizeof(name), f);
                fclose(f);
                char* nl = strpbrk(name, "\r\n");
                if (nl) *nl = 0;
                sprintf(out, "%spathfix_runs\\%s\\", exeDir, name);
            };
            auto dataRows = [&](const char* dir, const char* name) -> int {
                char p[MAX_PATH + 64];
                sprintf(p, "%s%s", dir, name);
                FILE* f = fopen(p, "r");
                if (!f) return -1;
                int lines = 0, c, last = '\n';
                while ((c = fgetc(f)) != EOF) { if (c == '\n') lines++; last = c; }
                if (last != '\n') lines++;
                fclose(f);
                return lines - 1;   // header
            };
            auto fileHas = [&](const char* dir, const char* name, const char* what) -> bool {
                char p[MAX_PATH + 64];
                sprintf(p, "%s%s", dir, name);
                ReadText(p, text, sizeof(text));
                return strstr(text, what) != nullptr;
            };
            printf("rec (P toggles a recording):\n");
            CHECK(runDir[0] == 0, "RE1 no run folder at launch");
            char d1[MAX_PATH] = "", d2[MAX_PATH] = "", d3[MAX_PATH] = "";
            Press(4);   // P: recording 1 on
            latestDir(d1);
            CHECK(d1[0] && GetFileAttributesA(d1) != INVALID_FILE_ATTRIBUTES, "RE2 P -> recording 1 folder %s", d1);
            Press(0);   // I -> G
            gCheck("RE3 recording 1, G on: block released past the narrow run", true);
            Press(2);   // O: marker
            Press(4);   // P: recording 1 off
            Sleep(1000);
            int rows1 = dataRows(d1, "pathfix_capture_v42_release.csv");
            CHECK(rows1 == 1, "RE4 recording 1: release.csv with the RE3 row (%d data rows)", rows1);
            CHECK(fileHas(d1, "pathfix_log.txt", "PATHFIX BUILD v42 recording 1") && strstr(text, "RECORDING STOPPED") &&
                  strstr(text, "P: recording 1 OFF"), "RE5 recording 1: its own log, stopped");
            CHECK(fileHas(d1, "pathfix_capture_v42_events.csv", ",MARKER,") && strstr(text, ",FIXMODE,"),
                  "RE6 recording 1: events.csv with the O marker and the fix mode rows");
            CHECK(dataRows(d1, "pathfix_capture_v42_orders.csv") >= 1, "RE7 recording 1: orders.csv rows");
            gCheck("RE8 not recording: G still acts", true);
            Press(4);   // P: recording 2 on
            latestDir(d2);
            CHECK(d2[0] && strcmp(d1, d2) != 0 && GetFileAttributesA(d2) != INVALID_FILE_ATTRIBUTES,
                  "RE9 P again -> a new folder for recording 2 (%s)", d2);
            gCheck("RE10 recording 2: block released", true);
            Press(4);   // P: recording 2 off
            Sleep(1000);
            int rows2 = dataRows(d2, "pathfix_capture_v42_release.csv");
            CHECK(rows2 == 1 && dataRows(d1, "pathfix_capture_v42_release.csv") == 1,
                  "RE11 recording 2 starts empty: 1 release row there (%d), recording 1 untouched", rows2);
            CHECK(fileHas(d2, "pathfix_log.txt", "PATHFIX BUILD v42 recording 2") && strstr(text, "RECORDING STOPPED") &&
                  !strstr(text, "recording 1 ON"), "RE12 recording 2: its own log, without recording 1's lines");
            Press(4);   // P: recording 3 on, then the game closes
            latestDir(d3);
            Press(0);   // I -> off
            FreeLibrary(proxy);
            CHECK(d3[0] && strcmp(d3, d2) != 0 && fileHas(d3, "pathfix_log.txt", "(game closed)") &&
                  strstr(text, "RECORDING STOPPED"), "RE13 recording 3 still on when the game closes: stopped and written");
            sprintf(logPath, "%spathfix_log.txt", exeDir);
            ReadText(logPath, text, sizeof(text));
            const char* l = strstr(text, "\n[");
            bool stamped = l && strlen(l) > 16 && l[4] == ':' && l[7] == ':' && l[10] == '.' && l[14] == ']';   // l -> "\n[hh"
            CHECK(stamped, "RE14 session log lines start with the local time [hh:mm:ss.mmm]");
            CHECK(strstr(text, "P: recording 1 ON") && strstr(text, "P: recording 1 OFF") && strstr(text, "P: recording 2 ON") &&
                  strstr(text, "P: recording 2 OFF") && strstr(text, "P: recording 3 ON") && strstr(text, "P refused") == nullptr,
                  "RE15 session log: recordings 1..3 on and off, nothing refused");
        } else if (play) {
            printf("play (no recording):\n");
            CHECK(runDir[0] == 0, "PL1 no run folder named in pathfix_runs\\latest.txt");
            Apex("PL2 launch: off -> game point", S[3], pA, r, zl, r, zl);
            gCheck("PL3 off: vanilla advance", false);
            Press(0);   // I -> G
            leadCheck("PL4 G on: lead-the-turn point");
            gCheck("PL5 G on: block released past the narrow run", true);
            Press(2);   // O: no recording, ignored (v42: P would start one, not pressed here)
            Press(0);   // I -> off
            Apex("PL6 I again: off -> game point", S[3], pA, r, zl, r, zl);
            gCheck("PL7 off again: vanilla advance", false);
            Sleep(10500);   // longer than v40's flush period: nothing may appear
            FreeLibrary(proxy);
            char runs[MAX_PATH + 32];
            sprintf(runs, "%spathfix_runs", exeDir);
            CHECK(GetFileAttributesA(runs) == INVALID_FILE_ATTRIBUTES, "PL8 no pathfix_runs folder in %s", exeDir);
            char pat[MAX_PATH + 32];
            sprintf(pat, "%s*.csv", exeDir);
            WIN32_FIND_DATAA fd;
            HANDLE fh = FindFirstFileA(pat, &fd);
            CHECK(fh == INVALID_HANDLE_VALUE, "PL9 no CSV file in the exe folder (%s)", fh == INVALID_HANDLE_VALUE ? "none" : fd.cFileName);
            if (fh != INVALID_HANDLE_VALUE) FindClose(fh);
            sprintf(logPath, "%spathfix_log.txt", exeDir);
        } else {
            printf("pkey (keys after P):\n");
            Press(4);   // P: finish the capture
            Sleep(1500);
            Press(0);   // I -> G after the capture ended
            gCheck("PK1 G switched on after P: block released", true);
            Press(0);   // I -> off
            gCheck("PK2 off after P: vanilla advance", false);
            FreeLibrary(proxy);
            sprintf(logPath, "%spathfix_log.txt", runDir);
        }
        if (rec) {
            printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
            return g_fail ? 4 : 0;
        }
        ReadText(logPath, text, sizeof(text));
        CHECK(strstr(text, "PATHFIX BUILD v42") != nullptr, "LG1 log: v42 banner");
        const char* on = strstr(text, "I: path fix ON, variant G (mode 10");
        const char* off = on ? strstr(on, "I: path fix OFF (mode 0") : nullptr;
        CHECK(on && off, "LG2 log: I -> ON variant G, then I -> OFF");
        if (play) {
            CHECK(strstr(text, "Capture:     OFF") != nullptr, "LG3 log: capture OFF");
            CHECK(strstr(text, "CAPTURE WINDOW OPEN") == nullptr && strstr(text, "P: recording") == nullptr &&
                  strstr(text, "O: marker") == nullptr && strstr(text, "O ignored: not recording") != nullptr,
                  "LG4 log: no capture window, no recording, O ignored without a recording");
        } else {
            const char* p = strstr(text, "P: finishing capture");
            CHECK(p && on && on > p, "LG3 log: I switched the fix on after P finished the capture");
        }
        CHECK(strstr(text, "FIX DISABLED") == nullptr, "LG5 log: no self-check failure");
        printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 4 : 0;
    }

    if (release) {
        // Public release equivalence: the same scenarios through the dev DLL in variant G (reference)
        // and through the release DLL must give the same results.
        bool devRef = g_variant != nullptr;
        printf("release checks on the %s DLL:\n", devRef ? "dev (reference: variant G via the test export)" : "release");
        if (devRef) Variant(3);
        if (!devRef) {
            CHECK(GetProcAddress(proxy, "_PathfixTestKeys@4") == nullptr && GetProcAddress(proxy, "PathfixTestKeys") == nullptr &&
                  GetProcAddress(proxy, "_PathfixTestVariant@4") == nullptr && GetProcAddress(proxy, "_PathfixTestOrder@4") == nullptr &&
                  GetProcAddress(proxy, "_PathfixTestGameNow@0") == nullptr, "RS1 release: no test exports");
            CHECK(g_base[0x1BC880] == 0xE9 && g_base[0x3DF6C0] == 0xE9 && g_base[0x367600] == 0xE9 &&
                  g_base[0x3DF4D1] == 0xE9 && g_base[0x3670A0] == 0xE9, "RS2 release: the five G hooks are patched");
            CHECK(g_base[0x1F6BF0] != 0xE9 && g_base[0x2557E0] != 0xE9 && g_base[0x10BB50] != 0xE9 &&
                  g_base[0x35AE20] != 0xE9 && g_base[0x37A850] != 0xE9 && g_base[0x20CAA0] != 0xE9 &&
                  g_base[0x20CC30] != 0xE9 && g_base[0x128570] != 0xE9 && g_base[0x1A9750] != 0xE9 &&
                  g_base[0x39BB40] != 0xE9 && g_base[0x3DBBF0] != 0xE9 && g_base[0x3DAFB0] != 0xE9,
                  "RS3 release: none of the twelve capture-only functions is patched");
        }
        printf("always on, no key pressed:\n");
        Apex("R1 apex outside the 6 m zone -> game point", S[3], pA, r, zl, r, zl);
        {   // goal call: portal 0
            uint32_t jnz = 0;
            memset(S[4] + 0x454, 0xCC, 0x14);
            RunPatch(S[4], nullptr, 33.0f, 2.5f, 44.0f, &jnz);
            CHECK(Near(GetF(S[4], 0x454), 33.0f) && Near(GetF(S[4], 0x45C), 44.0f), "R2 goal call (portal 0) untouched");
        }
        {   // JNZ path with byte +0x471 == 1: the stolen CMP must set ZF
            uint32_t jnz = 0xFF;
            S[5][0x471] = 1;
            RunPatch(S[5], pA.P, r, 2.5f, zl, &jnz);
            CHECK(jnz == 0, "R3 [soldier+0x471] == 1 -> ZF set after the stolen CMP (stub saw %u, expect 0)", jnz);
            S[5][0x471] = 0;
        }
        printf("lead the turn (verbatim from the full run):\n");
    g_wiggle = false;
    for (int i = 0; i < NS; i++) PutF(S[i], 0x50, -20.0f);
    {
        float ex, ez;
        double sg = 0;
        int st;
        // LD1 outside the 6 m zone: the game's point
        Apex("LD1 outside the zone -> game point", S[0], pA, r, zl, r, zl);
        // LD2 inside the zone, free point (S1 at (-2, -1); everyone else 20 m away)
        PutF(S[1], 0x48, -2.0f); PutF(S[1], 0x50, -1.0f);
        st = ExpectLead(S, NS, 1, 0, 0, r, zl, 1, -1, L, 0, &ex, &ez, &sg);
        CHECK(st == 0, "LD2 expectation computed (status %d, sign %+.0f)", st, sg);
        Apex("LD2 free lead point", S[1], pA, r, zl, ex, ez);
        // LD3 occupied: S3 stands on S2's lead point -> next radius
        Advance(3200);   // S1's stored sign no longer matters (other soldier anyway)
        PutF(S[2], 0x48, -1.0f); PutF(S[2], 0x50, -2.0f);
        {
            float fx, fz;
            double sg2 = 0;
            ExpectLead(S, NS, 2, 0, 0, r, zl, 1, -1, L, 0, &fx, &fz, &sg2);   // free point first
            {   // S3 0.1 m inside S2's free point (towards V): rho is occupied, rho + 0.7 clearly free
                double l = sqrt((double)fx * fx + (double)fz * fz);
                PutF(S[3], 0x48, (float)(fx - 0.1 * fx / l)); PutF(S[3], 0x50, (float)(fz - 0.1 * fz / l));
            }
            st = ExpectLead(S, NS, 2, 0, 0, r, zl, 1, -1, L, 0, &ex, &ez, &sg2);
            CHECK(st == 0 && (fabs(ex - fx) > 0.3 || fabs(ez - fz) > 0.3), "LD3 expectation moved off the occupied point");
            Apex("LD3 occupied point -> next free radius", S[2], pA, r, zl, ex, ez);
            PutF(S[3], 0x48, -1.5f + 3); PutF(S[3], 0x50, -20.0f);
        }
        // LD4 stored sign: S4 first at (-1, -2) (sign from the geometry), then
        // moved past the bisector within 3 s: same sign, lead clamped to the edge
        PutF(S[4], 0x48, -1.0f); PutF(S[4], 0x50, -2.0f);
        double s4 = 0;
        ExpectLead(S, NS, 4, 0, 0, r, zl, 1, -1, L, 0, &ex, &ez, &s4);
        Apex("LD4a first call, sign from the geometry", S[4], pA, r, zl, ex, ez);
        PutF(S[4], 0x48, 2.0f); PutF(S[4], 0x50, -0.5f);
        double s4b = 0;
        st = ExpectLead(S, NS, 4, 0, 0, r, zl, 1, -1, L, s4, &ex, &ez, &s4b);
        CHECK(st == 0 && s4 > 0, "LD4 expectation: stored sign %+.0f kept", s4);
        Apex("LD4b past the bisector, stored sign, clamped to the sector edge", S[4], pA, r, zl, ex, ez);
        // LD5 beyond the far edge (same corner, stored sign): the game's point
        PutF(S[4], 0x48, -0.2f); PutF(S[4], 0x50, 2.0f);
        st = ExpectLead(S, NS, 4, 0, 0, r, zl, 1, -1, L, s4, &ex, &ez, &s4b);
        CHECK(st == 2, "LD5 expectation: beyond the far edge (status %d)", st);
        Apex("LD5 beyond the far edge -> game point", S[4], pA, r, zl, r, zl);
        // LD6 short lane (B at (3, 0): L = 1.9): radius cut to |P - V| + 1.9
        Portal pShort = MakePortal(0, 0, 1, -1, 3.0f, 0, -1, -1, 0, 0);
        PutF(S[5], 0x48, -4.5f); PutF(S[5], 0x50, -2.5f);
        double Ls = 3.0 - 2 * r;
        st = ExpectLead(S, NS, 5, 0, 0, r, zl, 1, -1, Ls, 0, &ex, &ez, &sg);
        CHECK(st == 0, "LD6 expectation computed");
        Apex("LD6 short lane: radius cut", S[5], pShort, r, zl, ex, ez);
        for (int i = 1; i < 6; i++) { PutF(S[i], 0x48, -1.5f + i); PutF(S[i], 0x50, -20.0f); }
    }
    Apex("M1 lone soldier (no unit) untouched", Lone, pA, r, zl, r, zl);
    Apex("M1 special link untouched", S[1], pLink, r, zl, r, zl);
    Advance(1600);
    Apex("M1 type 2 gate, apex pA, outside the zone -> game point", S[3], pGate, r, zl, r, zl);
    Advance(1600);
    Apex("M1 type 2 gate, apex pB, outside the zone -> game point", S[0], pGate, 10 - r, zl, 10 - r, zl);
    Advance(1600);
    Apex("M1 type 7 edge, left (r, 0), outside the zone -> game point", S[6], pEdge, r, 0, r, 0);
    Apex("M1 narrow (candidates crossed) untouched", S[2], pNarrow, r, zl, r, zl);
    Advance(1600);
    Apex("M1 other end |mB| 0 pulled in, outside the zone -> game point", S[9], pOther, r, zl, r, zl);
    Apex("M1 vertex mismatch untouched", S[3], pA, 3.0f, zl, 3.0f, zl);
    Apex("M1 v17 candidate A + d*r (on the edge) is a mismatch now", S[3], pA, r, 0, r, 0);
    {   // fault: portal pointer to a freed page
        uint8_t* gone = Alloc(0x1000);
        VirtualFree(gone, 0, MEM_RELEASE);
        uint32_t jnz = 0;
        memset(S[6] + 0x454, 0xCC, 0x14);
        int f = RunPatch(S[6], gone, r, 2.5f, zl, &jnz);
        CHECK(!f && Near(GetF(S[6], 0x454), r) && Near(GetF(S[6], 0x45C), zl), "M1 unreadable portal: no crash, untouched");
        Sleep(120);
    }
    g_wiggle = true;

        printf("no crowd damping (G: within 30 m of the vertex):\n");
        {
            g_wiggle = false;
            for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -1.5f + i); PutF(S[i], 0x50, -20.0f); PutF(S[i], 0x58, 0.43f); }
            Advance(5000);
            uint8_t* me = S[4];
            PutF(me, 0x48, -2.0f); PutF(me, 0x50, -2.0f);
            Feed(me, pA, r, zl);                          // apex pA: corner vertex A = (0, 0)
            PutF(me, 0xD8, 0.4781f); PutF(me, 0xDC, 0.0f); PutF(me, 0xE0, 0.2719f);
            NoDamp("RN1 corner man 2.8 m from the vertex -> damping off", me, 0);
            PutF(me, 0x48, -13.0f); PutF(me, 0x50, -2.0f); PutF(me, 0xD8, 0.55f); PutF(me, 0xE0, 0.0f);
            NoDamp("RN2 13 m from the vertex -> damping off (G zone 30 m)", me, 0);
            PutF(me, 0x48, -31.0f);
            NoDamp("RN3 31 m from the vertex -> damping kept", me, 1);
            PutF(me, 0x48, -2.0f);
            {   // a non-finite move D: the soldier is left alone
                float nan; uint32_t bits = 0x7FC00000; memcpy(&nan, &bits, 4);
                PutF(me, 0xD8, nan);
                me[0xD6] = 1;
                CallInteg(me);
                CHECK(me[0xD6] == 1, "RN4 non-finite move -> damping kept");
                PutF(me, 0xD8, 0.55f);
            }
            PutF(Lone, 0x48, -2.0f); PutF(Lone, 0x50, -2.0f); PutF(Lone, 0xD8, 0.5f); PutF(Lone, 0xE0, 0.2f);
            NoDamp("RN5 unlisted soldier -> damping kept", Lone, 1);
            Advance(1700);
            NoDamp("RN6 stale apex (1.7 s game-equivalent) -> damping kept", me, 1);
            for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -1.5f + i); PutF(S[i], 0x50, -20.0f); }
            g_wiggle = true;
        }
        printf("radius lanes (verbatim from the full run):\n");
    const float MX = 100.0f, MZ = 100.0f;
    {
        g_wiggle = false;
        if (g_order) g_order((uint32_t)(uintptr_t)U);   // a unit order note (dev only)
        const float W0 = 1.2f;
        auto rad = [&](uint8_t* p) -> float { return RunRadius(p, W0, GetF(p, 0x420), GetF(p, 0x428)); };
        auto feed = [&](int nmen) { for (int i = 0; i < nmen; i++) { uint32_t j = 0; RunPatch(S[i], pA.P, r, 2.5f, zl, &j); } };
        float w;
        Advance(1600);                     // every stored apex stale
        for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -4.0f + i); PutF(S[i], 0x50, -8.0f); }
        // RQ0 the stolen CMP is replayed for the JZ
        g_cmpByte = 1; w = rad(S[4]);
        CHECK(g_cmpZ == 1, "RQ0a [ESP+0xF] == 1 -> ZF set after the replayed CMP (got %u)", g_cmpZ);
        g_cmpByte = 0; w = rad(S[4]);
        CHECK(g_cmpZ == 0, "RQ0b [ESP+0xF] == 0 -> ZF clear (got %u)", g_cmpZ);
        g_cmpByte = 1;
        CHECK(w == W0, "RQ1 no corner (no fresh apex) -> w unchanged (%.3f)", w);
        feed(7);
        w = rad(S[3]);
        CHECK(w == W0, "RQ2 queue of 7 (< 8) -> w unchanged (%.3f)", w);
        Apex("RQ3 RC: apex call outside the 6 m lead zone -> waypoint untouched", S[9], pA, r, zl, r, zl);
        feed(10);
        w = rad(S[0]);
        CHECK(fabsf(w - W0) < 1e-5f, "RQ4 queue of 10, innermost S0 -> lane 0, w0 (%.3f)", w);
        w = rad(S[4]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ5 S4 (q 4/9) -> lane 1, w0 + 1.5 (%.3f)", w);
        w = rad(S[9]);
        CHECK(fabsf(w - 4.2f) < 1e-5f, "RQ6 outermost S9 -> lane 2, w0 + 3.0 capped at 4.2 (%.3f)", w);
        rad(S[6]); rad(S[7]);
        w = rad(S[8]);
        CHECK(fabsf(w - 4.2f) < 1e-5f, "RQ7 S8 -> lane 2 (4 men there now, cap 1.3 * 10 / 3 = 4.33) (%.3f)", w);
        // S5 to the outer edge: rank 2, lane 2 holds 4 -> overfull -> adjacent lane 1 (lane 0 is two away)
        PutF(S[5], 0x48, 6.0f);
        feed(10);
        w = rad(S[5]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ8 overfull lane 2 -> adjacent lane 1, not lane 0 (%.3f)", w);
        w = RunRadius(S[4], W0, 999.0f, 999.0f);
        CHECK(w == W0, "RQ9 bad frame (request end != slot) -> w unchanged (%.3f)", w);
        w = rad(S[4]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ10 S4 again -> lane 1 kept (%.3f)", w);
        // stuck: S4 does not move while the queue stays fresh -> w0 after 1.5 s
        // GT4 (v36): wall time alone does not age the stuck clock
        Sleep(1700);
        feed(10);
        w = rad(S[4]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "GT4 1.7 wall s without game time -> not stuck, lane 1 kept (%.3f)", w);
        float last = 0.0f;
        for (int k = 0; k < 4; k++) { Advance(450); feed(10); last = rad(S[4]); }
        CHECK(last == W0, "RQ11 stuck 1.8 s -> w0 (%.3f)", last);
        // ---- v35 unit key. All lanes and apex vertices expire (keep 8 s, apex 1.5 s).
        Advance(8600);
        for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -4.0f + i); PutF(S[i], 0x50, -8.0f); }
        feed(8);                           // S0..S7 fresh at corner A; S8, S9 have no call of their own
        PutF(S[8], 0x48, 0.5f); PutF(S[8], 0x50, -8.0f);    // on the queue line (x 0.5), slot beyond the corner (z 5)
        w = rad(S[8]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ13 unit key: no own apex, 8 men ahead at A -> lane 1 (q 5/8), w0 + 1.5 (%.3f)", w);
        Advance(2000);
        feed(8);
        w = rad(S[8]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ14 unit key: lane kept 2 s later with no call of his own (%.3f)", w);
        PutF(S[9], 0x48, 1.0f); PutF(S[9], 0x50, 6.0f); PutF(S[9], 0x428, 20.0f);   // past the corner, slot far ahead
        w = rad(S[9]);
        CHECK(w == W0, "RQ15 unit key: the corner is behind him -> no lane (%.3f)", w);
        PutF(S[9], 0x428, 5.0f);
        w = rad(S[3]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ16 own key still: S3 (q 3/7) -> lane 1 (%.3f)", w);
        // RC1 (v36): lead the turn is active in RC (S1 inside the 6 m zone, as LD2)
        {
            float ex, ez;
            double sg = 0;
            PutF(S[1], 0x48, -2.0f); PutF(S[1], 0x50, -1.0f);
            int st = ExpectLead(S, NS, 1, 0, 0, r, zl, 1, -1, L, 0, &ex, &ez, &sg);
            CHECK(st == 0, "RC1 expectation computed (status %d, sign %+.0f)", st, sg);
            Apex("RC1 RC: lead point adjusted as in C", S[1], pA, r, zl, ex, ez);
            PutF(S[1], 0x48, -3.0f); PutF(S[1], 0x50, -8.0f);
        }
        // NF1 (kept from v31): R switches the crowd damping off within 30 m of the vertex
        for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -1.5f + i); PutF(S[i], 0x50, -20.0f); }
        uint8_t* me = S[6];
        PutF(me, 0x48, -13.0f); PutF(me, 0x50, -2.0f); PutF(me, 0xD8, 0.55f); PutF(me, 0xE0, 0.0f);
        Feed(me, pA, r, zl);
        NoDamp("NF1 RC: 13 m from the vertex -> damping off", me, 0);
        for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -1.5f + i); PutF(S[i], 0x50, -20.0f); }
        g_wiggle = true;
    }

        printf("release beyond the choke (verbatim from the full run):\n");
    uint8_t* GN[10];
    {
        // path: nodes 0..2 wide (36 m, 20 m long: 0..60), 3..6 narrow (5.5 m, 8 m: 60..92), 7..9 wide (92..152)
        const float gl[10] = { 20, 20, 20, 8, 8, 8, 8, 20, 20, 20 };
        const float gw[10] = { 36, 36, 36, 5.5f, 5.5f, 5.5f, 5.5f, 36, 36, 36 };
        MakeNodes(GN, 10, gl, gw);
        // NS = 10 men at 36 m: floor(36 / 1.4) = 25 files -> 1 row -> depth 0.85 m; A = 92 + 0.85 + 3 = 95.85
        float cur[2];
        auto setcur = [&](uint8_t* node, float t) { uint32_t u = (uint32_t)(uintptr_t)node; memcpy(cur, &u, 4); cur[1] = t; };
        auto plan = [&](uint8_t* node, float t) { PutU(C, 0x34, (uint32_t)(uintptr_t)node); PutF(C, 0x38, t); return CallOrders(C); };
        int f1 = plan(GN[2], 0.5f);
        setcur(GN[2], 0.5f);
        int f2 = CallAdvance(cur, 11.0f);
        CHECK(f1 == 0 && f2 == 0 && GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[7] && fabsf(cur[1] - 0.1925f) < 0.01f,
              "G1 cursor 50 m + D 11 inside the run [60, 92) -> advanced to 95.85 m = node 7 t 0.1925 (node %s t %.4f)",
              GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[7] ? "7" : "other", cur[1]);
        // G9 (v39): the hold. Cursor 7.2 m into node 5 (the run from here is [0, 16)): D 11 -> P 18.2 on wide
        // ground, 57.8 m from the path end, BEHIND the anchor of G1 (56.15 m from the end) -> held there: node 7 t 0.1925
        plan(GN[5], 0.9f);
        setcur(GN[5], 0.9f);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[7] && fabsf(cur[1] - 0.1925f) < 0.01f,
              "G9 vanilla point behind the previous anchor -> held at the anchor (node %s t %.4f)",
              GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[7] ? "7" : "other", cur[1]);
        // G10: cursor 7.2 m into node 6: P 18.2 is 49.8 m from the end, past the anchor -> vanilla (node 7 t 0.51), hold ends
        plan(GN[6], 0.9f);
        setcur(GN[6], 0.9f);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[7] && fabsf(cur[1] - 0.51f) < 1e-3f,
              "G10 vanilla point past the anchor -> vanilla, the hold ends (node 7 t %.4f)", cur[1]);
        plan(GN[0], 0.0f);
        setcur(GN[0], 0.0f);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[0] && fabsf(cur[1] - 0.55f) < 1e-3f,
              "G2 cursor 0 + D 11 on wide ground -> untouched (node 0 t %.4f)", cur[1]);
        setcur(GN[2], 0.5f);                      // the plan of G2 is consumed: no plan for this call
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[3] && fabsf(cur[1] - 0.125f) < 1e-3f,
              "G3 no plan pending -> vanilla advance (node 3 t %.4f)", cur[1]);
        plan(GN[2], 0.5f);
        setcur(GN[1], 0.0f);                      // planned for node 2, called with node 1
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[1] && fabsf(cur[1] - 0.55f) < 1e-3f,
              "G4 cursor differs from the plan -> vanilla (node 1 t %.4f)", cur[1]);
        plan(GN[2], 0.5f);
        setcur(GN[2], 0.5f);
        CallAdvanceDirect(cur, 11.0f);            // not from 0x1BC983
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[3] && fabsf(cur[1] - 0.125f) < 1e-3f,
              "G5 another call site -> vanilla (node 3 t %.4f)", cur[1]);
        // G6: the narrow run reaches the path end: nodes 0..1 wide (0..40), 2..4 narrow (40..64, end)
        uint8_t* M[5];
        const float ml[5] = { 20, 20, 8, 8, 8 };
        const float mw[5] = { 36, 36, 5.5f, 5.5f, 5.5f };
        MakeNodes(M, 5, ml, mw);
        plan(M[1], 0.5f);                         // cursor 30, P 41 in [40, 64), A 67.85 > 64 - 2 -> 62 (last node t 0.75)
        setcur(M[1], 0.5f);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)M[4] && fabsf(cur[1] - 0.75f) < 1e-3f,
              "G6 run at the path end -> anchor 2 m before the end, last node t %.3f (end never reached)", cur[1]);
        // G7: no node wider than 10 m -> no runs -> vanilla
        uint8_t* K[3];
        const float kl[3] = { 20, 20, 20 };
        const float kw[3] = { 8, 5, 8 };
        MakeNodes(K, 3, kl, kw);
        plan(K[0], 0.5f);
        setcur(K[0], 0.5f);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)K[1] && fabsf(cur[1] - 0.05f) < 1e-3f,
              "G7 widest node 8 m -> no choke by the rule -> vanilla (node 1 t %.4f)", cur[1]);
        PutU(C, 0x34, 0); PutF(C, 0x38, 0.0f);
    }
        printf("park release (v42, as in the full run):\n");
        ParkSection(C, S[2]);

        printf("frame self-check:\n");
        g_eaxDelta = 0x20;
        {
            uint32_t jnz = 0;
            RunPatch(S[8], pA.P, r, 2.5f, zl, &jnz);
        }
        g_eaxDelta = 0;
        Sleep(300);
        g_wiggle = false;
        PutF(S[1], 0x48, -2.0f); PutF(S[1], 0x50, -1.0f);
        Apex("RD1 after the frame failure: a lead-zone apex is untouched", S[1], pA, r, zl, r, zl);
        {
            uint8_t* N[10];
            const float nl[10] = { 20, 20, 20, 8, 8, 8, 8, 20, 20, 20 };
            const float nw[10] = { 36, 36, 36, 5.5f, 5.5f, 5.5f, 5.5f, 36, 36, 36 };
            MakeNodes(N, 10, nl, nw);
            float cur[2];
            uint32_t u = (uint32_t)(uintptr_t)N[2];
            memcpy(cur, &u, 4); cur[1] = 0.5f;
            PutU(C, 0x34, u); PutF(C, 0x38, 0.5f);
            CallOrders(C);
            CallAdvance(cur, 11.0f);
            CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)N[3] && fabsf(cur[1] - 0.125f) < 1e-3f,
                  "RD2 after the frame failure: vanilla advance (node 3 t %.4f)", cur[1]);
            PutU(C, 0x34, 0); PutF(C, 0x38, 0.0f);
        }
        {
            float w = RunRadius(S[4], 1.2f, GetF(S[4], 0x420), GetF(S[4], 0x428));
            CHECK(w == 1.2f, "RD3 after the frame failure: radius unchanged (%.3f)", w);
            uint8_t* me = S[4];
            PutF(me, 0x48, -2.0f); PutF(me, 0x50, -2.0f); PutF(me, 0xD8, 0.5f); PutF(me, 0xE0, 0.2f);
            NoDamp("RD4 after the frame failure: damping kept", me, 1);
        }
        FreeLibrary(proxy);
        CHECK(memcmp(g_base + 0x3DF6C0, "\x80\xBB\x71\x04\x00\x00\x01", 7) == 0, "RU1 waypoint patch site restored");
        CHECK(memcmp(g_base + 0x367600, "\x55\x8B\xEC\x83\xE4\xF0", 6) == 0, "RU2 integrator entry restored");
        CHECK(memcmp(g_base + 0x3670A0, "\x83\xEC\x10\x56\x57", 5) == 0, "RU3 advance entry restored");
        CHECK(memcmp(g_base + 0x1BC880, "\x81\xEC\xC8\x00\x00\x00", 6) == 0, "RU4 orders entry restored");
        CHECK(memcmp(g_base + 0x3DF4D1, "\x80\x7C\x24\x0F\x01", 5) == 0, "RU5 radius patch site restored");
        if (!devRef) {
            char p[MAX_PATH + 32];
            sprintf(p, "%spathfix_log.txt", exeDir);
            CHECK(GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES, "RF1 release: no log file");
            sprintf(p, "%spathfix_runs", exeDir);
            CHECK(GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES, "RF2 release: no run folder");
            sprintf(p, "%s*.csv", exeDir);
            WIN32_FIND_DATAA fd;
            HANDLE fh = FindFirstFileA(p, &fd);
            CHECK(fh == INVALID_HANDLE_VALUE, "RF3 release: no CSV file");
            if (fh != INVALID_HANDLE_VALUE) FindClose(fh);
            sprintf(p, "%scrash.dmp", exeDir);
            CHECK(GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES, "RF4 release: no crash dump");
        }
        printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 4 : 0;
    }

    // ---- off (launch): nothing changes
    printf("fix off:\n");
    Apex("O1 apex A, off", S[3], pA, r, zl, r, zl);
    {   // goal call: portal 0
        uint32_t jnz = 0;
        memset(S[4] + 0x454, 0xCC, 0x14);
        RunPatch(S[4], nullptr, 33.0f, 2.5f, 44.0f, &jnz);
        CHECK(Near(GetF(S[4], 0x454), 33.0f) && Near(GetF(S[4], 0x45C), 44.0f), "O2 goal call (portal 0) untouched");
    }
    {   // JNZ path with byte +0x471 == 1: the stolen CMP must set ZF
        uint32_t jnz = 0xFF;
        S[5][0x471] = 1;
        RunPatch(S[5], pA.P, r, 2.5f, zl, &jnz);
        CHECK(jnz == 0, "O3 [soldier+0x471] == 1 -> ZF set after the stolen CMP (stub saw %u, expect 0)", jnz);
        S[5][0x471] = 0;
    }

    // ---- variant C (v41: test export; was the first I press) = v23 lead the turn (corner A of pA: m = (1, -1), L = 8.9 m)
    Variant(1);
    printf("mode 1 (lead the turn):\n");
    g_wiggle = false;
    for (int i = 0; i < NS; i++) PutF(S[i], 0x50, -20.0f);
    {
        float ex, ez;
        double sg = 0;
        int st;
        // LD1 outside the 6 m zone: the game's point
        Apex("LD1 outside the zone -> game point", S[0], pA, r, zl, r, zl);
        // LD2 inside the zone, free point (S1 at (-2, -1); everyone else 20 m away)
        PutF(S[1], 0x48, -2.0f); PutF(S[1], 0x50, -1.0f);
        st = ExpectLead(S, NS, 1, 0, 0, r, zl, 1, -1, L, 0, &ex, &ez, &sg);
        CHECK(st == 0, "LD2 expectation computed (status %d, sign %+.0f)", st, sg);
        Apex("LD2 free lead point", S[1], pA, r, zl, ex, ez);
        // LD3 occupied: S3 stands on S2's lead point -> next radius
        Advance(3200);   // S1's stored sign no longer matters (other soldier anyway)
        PutF(S[2], 0x48, -1.0f); PutF(S[2], 0x50, -2.0f);
        {
            float fx, fz;
            double sg2 = 0;
            ExpectLead(S, NS, 2, 0, 0, r, zl, 1, -1, L, 0, &fx, &fz, &sg2);   // free point first
            {   // S3 0.1 m inside S2's free point (towards V): rho is occupied, rho + 0.7 clearly free
                double l = sqrt((double)fx * fx + (double)fz * fz);
                PutF(S[3], 0x48, (float)(fx - 0.1 * fx / l)); PutF(S[3], 0x50, (float)(fz - 0.1 * fz / l));
            }
            st = ExpectLead(S, NS, 2, 0, 0, r, zl, 1, -1, L, 0, &ex, &ez, &sg2);
            CHECK(st == 0 && (fabs(ex - fx) > 0.3 || fabs(ez - fz) > 0.3), "LD3 expectation moved off the occupied point");
            Apex("LD3 occupied point -> next free radius", S[2], pA, r, zl, ex, ez);
            PutF(S[3], 0x48, -1.5f + 3); PutF(S[3], 0x50, -20.0f);
        }
        // LD4 stored sign: S4 first at (-1, -2) (sign from the geometry), then
        // moved past the bisector within 3 s: same sign, lead clamped to the edge
        PutF(S[4], 0x48, -1.0f); PutF(S[4], 0x50, -2.0f);
        double s4 = 0;
        ExpectLead(S, NS, 4, 0, 0, r, zl, 1, -1, L, 0, &ex, &ez, &s4);
        Apex("LD4a first call, sign from the geometry", S[4], pA, r, zl, ex, ez);
        PutF(S[4], 0x48, 2.0f); PutF(S[4], 0x50, -0.5f);
        double s4b = 0;
        st = ExpectLead(S, NS, 4, 0, 0, r, zl, 1, -1, L, s4, &ex, &ez, &s4b);
        CHECK(st == 0 && s4 > 0, "LD4 expectation: stored sign %+.0f kept", s4);
        Apex("LD4b past the bisector, stored sign, clamped to the sector edge", S[4], pA, r, zl, ex, ez);
        // LD5 beyond the far edge (same corner, stored sign): the game's point
        PutF(S[4], 0x48, -0.2f); PutF(S[4], 0x50, 2.0f);
        st = ExpectLead(S, NS, 4, 0, 0, r, zl, 1, -1, L, s4, &ex, &ez, &s4b);
        CHECK(st == 2, "LD5 expectation: beyond the far edge (status %d)", st);
        Apex("LD5 beyond the far edge -> game point", S[4], pA, r, zl, r, zl);
        // LD6 short lane (B at (3, 0): L = 1.9): radius cut to |P - V| + 1.9
        Portal pShort = MakePortal(0, 0, 1, -1, 3.0f, 0, -1, -1, 0, 0);
        PutF(S[5], 0x48, -4.5f); PutF(S[5], 0x50, -2.5f);
        double Ls = 3.0 - 2 * r;
        st = ExpectLead(S, NS, 5, 0, 0, r, zl, 1, -1, Ls, 0, &ex, &ez, &sg);
        CHECK(st == 0, "LD6 expectation computed");
        Apex("LD6 short lane: radius cut", S[5], pShort, r, zl, ex, ez);
        for (int i = 1; i < 6; i++) { PutF(S[i], 0x48, -1.5f + i); PutF(S[i], 0x50, -20.0f); }
    }
    Apex("M1 lone soldier (no unit) untouched", Lone, pA, r, zl, r, zl);
    Apex("M1 special link untouched", S[1], pLink, r, zl, r, zl);
    Advance(1600);
    Apex("M1 type 2 gate, apex pA, outside the zone -> game point", S[3], pGate, r, zl, r, zl);
    Advance(1600);
    Apex("M1 type 2 gate, apex pB, outside the zone -> game point", S[0], pGate, 10 - r, zl, 10 - r, zl);
    Advance(1600);
    Apex("M1 type 7 edge, left (r, 0), outside the zone -> game point", S[6], pEdge, r, 0, r, 0);
    Apex("M1 narrow (candidates crossed) untouched", S[2], pNarrow, r, zl, r, zl);
    Advance(1600);
    Apex("M1 other end |mB| 0 pulled in, outside the zone -> game point", S[9], pOther, r, zl, r, zl);
    Apex("M1 vertex mismatch untouched", S[3], pA, 3.0f, zl, 3.0f, zl);
    Apex("M1 v17 candidate A + d*r (on the edge) is a mismatch now", S[3], pA, r, 0, r, 0);
    {   // fault: portal pointer to a freed page
        uint8_t* gone = Alloc(0x1000);
        VirtualFree(gone, 0, MEM_RELEASE);
        uint32_t jnz = 0;
        memset(S[6] + 0x454, 0xCC, 0x14);
        int f = RunPatch(S[6], gone, r, 2.5f, zl, &jnz);
        CHECK(!f && Near(GetF(S[6], 0x454), r) && Near(GetF(S[6], 0x45C), zl), "M1 unreadable portal: no crash, untouched");
        Sleep(120);
    }
    g_wiggle = true;

    // ---- F7: lane mode 2 (own position), the fix stays on
    Press(1);
    printf("mode 2 (own position):\n");
    PutF(S[0], 0x48, 2.0f);
    Apex("M2 pos x 2 -> apex pA s 1.45 (x 2)", S[0], pA, r, zl, 2.0f, zl);
    PutF(S[1], 0x48, -5.0f);
    Apex("M2 pos x -5 -> clamp to s 0", S[1], pA, r, zl, r, zl);
    PutF(S[2], 0x48, 20.0f);
    Apex("M2 pos x 20 -> clamp to s L", S[2], pA, r, zl, 10 - r, zl);
    PutF(S[3], 0x48, 2.0f);
    Apex("M2 pos x 2 -> apex pB s 7.45 (x 2)", S[3], pA, 10 - r, zl, 2.0f, zl);
    printf("v21 guards in mode 2:\n");
    {
        g_wiggle = false;
        // turn limit: S9 at (r + 3, zl - 2): own s 3, a = (-3, 2), u = (1, 0):
        // smax = tan25 * 13 / (2 + tan25 * 3) = 1.78354
        PutF(S[9], 0x48, r + 3.0f); PutF(S[9], 0x50, zl - 2.0f);
        Apex("V1 turn limit: own s 3 -> 1.7835", S[9], pA, r, zl, r + 1.78354f, zl);
        // stuck release: S8 standing still at own s 2.4, calls every ~100 ms
        PutF(S[8], 0x48, r + 2.4f); PutF(S[8], 0x50, -20.0f);
        Advance(1200);
        Apex("V3 stuck: first call adjusted (s 2.4)", S[8], pA, r, zl, r + 2.4f, zl);
        for (int k = 0; k < 17; k++) {   // 17 x 360 game ms = 6.1 game s without moving (gaps 0.36 < 3.6 s)
            uint32_t jnz = 0;
            RunPatch(S[8], pA.P, r, 2.5f, zl, &jnz);
            Advance(100);
        }
        Apex("V4 stuck: after 1.5 s without moving -> game waypoint", S[8], pA, r, zl, r, zl);
        PutF(S[8], 0x50, -19.0f);
        Apex("V5 stuck: moved but still held -> game waypoint", S[8], pA, r, zl, r, zl);
        Advance(4300);
        Apex("V6 stuck: hold over -> adjusted again (s 2.4)", S[8], pA, r, zl, r + 2.4f, zl);
        g_wiggle = true;
        PutF(S[9], 0x48, -1.5f + 9); PutF(S[9], 0x50, -20.0f);
        PutF(S[8], 0x48, -1.5f + 8); PutF(S[8], 0x50, -20.0f);
    }

    // ---- v25 no crowd damping near corners (fix on, mode 2 here; runs in any mode)
    printf("v25 damping switch on the integrator:\n");
    {
        g_wiggle = false;
        for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -1.5f + i); PutF(S[i], 0x50, -20.0f); PutF(S[i], 0x58, 0.43f); }
        uint8_t* me = S[4];
        PutF(me, 0x48, -2.0f); PutF(me, 0x50, -2.0f);
        Feed(me, pA, r, zl);                          // apex pA: corner vertex A = (0, 0)
        PutF(me, 0xD8, 0.4781f); PutF(me, 0xDC, 0.0f); PutF(me, 0xE0, 0.2719f);
        PutF(S[5], 0x48, -1.12f); PutF(S[5], 0x50, -1.5f);   // a man 1 m ahead: no avoidance in v25
        NoDamp("ND0 corner man, man ahead -> D kept, damping off", me, 0);
        PutF(S[5], 0x48, 3.5f); PutF(S[5], 0x50, -20.0f);
        NoDamp("ND1 corner man, free ahead -> damping off", me, 0);
        PutF(me, 0xD8, 0.0f); PutF(me, 0xE0, 0.0f);
        NoDamp("ND2 corner man standing (D 0) -> damping off", me, 0);
        PutF(me, 0x48, -13.0f); PutF(me, 0x50, -2.0f); PutF(me, 0xD8, 0.55f); PutF(me, 0xE0, 0.0f);
        NoDamp("ND3 13 m from the vertex -> damping kept", me, 1);
        PutF(Lone, 0x48, -2.0f); PutF(Lone, 0x50, -2.0f); PutF(Lone, 0xD8, 0.5f); PutF(Lone, 0xE0, 0.2f);
        NoDamp("ND4 unlisted soldier -> damping kept", Lone, 1);
        PutF(me, 0x48, -2.0f); PutF(me, 0x50, -2.0f);
        Sleep(1700);   // wall time only: in game time the apex is still fresh (v36)
        NoDamp("GT3 1.7 wall s without game time -> apex still fresh, damping off", me, 0);
        Advance(1700);
        NoDamp("ND5 stale apex (1.7 s game-equivalent) -> damping kept", me, 1);
        for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -1.5f + i); PutF(S[i], 0x50, -20.0f); }
        g_wiggle = true;
    }

    // ---- v34: variant RC (v41: test export; was the second I press) (radius lanes)
    Variant(2);
    printf("variant R (radius lanes):\n");
    const float MX = 100.0f, MZ = 100.0f;
    {
        g_wiggle = false;
        g_order((uint32_t)(uintptr_t)U);   // a unit order note (T10)
        const float W0 = 1.2f;
        auto rad = [&](uint8_t* p) -> float { return RunRadius(p, W0, GetF(p, 0x420), GetF(p, 0x428)); };
        auto feed = [&](int nmen) { for (int i = 0; i < nmen; i++) { uint32_t j = 0; RunPatch(S[i], pA.P, r, 2.5f, zl, &j); } };
        float w;
        Advance(1600);                     // every stored apex stale
        for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -4.0f + i); PutF(S[i], 0x50, -8.0f); }
        // RQ0 the stolen CMP is replayed for the JZ
        g_cmpByte = 1; w = rad(S[4]);
        CHECK(g_cmpZ == 1, "RQ0a [ESP+0xF] == 1 -> ZF set after the replayed CMP (got %u)", g_cmpZ);
        g_cmpByte = 0; w = rad(S[4]);
        CHECK(g_cmpZ == 0, "RQ0b [ESP+0xF] == 0 -> ZF clear (got %u)", g_cmpZ);
        g_cmpByte = 1;
        CHECK(w == W0, "RQ1 no corner (no fresh apex) -> w unchanged (%.3f)", w);
        feed(7);
        w = rad(S[3]);
        CHECK(w == W0, "RQ2 queue of 7 (< 8) -> w unchanged (%.3f)", w);
        Apex("RQ3 RC: apex call outside the 6 m lead zone -> waypoint untouched", S[9], pA, r, zl, r, zl);
        feed(10);
        w = rad(S[0]);
        CHECK(fabsf(w - W0) < 1e-5f, "RQ4 queue of 10, innermost S0 -> lane 0, w0 (%.3f)", w);
        w = rad(S[4]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ5 S4 (q 4/9) -> lane 1, w0 + 1.5 (%.3f)", w);
        w = rad(S[9]);
        CHECK(fabsf(w - 4.2f) < 1e-5f, "RQ6 outermost S9 -> lane 2, w0 + 3.0 capped at 4.2 (%.3f)", w);
        rad(S[6]); rad(S[7]);
        w = rad(S[8]);
        CHECK(fabsf(w - 4.2f) < 1e-5f, "RQ7 S8 -> lane 2 (4 men there now, cap 1.3 * 10 / 3 = 4.33) (%.3f)", w);
        // S5 to the outer edge: rank 2, lane 2 holds 4 -> overfull -> adjacent lane 1 (lane 0 is two away)
        PutF(S[5], 0x48, 6.0f);
        feed(10);
        w = rad(S[5]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ8 overfull lane 2 -> adjacent lane 1, not lane 0 (%.3f)", w);
        w = RunRadius(S[4], W0, 999.0f, 999.0f);
        CHECK(w == W0, "RQ9 bad frame (request end != slot) -> w unchanged (%.3f)", w);
        w = rad(S[4]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ10 S4 again -> lane 1 kept (%.3f)", w);
        // stuck: S4 does not move while the queue stays fresh -> w0 after 1.5 s
        // GT4 (v36): wall time alone does not age the stuck clock
        Sleep(1700);
        feed(10);
        w = rad(S[4]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "GT4 1.7 wall s without game time -> not stuck, lane 1 kept (%.3f)", w);
        float last = 0.0f;
        for (int k = 0; k < 4; k++) { Advance(450); feed(10); last = rad(S[4]); }
        CHECK(last == W0, "RQ11 stuck 1.8 s -> w0 (%.3f)", last);
        // ---- v35 unit key. All lanes and apex vertices expire (keep 8 s, apex 1.5 s).
        Advance(8600);
        for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -4.0f + i); PutF(S[i], 0x50, -8.0f); }
        feed(8);                           // S0..S7 fresh at corner A; S8, S9 have no call of their own
        PutF(S[8], 0x48, 0.5f); PutF(S[8], 0x50, -8.0f);    // on the queue line (x 0.5), slot beyond the corner (z 5)
        w = rad(S[8]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ13 unit key: no own apex, 8 men ahead at A -> lane 1 (q 5/8), w0 + 1.5 (%.3f)", w);
        Advance(2000);
        feed(8);
        w = rad(S[8]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ14 unit key: lane kept 2 s later with no call of his own (%.3f)", w);
        PutF(S[9], 0x48, 1.0f); PutF(S[9], 0x50, 6.0f); PutF(S[9], 0x428, 20.0f);   // past the corner, slot far ahead
        w = rad(S[9]);
        CHECK(w == W0, "RQ15 unit key: the corner is behind him -> no lane (%.3f)", w);
        PutF(S[9], 0x428, 5.0f);
        w = rad(S[3]);
        CHECK(fabsf(w - (W0 + 1.5f)) < 1e-5f, "RQ16 own key still: S3 (q 3/7) -> lane 1 (%.3f)", w);
        // RC1 (v36): lead the turn is active in RC (S1 inside the 6 m zone, as LD2)
        {
            float ex, ez;
            double sg = 0;
            PutF(S[1], 0x48, -2.0f); PutF(S[1], 0x50, -1.0f);
            int st = ExpectLead(S, NS, 1, 0, 0, r, zl, 1, -1, L, 0, &ex, &ez, &sg);
            CHECK(st == 0, "RC1 expectation computed (status %d, sign %+.0f)", st, sg);
            Apex("RC1 RC: lead point adjusted as in C", S[1], pA, r, zl, ex, ez);
            PutF(S[1], 0x48, -3.0f); PutF(S[1], 0x50, -8.0f);
        }
        // NF1 (kept from v31): R switches the crowd damping off within 30 m of the vertex
        for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -1.5f + i); PutF(S[i], 0x50, -20.0f); }
        uint8_t* me = S[6];
        PutF(me, 0x48, -13.0f); PutF(me, 0x50, -2.0f); PutF(me, 0xD8, 0.55f); PutF(me, 0xE0, 0.0f);
        Feed(me, pA, r, zl);
        NoDamp("NF1 RC: 13 m from the vertex -> damping off", me, 0);
        for (int i = 0; i < NS; i++) { PutF(S[i], 0x48, -1.5f + i); PutF(S[i], 0x50, -20.0f); }
        g_wiggle = true;
    }

    // ---- v38: variant G (RC + release beyond the choke); v41: the real I key from off
    Variant(0);
    Press(0);
    printf("variant G (release beyond the choke):\n");
    uint8_t* GN[10];
    {
        // path: nodes 0..2 wide (36 m, 20 m long: 0..60), 3..6 narrow (5.5 m, 8 m: 60..92), 7..9 wide (92..152)
        const float gl[10] = { 20, 20, 20, 8, 8, 8, 8, 20, 20, 20 };
        const float gw[10] = { 36, 36, 36, 5.5f, 5.5f, 5.5f, 5.5f, 36, 36, 36 };
        MakeNodes(GN, 10, gl, gw);
        // NS = 10 men at 36 m: floor(36 / 1.4) = 25 files -> 1 row -> depth 0.85 m; A = 92 + 0.85 + 3 = 95.85
        float cur[2];
        auto setcur = [&](uint8_t* node, float t) { uint32_t u = (uint32_t)(uintptr_t)node; memcpy(cur, &u, 4); cur[1] = t; };
        auto plan = [&](uint8_t* node, float t) { PutU(C, 0x34, (uint32_t)(uintptr_t)node); PutF(C, 0x38, t); return CallOrders(C); };
        int f1 = plan(GN[2], 0.5f);
        setcur(GN[2], 0.5f);
        int f2 = CallAdvance(cur, 11.0f);
        CHECK(f1 == 0 && f2 == 0 && GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[7] && fabsf(cur[1] - 0.1925f) < 0.01f,
              "G1 cursor 50 m + D 11 inside the run [60, 92) -> advanced to 95.85 m = node 7 t 0.1925 (node %s t %.4f)",
              GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[7] ? "7" : "other", cur[1]);
        // G9 (v39): the hold. Cursor 7.2 m into node 5 (the run from here is [0, 16)): D 11 -> P 18.2 on wide
        // ground, 57.8 m from the path end, BEHIND the anchor of G1 (56.15 m from the end) -> held there: node 7 t 0.1925
        plan(GN[5], 0.9f);
        setcur(GN[5], 0.9f);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[7] && fabsf(cur[1] - 0.1925f) < 0.01f,
              "G9 vanilla point behind the previous anchor -> held at the anchor (node %s t %.4f)",
              GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[7] ? "7" : "other", cur[1]);
        // G10: cursor 7.2 m into node 6: P 18.2 is 49.8 m from the end, past the anchor -> vanilla (node 7 t 0.51), hold ends
        plan(GN[6], 0.9f);
        setcur(GN[6], 0.9f);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[7] && fabsf(cur[1] - 0.51f) < 1e-3f,
              "G10 vanilla point past the anchor -> vanilla, the hold ends (node 7 t %.4f)", cur[1]);
        plan(GN[0], 0.0f);
        setcur(GN[0], 0.0f);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[0] && fabsf(cur[1] - 0.55f) < 1e-3f,
              "G2 cursor 0 + D 11 on wide ground -> untouched (node 0 t %.4f)", cur[1]);
        setcur(GN[2], 0.5f);                      // the plan of G2 is consumed: no plan for this call
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[3] && fabsf(cur[1] - 0.125f) < 1e-3f,
              "G3 no plan pending -> vanilla advance (node 3 t %.4f)", cur[1]);
        plan(GN[2], 0.5f);
        setcur(GN[1], 0.0f);                      // planned for node 2, called with node 1
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[1] && fabsf(cur[1] - 0.55f) < 1e-3f,
              "G4 cursor differs from the plan -> vanilla (node 1 t %.4f)", cur[1]);
        plan(GN[2], 0.5f);
        setcur(GN[2], 0.5f);
        CallAdvanceDirect(cur, 11.0f);            // not from 0x1BC983
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[3] && fabsf(cur[1] - 0.125f) < 1e-3f,
              "G5 another call site -> vanilla (node 3 t %.4f)", cur[1]);
        // G6: the narrow run reaches the path end: nodes 0..1 wide (0..40), 2..4 narrow (40..64, end)
        uint8_t* M[5];
        const float ml[5] = { 20, 20, 8, 8, 8 };
        const float mw[5] = { 36, 36, 5.5f, 5.5f, 5.5f };
        MakeNodes(M, 5, ml, mw);
        plan(M[1], 0.5f);                         // cursor 30, P 41 in [40, 64), A 67.85 > 64 - 2 -> 62 (last node t 0.75)
        setcur(M[1], 0.5f);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)M[4] && fabsf(cur[1] - 0.75f) < 1e-3f,
              "G6 run at the path end -> anchor 2 m before the end, last node t %.3f (end never reached)", cur[1]);
        // G7: no node wider than 10 m -> no runs -> vanilla
        uint8_t* K[3];
        const float kl[3] = { 20, 20, 20 };
        const float kw[3] = { 8, 5, 8 };
        MakeNodes(K, 3, kl, kw);
        plan(K[0], 0.5f);
        setcur(K[0], 0.5f);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)K[1] && fabsf(cur[1] - 0.05f) < 1e-3f,
              "G7 widest node 8 m -> no choke by the rule -> vanilla (node 1 t %.4f)", cur[1]);
        PutU(C, 0x34, 0); PutF(C, 0x38, 0.0f);
    }
    printf("park release (v42):\n");
    ParkSection(C, S[2]);

    // ---- I: off (cycle G -> off); F7 while off selects mode 1 without switching on
    Press(0);
    printf("off again:\n");
    {   // G8: fix off -> the advance hook leaves the distance alone
        float cur[2];
        uint32_t u = (uint32_t)(uintptr_t)GN[2];
        memcpy(cur, &u, 4); cur[1] = 0.5f;
        PutU(C, 0x34, u); PutF(C, 0x38, 0.5f);
        CallOrders(C);
        CallAdvance(cur, 11.0f);
        CHECK(GetU((uint8_t*)cur, 0) == (uint32_t)(uintptr_t)GN[3] && fabsf(cur[1] - 0.125f) < 1e-3f,
              "G8 fix off -> vanilla advance (node 3 t %.4f)", cur[1]);
        PutU(C, 0x34, 0); PutF(C, 0x38, 0.0f);
    }
    Apex("O4 apex pA, off again", S[7], pA, r, zl, r, zl);
    {
        uint8_t* me = S[4];
        PutF(me, 0x48, -2.0f); PutF(me, 0x50, -2.0f);
        PutF(S[5], 0x48, -1.0f); PutF(S[5], 0x50, -1.7f);
        PutF(me, 0xD8, 0.5f); PutF(me, 0xE0, 0.2f);
        NoDamp("ND6 fix off -> damping kept", me, 1);
        PutF(me, 0x48, -1.5f + 4); PutF(me, 0x50, -20.0f);
        PutF(S[5], 0x48, -1.5f + 5); PutF(S[5], 0x50, -20.0f);
    }
    {   // v33 AV12: fix off -> no avoidance
        uint8_t* me = S[0];
        PutF(me, 0x48, MX); PutF(me, 0x50, MZ); PutF(me, 0x420, MX + 20); PutF(me, 0x428, MZ);
        PutF(S[5], 0x48, MX + 2); PutF(S[5], 0x50, MZ);
        Follow(me, MX + 20, MZ);
        PutF(me, 0xD8, 0.6f); PutF(me, 0xE0, 0.0f);
        int fault = CallInteg(me);
        CheckAv("AV12 fix off -> D kept", fault, me, 0);
        float wOff = RunRadius(S[1], 1.2f, GetF(S[1], 0x420), GetF(S[1], 0x428));
        CHECK(wOff == 1.2f, "RQ12 fix off -> radius unchanged (%.3f)", wOff);
        PutF(me, 0x48, -1.5f); PutF(me, 0x50, -20.0f);
        PutF(S[5], 0x48, -1.5f + 5); PutF(S[5], 0x50, -20.0f);
    }
    Press(1);
    Apex("O5 F7 while off: still untouched", S[7], pA, r, zl, r, zl);

    // ---- other hooks still record; flush after 10 s
    Sleep(9000);
    printf("files before the frame test:\n");
    FileSize(runDir, "pathfix_capture_v42_fix.csv");
    FileSize(runDir, "pathfix_capture_v42_fixraw.csv");
    FileSize(runDir, "pathfix_capture_v42_fix_totals.csv");
    FileSize(runDir, "pathfix_capture_v42_soldiers.csv");
    FileSize(runDir, "pathfix_capture_v42_events.csv");

    // ---- frame self-check: EAX != ESP+0x44 must disable the fix for good
    Press(0);   // v41: I -> G
    Apex("F1 G on again: outside the zone -> game point", S[9], pA, r, zl, r, zl);
    g_eaxDelta = 0x20;
    {
        uint32_t jnz = 0;
        RunPatch(S[8], pA.P, r, 2.5f, zl, &jnz);
    }
    g_eaxDelta = 0;
    Sleep(300);
    Press(0);   // must be refused (log, alarm), mode stays 0
    Apex("D1 after FIX DISABLED: untouched", S[9], pA, r, zl, r, zl);
    Press(2);   // O (marker)

    FreeLibrary(proxy);   // final flush, remove handlers, restore patches
    printf("after unload: 0x3DF6C0 %02X %02X %02X (80 BB 71 = restored), 0x1BC880 %02X %02X\n",
           g_base[0x3DF6C0], g_base[0x3DF6C1], g_base[0x3DF6C2], g_base[0x1BC880], g_base[0x1BC881]);
    CHECK(memcmp(g_base + 0x3DF6C0, "\x80\xBB\x71\x04\x00\x00\x01", 7) == 0, "U1 patch site restored");
    CHECK(memcmp(g_base + 0x367600, "\x55\x8B\xEC\x83\xE4\xF0", 6) == 0, "U2 integrator entry restored");
    CHECK(memcmp(g_base + 0x3670A0, "\x83\xEC\x10\x56\x57", 5) == 0, "U3 advance entry restored");
    printf("files after unload:\n");
    FileSize(runDir, "pathfix_capture_v42_fix.csv");
    FileSize(runDir, "pathfix_capture_v42_fixraw.csv");
    FileSize(runDir, "pathfix_capture_v42_fix_totals.csv");
    {
        static char text[1 << 20];
        char path[MAX_PATH + 48];
        sprintf(path, "%spathfix_log.txt", runDir);
        ReadText(path, text, sizeof(text));

        CHECK(strstr(text, "TEST variant C: mode 4: lead the turn + no crowd damping within 30 m") != nullptr,
              "L1 log: test export -> variant C (mode 4)");
        CHECK(strstr(text, "TEST variant RC: mode 9: radius lanes keyed to the unit's busy corner") != nullptr,
              "L13 log: test export -> variant RC (mode 9)");
        CHECK(strstr(text, "I: path fix OFF (mode 0: off (waypoints untouched))") != nullptr,
              "L14 log: I -> off after G");
        {
            int ons = 0;
            for (const char* q = text; (q = strstr(q, "I: path fix ON, variant G (mode 10")) != nullptr; q++) ons++;
            CHECK(ons == 2, "L15 log: the real I switched G on twice (%d)", ons);
        }
        CHECK(strstr(text, "Capture:     ON (PATHFIX_CAPTURE=1)") != nullptr, "L16 log: capture mode on in the full run");
        CHECK(strstr(text, "Lane: lane mode 2 selected, fix ON, mode 2: own position") != nullptr, "L2 log: F7 mode 2 while on");
        CHECK(strstr(text, "I: path fix OFF (mode 0") != nullptr, "L3 log: I off");
        CHECK(strstr(text, "Lane: lane mode 1 selected (lead the turn") != nullptr, "L4 log: F7 while off");
        CHECK(strstr(text, "FIX DISABLED: frame: EAX != hook-point ESP + 0x44") != nullptr, "L5 log: FIX DISABLED (frame)");
        CHECK(strstr(text, "I refused: the corner fix is DISABLED") != nullptr, "L6 log: F6 refused after disable");
        CHECK(strstr(text, "self-checks: frame 1/") != nullptr, "L7 log: fix totals with one frame failure");
        CHECK(strstr(text, "PATHFIX BUILD v42") != nullptr, "L8 log: v42 banner");
        CHECK(strstr(text, "v36 game clock: object 0x") != nullptr, "GT6 log: clock object noted at the first FUN_101bc880 hit");
        CHECK(strstr(text, "Endpoints:   FUN_10393000 PASS, FUN_103a0080 PASS") != nullptr, "L12 log: endpoint helpers byte-checked");
        CHECK(strstr(text, "Sound check: one low tone") != nullptr, "L9 log: sound check at window open");
        CHECK(strstr(text, "PlaySound failed") == nullptr, "L10 log: no PlaySound failure");
        CHECK(strstr(text, "O: marker 1") != nullptr, "L11 log: marker under its letter key");

        sprintf(path, "%spathfix_capture_v42_fix_totals.csv", runDir);
        ReadText(path, text, sizeof(text));
        printf("  totals: adjusted %ld, no_helpers %ld, other_pulled %ld, narrow %ld, geometry %ld, mismatch %ld, "
               "geo checked %ld failed %ld, match checked %ld failed %ld, raw %ld, fix-hook samples %ld, "
               "cross checked %ld failed %ld\n",
               TotalsValue(text, "status_adjusted"), TotalsValue(text, "status_no_helpers"),
               TotalsValue(text, "other_pulled"), TotalsValue(text, "status_narrow"),
               TotalsValue(text, "status_geometry"), TotalsValue(text, "status_vertex_mismatch"),
               TotalsValue(text, "geometry_checked"), TotalsValue(text, "geometry_failures"),
               TotalsValue(text, "apex_match_checked"), TotalsValue(text, "apex_match_failures"),
               TotalsValue(text, "raw_dumps"), TotalsValue(text, "samples_from_fix_hook"),
               TotalsValue(text, "cross_checked"), TotalsValue(text, "cross_failures"));
        CHECK(TotalsValue(text, "status_no_helpers") == 0 && TotalsValue(text, "other_pulled") >= 1 &&
              TotalsValue(text, "status_narrow") == 1 && TotalsValue(text, "status_geometry") == 0 &&
              TotalsValue(text, "status_vertex_mismatch") == 2 && TotalsValue(text, "geometry_failures") == 0 &&
              TotalsValue(text, "cross_failures") == 0 && TotalsValue(text, "cross_checked") > 0 &&
              TotalsValue(text, "disabled") == 1, "T1 totals: helpers used, narrow 1, mismatch 2, cross-check clean, disabled");
        printf("  totals v21: turn_capped %ld, stuck_releases %ld, status_stuck_release %ld\n",
               TotalsValue(text, "turn_capped"), TotalsValue(text, "stuck_releases"),
               TotalsValue(text, "status_stuck_release"));
        CHECK(TotalsValue(text, "turn_capped") == 1, "T5 v21: exactly one turn-limited call (V1)");
        printf("  totals v23: lead_calls %ld, sector_clamp %ld, no_free %ld, radius_moved %ld, radius_cut %ld, outside_zone %ld, past_sector %ld\n",
               TotalsValue(text, "lead_calls"), TotalsValue(text, "lead_sector_clamp"), TotalsValue(text, "lead_no_free"),
               TotalsValue(text, "lead_radius_moved"), TotalsValue(text, "lead_radius_cut"),
               TotalsValue(text, "status_outside_zone"), TotalsValue(text, "status_past_sector"));
        CHECK(TotalsValue(text, "lead_calls") == 6 && TotalsValue(text, "lead_sector_clamp") == 1 &&
              TotalsValue(text, "lead_radius_moved") == 1 && TotalsValue(text, "lead_radius_cut") == 1 &&
              TotalsValue(text, "lead_no_free") == 0 && TotalsValue(text, "status_past_sector") == 1 &&
              TotalsValue(text, "status_outside_zone") >= 6,
              "T7 v23 + v36: 6 lead calls (5 in C, RC1 in RC), 1 sector clamp, 1 radius moved, 1 radius cut, 1 past sector, >= 6 outside zone");
        CHECK(TotalsValue(text, "stuck_releases") == 11 && TotalsValue(text, "status_stuck_release") >= 3,
              "T6 v21 + v36: 11 stuck releases (V-section S8; RQ11 all ten standing men, since RC runs the stuck clock on every apex call like C), held calls returned the game waypoint");
        printf("  totals v35: key_own %ld, key_unit %ld, passed %ld, candidates %ld\n", TotalsValue(text, "radius_key_own"),
               TotalsValue(text, "radius_key_unit"), TotalsValue(text, "radius_passed"), TotalsValue(text, "radius_candidates"));
        CHECK(TotalsValue(text, "radius_key_unit") >= 2 && TotalsValue(text, "radius_key_own") >= 8 &&
              TotalsValue(text, "radius_candidates") >= 10,
              "T14 v35: unit-key lanes (RQ13, RQ14), own-key lanes, candidates counted");
        printf("  totals v36: gt_notes %ld, gt_reads %ld, gt_read_fail %ld, gt_fallback %ld, gt_object_changes %ld, gt_last_tick %ld\n",
               TotalsValue(text, "gt_notes"), TotalsValue(text, "gt_reads"), TotalsValue(text, "gt_read_fail"),
               TotalsValue(text, "gt_fallback"), TotalsValue(text, "gt_object_changes"), TotalsValue(text, "gt_last_tick"));
        CHECK(TotalsValue(text, "gt_notes") >= 2 && TotalsValue(text, "gt_reads") > 100 && TotalsValue(text, "gt_read_fail") == 0 &&
              TotalsValue(text, "gt_fallback") == 0 && TotalsValue(text, "gt_object_changes") == 0 &&
              TotalsValue(text, "gt_last_tick") == (long)(GT_TICK0 + (uint32_t)g_gtTicks),
              "GT5 totals: clock noted by both controllers, read on every fix call, no wall-clock fallback, one object, last tick %u",
              GT_TICK0 + (uint32_t)g_gtTicks);
        printf("  totals v27: steer_hits %ld, nodamp %ld, tracked %ld, turned %ld, no_apex %ld, far %ld, faults %ld\n",
               TotalsValue(text, "steer_hits"), TotalsValue(text, "steer_nodamp"), TotalsValue(text, "steer_tracked"),
               TotalsValue(text, "steer_turned"), TotalsValue(text, "steer_no_apex"), TotalsValue(text, "steer_far"),
               TotalsValue(text, "steer_faults"));
        CHECK(TotalsValue(text, "steer_hits") == 8 && TotalsValue(text, "steer_nodamp") == 5 &&
              TotalsValue(text, "steer_tracked") == 0 && TotalsValue(text, "steer_turned") == 0 &&
              TotalsValue(text, "steer_far") == 1 && TotalsValue(text, "steer_no_apex") == 1 &&
              TotalsValue(text, "steer_faults") == 0,
              "T8 v36: 8 hits while on, 5 damping-off (3 in C, GT3, 1 in RC), 1 far, 1 stale apex, no faults");
        printf("  totals gather: adjusted %ld, passed %ld, slot_ahead %ld, no_queue %ld, no_order %ld, far %ld, read_fail %ld, stuck %ld, order notes %ld\n",
               TotalsValue(text, "gather_adjusted"), TotalsValue(text, "gather_passed"), TotalsValue(text, "gather_slot_ahead"),
               TotalsValue(text, "gather_no_queue"), TotalsValue(text, "gather_no_order"), TotalsValue(text, "gather_far"),
               TotalsValue(text, "gather_read_fail"), TotalsValue(text, "gather_stuck_release"), TotalsValue(text, "unit_order_notes"));
        CHECK(TotalsValue(text, "gather_adjusted") == 0 && TotalsValue(text, "unit_order_notes") >= 1,
              "T10 v34: no gather in R (gather is D only)");
        printf("  totals radius: calls %ld, active %ld, frame checked %ld failed %ld bad %ld, no corner %ld, no queue %ld, "
               "stuck %ld, balanced %ld, kept %ld, raised %ld, lanes %ld / %ld / %ld, rows %ld, disabled %ld\n",
               TotalsValue(text, "radius_calls"), TotalsValue(text, "radius_active"),
               TotalsValue(text, "radius_frame_checked"), TotalsValue(text, "radius_frame_failures"),
               TotalsValue(text, "radius_frame_bad"), TotalsValue(text, "radius_no_corner"),
               TotalsValue(text, "radius_no_queue"), TotalsValue(text, "radius_stuck"),
               TotalsValue(text, "radius_balanced"), TotalsValue(text, "radius_kept"), TotalsValue(text, "radius_raised"),
               TotalsValue(text, "radius_lane0"), TotalsValue(text, "radius_lane1"), TotalsValue(text, "radius_lane2"),
               TotalsValue(text, "radius_rows"), TotalsValue(text, "disabled"));
        CHECK(TotalsValue(text, "radius_frame_bad") == 1 && TotalsValue(text, "radius_frame_failures") == 1 &&
              TotalsValue(text, "radius_no_corner") >= 3 && TotalsValue(text, "radius_no_queue") >= 1 &&
              TotalsValue(text, "radius_stuck") >= 1 && TotalsValue(text, "radius_balanced") == 1 &&
              TotalsValue(text, "radius_kept") >= 1 && TotalsValue(text, "radius_lane0") >= 1 &&
              TotalsValue(text, "radius_lane1") >= 3 && TotalsValue(text, "radius_lane2") >= 4 &&
              TotalsValue(text, "radius_rows") >= 5,
              "T12 v34 radius counters: one bad frame, no corner, small queue, stuck, one balance, lanes 0 / 1 / 2, rows");
        CHECK(TotalsValue(text, "raw_dumps") == 6, "T2 raw dump: 6 distinct side objects (pA, short, gate, edge, narrow, other end; the link portal has no E read)");
        CHECK(TotalsValue(text, "samples_from_fix_hook") > 0, "T3 soldier samples also triggered from the fix hook");

        sprintf(path, "%spathfix_capture_v42_fixraw.csv", runDir);
        ReadText(path, text, sizeof(text));
        int lines = 0;
        for (const char* p = text; *p; p++) if (*p == '\n') lines++;
        CHECK(lines == 7, "T4 fixraw.csv: header + 6 rows (got %d lines)", lines);
        // v37: speed-law columns in the soldier rows and leader / ready / speed mode in the order rows
        sprintf(path, "%spathfix_capture_v42_soldiers.csv", runDir);
        ReadText(path, text, sizeof(text));
        CHECK(strstr(text, ",s_corridor,s_radius,spd_max,spd_cur,spd_tgt,spd_cmd,spd_mul,ord_spd,ord_t,ord_mode,elapsed,moving,direct,f384,f308") != nullptr,
              "SP1 soldiers.csv header: speed-law columns");
        CHECK(strstr(text, ",3.500,1.250,7.500,1.200,0.9000,3.500,3.143,0,0.70,1,0,1,0") != nullptr,
              "SP1 soldiers.csv: a fake soldier's speed fields recorded (max 3.5, cur 1.25, tgt 7.5, cmd 1.2, mul 0.9, order 3.5 / 3.14 / mode 0, elapsed 0.7, moving, f384)");
        sprintf(path, "%spathfix_capture_v42_orders.csv", runDir);
        ReadText(path, text, sizeof(text));
        CHECK(strstr(text, ",listed,leader_idx,ready,speed_mode,soldiers") != nullptr, "SP2 orders.csv header: leader_idx, ready, speed_mode");
        CHECK(strstr(text, ",2,1,1,") != nullptr, "SP2 orders.csv: leader index 2, ready 1, speed mode 1 recorded");
        sprintf(path, "%spathfix_capture_v42_radius.csv", runDir);
        ReadText(path, text, sizeof(text));
        lines = 0;
        for (const char* p = text; *p; p++) if (*p == '\n') lines++;
        CHECK(lines >= 6 && strstr(text, ",assigned,") != nullptr && strstr(text, ",balanced,") != nullptr &&
              strstr(text, ",unit,") != nullptr && strstr(text, ",own,") != nullptr,
              "T13 radius.csv: assigned / balanced rows, own and unit keys (%d lines)", lines);
        // v38: release counters and rows
        sprintf(path, "%spathfix_capture_v42_fix_totals.csv", runDir);
        ReadText(path, text, sizeof(text));
        printf("  release: plans %ld (bad cursor %ld, walk fail %ld, no wide %ld, no runs %ld, with runs %ld), adv calls %ld, "
               "other site %ld, no plan %ld, mismatch %ld, vanilla %ld, applied %ld, to end %ld, rows %ld, held %ld, hold end %ld, reset %ld\n",
               TotalsValue(text, "release_plan_calls"), TotalsValue(text, "release_plan_bad_cursor"),
               TotalsValue(text, "release_plan_walk_fail"), TotalsValue(text, "release_plan_no_wide"),
               TotalsValue(text, "release_plan_no_runs"), TotalsValue(text, "release_plan_with_runs"),
               TotalsValue(text, "release_adv_calls"), TotalsValue(text, "release_adv_other_site"),
               TotalsValue(text, "release_adv_no_plan"), TotalsValue(text, "release_adv_mismatch"),
               TotalsValue(text, "release_adv_vanilla"), TotalsValue(text, "release_adv_applied"),
               TotalsValue(text, "release_adv_to_end"), TotalsValue(text, "release_rows"),
               TotalsValue(text, "release_adv_held"), TotalsValue(text, "release_adv_hold_end"), TotalsValue(text, "release_hold_reset"));
        // v42: the park section adds 8 plans (3 with runs), 2 applied, 6 vanilla-point calls, 2 hold ends, 8 rows
        CHECK(TotalsValue(text, "release_plan_calls") == 16 && TotalsValue(text, "release_plan_with_runs") == 10 &&
              TotalsValue(text, "release_plan_no_wide") == 1 && TotalsValue(text, "release_plan_walk_fail") == 0 &&
              TotalsValue(text, "release_adv_applied") == 4 && TotalsValue(text, "release_adv_to_end") == 1 &&
              TotalsValue(text, "release_adv_vanilla") == 10 && TotalsValue(text, "release_adv_no_plan") == 1 &&
              TotalsValue(text, "release_adv_mismatch") == 1 && TotalsValue(text, "release_adv_other_site") == 1 &&
              TotalsValue(text, "release_adv_held") == 1 && TotalsValue(text, "release_adv_hold_end") == 3 &&
              TotalsValue(text, "release_rows") == 14,
              "T14 release counters: 16 plans (10 with runs, 1 no wide node), 4 applied (1 to the end), 10 vanilla, 1 held, 3 hold ends, 1 no plan, 1 mismatch, 1 other site, 14 rows");
        CHECK(TotalsValue(text, "release_adv_park") == 1 && TotalsValue(text, "release_adv_park_latched") == 1,
              "T17 park counters: 1 park release, 1 latched call (park %ld, latched %ld)",
              TotalsValue(text, "release_adv_park"), TotalsValue(text, "release_adv_park_latched"));
        sprintf(path, "%spathfix_capture_v42_release.csv", runDir);
        ReadText(path, text, sizeof(text));
        CHECK(strstr(text, "seq,timestamp_ms,thread_id,ctrl,unit,n,node,t,cursor_arc,path_end,nodes,max_width,narrow_runs,run,run_start,run_end,depth,D,D_new,P,A,applied,to_end,reason") != nullptr,
              "T15 release.csv header");
        // arc lengths count from the start of the cursor node: cursor 10, path end 112, run 20..52, A 55.85
        CHECK(strstr(text, ",0.5000,10.00,112.00,8,36.00,1,0,20.00,52.00,0.85,11.000,45.850,21.00,55.85,1,0,past_run") != nullptr,
              "T15 release.csv: G1 row (cursor t 0.5 = 10 m, path end 112, 8 nodes, widest 36, run 20..52, depth 0.85, D 11 -> 45.85, P 21, A 55.85, applied)");
        CHECK(strstr(text, ",1,1,end_gap") != nullptr && strstr(text, ",0,0,vanilla") != nullptr,
              "T15 release.csv: an end_gap row and a vanilla row");
        // G9: cursor 7.2 m into node 5, path end 76 from there, P 18.2, held at A 19.85, D 11 -> 12.65
        CHECK(strstr(text, ",0.9000,7.20,76.00,5,36.00,1,-1,0.00,0.00,0.85,11.000,12.650,18.20,19.85,2,0,hold") != nullptr,
              "T16 release.csv: the G9 hold row (P 18.2 -> A 19.85, applied 2, reason hold)");
        // PR3: cursor 7 m into node 5, path end 20, D 11 -> 13.5, P 18, A 20.5, applied 3, to the end, reason park
        CHECK(strstr(text, ",0.3500,7.00,20.00,1,36.00,0,-1,0.00,0.00,0.85,11.000,13.500,18.00,20.50,3,1,park") != nullptr,
              "T18 release.csv: the PR3 park row (13 m left, D 11 -> 13.5, front 0.5 m past the end, reason park)");
    }
    printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 4 : 0;
}
