// collapsefix.cpp -- wall collapse fix for Total War: Shogun 2 (original x86 Empire.Retail.dll).
// Folder fixes/wall-collapse-fix/, code name collapsefix.
//
// Dev build. Fault: in a siege defence, wall pieces damaged in the campaign are shown destroyed
// during deployment and units can be deployed on them; now and then such a piece is destroyed
// again when the battle starts and the men standing on it die.
//
// Engine decode so far (docs/re/building_destruction_x86.md):
// - battle = [tick object +0xB4] (the phase machine, phase at +0x14): buildings at +0x118 (count
//   +0x114); a building's pieces at +0x1D0 (count +0x1CC).
// - A piece: template +0x18 (level count +0x6C, level models +0x70, each with a damage threshold at
//   +0x90), damage +0xEC (float), damage level +0x54, level model +0x64, destruct level +0x68,
//   variant +0x78, destroyed +0xF1, destroyed before the battle +0xF2, takes damage +0xF3.
// - FUN_10477f40 (ECX = piece, level, variant) applies a level model; FUN_10477ff0 sets the damage
//   and the level from it; FUN_1047b370 adds damage (projectiles); FUN_10479ca0 starts the collapse
//   effects of a destroyed piece; FUN_10426940 applies the campaign's fort damage at battle set-up.
//
// Cause (replay "broken wall bug"): the campaign can leave a castle's gates open to everyone
// (the flag at [tick object +0x5C] +0x9C; gate pieces are the pieces with byte +0x6C set, built
// open when the flag is set, and passable for every alliance). The battle start, run once on the
// first tick of phase 5 (FUN_101787b0, guarded by battle +0x18), then calls FUN_1043c870 for every
// building, which gives every gate piece one more damage than the threshold of its current level
// (FUN_1043c8b0 -> FUN_1047b370): an intact gate door collapses there, at Start Battle. Its
// gatehouse top is a deployable wall position; the men standing on it leave their unit the tick
// after (10 men in the replay). The deployment view showed the door intact, next to a gate tower
// already destroyed by the campaign damage, so nothing warned the player.
//
// The fix (on from launch; COLLAPSEFIX_OFF=1 starts off, I toggles; a build with the CMake option
// COLLAPSEFIX_START_OFF starts off unless COLLAPSEFIX_ON=1): an entry hook on FUN_1043c870 returns
// at once, so no gate is damaged at the battle start. The gates stay open and passable for every
// alliance for the whole battle (the game's own gate tick and passability test read the same
// flag), and every other way to damage a piece (artillery, rams, fire) is untouched.
//
// The research probe (COLLAPSEFIX_PROBE=1) writes <game>\collapsefix_runs\<stamp>\:
//   collapse_phase.csv   every battle phase change (FUN_101acfb0)
//   collapse_events.csv  every call of the piece functions above, with the piece state before /
//                        after, the caller and the module return addresses found on the stack
//   collapse_pieces.csv  every piece of every building at each phase change, and every change of a
//                        piece's state seen by the 50 ms sampler
//   collapse_men.csv     every unit's men count (sum of its groups) whenever it changes
//   collapse_deaths.csv  every soldier hit point set (FUN_103a3080) to 0 or less, and every set
//                        from deployment until 30 game seconds into the battle
//   collapse_near.csv    every soldier within 30 m of a piece that collapses in battle, at the
//                        collapse and 0.5 / 1 / 2 / 4 / 10 game seconds later (still in its group?)

#include "log.h"
#include "hook.h"
#include "gamemem.h"
#include "keys.h"
#include "stack_offsets_collapse.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint32_t g_base = 0;

// ---------------------------------------------------------------------------
// Hooks (stolen bytes: docs/re/prologues_collapsefix.txt; whole instructions, no branch into
// the range)
// ---------------------------------------------------------------------------

enum {
    H_SETTER = 0,  // FUN_10477f40: piece level model apply (ECX = piece, level, variant; RET 8); wrapped
    H_DMGSET,      // FUN_10477ff0: piece damage set (ECX = piece, int damage; RET 4); wrapped
    H_COLLAPSE,    // FUN_10479ca0: collapse effects of a destroyed piece (ECX = piece)
    H_DAMAGE,      // FUN_1047b370: damage added to a piece (ECX = piece, float; RET 4); wrapped
    H_FORT,        // FUN_10426940: campaign fort damage applied (ECX = holder, data; RET 4); wrapped
    H_PHASE,       // FUN_101acfb0: battle phase set (ECX = battle, new phase; RET 4)
    H_HP,          // FUN_103a3080: soldier hit points set (soldier, hp, flag; RET 0xC)
    H_GATES,       // FUN_1043c870: battle start, every gate piece of a building damaged (ECX = building); the fix
    HOOK_COUNT
};

static const HookDef g_hookDefs[HOOK_COUNT] = {
    { "FUN_10477f40", "piece level apply", 0x477F40, 5,
      { 0x83, 0xEC, 0x0C, 0x56, 0x57 }, -1, true },                        // SUB ESP,0xC; PUSH ESI; PUSH EDI
    { "FUN_10477ff0", "piece damage set", 0x477FF0, 5,
      { 0x8B, 0x44, 0x24, 0x04, 0x53 }, -1, true },                        // MOV EAX,[ESP+4]; PUSH EBX
    { "FUN_10479ca0", "piece collapse effects", 0x479CA0, 6,
      { 0x83, 0xEC, 0x14, 0x55, 0x8B, 0xE9 }, -1, true },                  // SUB ESP,0x14; PUSH EBP; MOV EBP,ECX
    { "FUN_1047b370", "piece damage added", 0x47B370, 7,
      { 0x80, 0xB9, 0xF4, 0x00, 0x00, 0x00, 0x00 }, -1, true },            // CMP byte [ECX+0xF4],0
    { "FUN_10426940", "fort damage from the campaign", 0x426940, 5,
      { 0x83, 0xEC, 0x10, 0x56, 0x57 }, -1, true },                        // SUB ESP,0x10; PUSH ESI; PUSH EDI
    { "FUN_101acfb0", "battle phase set", 0x1ACFB0, 7,
      { 0x8B, 0x44, 0x24, 0x04, 0x89, 0x41, 0x14 }, -1, true },            // MOV EAX,[ESP+4]; MOV [ECX+0x14],EAX
    { "FUN_103a3080", "soldier hit points set", 0x3A3080, 8,
      { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x44, 0x24, 0x08 }, -1, true },      // MOV ECX,[ESP+4]; MOV EAX,[ESP+8]
    // docs/re/prologues_collapsefix_fix.txt
    { "FUN_1043c870", "battle start gate damage (the fix)", 0x43C870, 10,
      { 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0xB7, 0xD0, 0x01, 0x00, 0x00 }, -1, false },   // PUSH ESI; PUSH EDI; MOV EDI,ECX; MOV ESI,[EDI+0x1D0]
};

static HookSlot g_slots[HOOK_COUNT] = {};
extern "C" uint8_t* g_collapseTrampolines[HOOK_COUNT] = {};

// ---------------------------------------------------------------------------
// Engine layout (see the header)
// ---------------------------------------------------------------------------

static const uint32_t TOBJ_OFF_BATTLE = 0xB4, BAT_OFF_PHASE = 0x14;
static const uint32_t BAT_OFF_NALLY = 0x88, BAT_OFF_ALLY = 0x8C, ALLY_OFF_NARMY = 0x20, ALLY_OFF_ARMY = 0x24;
static const uint32_t ARMY_OFF_NUNIT = 0x114, ARMY_OFF_UNIT = 0x118;
static const uint32_t BAT_OFF_NBLD = 0x114, BAT_OFF_BLD = 0x118, BLD_OFF_NPIECE = 0x1CC, BLD_OFF_PIECE = 0x1D0;
static const uint32_t PC_OFF_TMPL = 0x18, TMPL_OFF_NLEV = 0x6C, TMPL_OFF_LEV = 0x70, LEV_OFF_THRESH = 0x90;
static const uint32_t PC_OFF_DMGLEV = 0x54, PC_OFF_MODEL = 0x64, PC_OFF_LEVEL = 0x68, PC_OFF_VARIANT = 0x78;
static const uint32_t PC_OFF_POS = 0xB4, PC_OFF_DMG = 0xEC, PC_OFF_FLAGS = 0xF0, PC_OFF_WALL = 0x1AD;
static const uint32_t PC_OFF_GATE = 0x6C, PC_OFF_OWNER = 0x0C, PC_OFF_DESTROYED = 0xF1;
static const uint32_t OWNER_OFF_TOBJ = 0x08, TOBJ_OFF_SETTINGS = 0x5C, SET_OFF_GATES_OPEN = 0x9C;
static const uint32_t U_OFF_GA_N = 0xB8C, U_OFF_GA = 0xB90, U_OFF_GB_N = 0xB70, U_OFF_GB = 0xB74, U_OFF_MAIN = 0xB9C;
static const uint32_t GRP_OFF_COUNT = 0x18, GRP_OFF_MEN = 0x1C;
static const uint32_t SOL_OFF_GROUP = 0x3B4, SOL_OFF_UNIT = 0x214, SOL_OFF_POS = 0x48;
static const uint32_t MAX_MEN = 400;

static inline uint32_t R32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint32_t Rd(uint32_t addr) { uint32_t v = 0; ReadDword(addr, &v); return v; }
static inline float RdF(uint32_t addr) { uint32_t v = Rd(addr); float f; memcpy(&f, &v, 4); return f; }
static inline uint8_t RdB(uint32_t addr) { uint8_t v = 0; ReadByte(addr, &v); return v; }
static inline float AsF(uint32_t v) { float f; memcpy(&f, &v, 4); return f; }
static inline uint32_t Rva(uint32_t a) { return a >= g_base && a < g_base + HOOK_ANALYSED_IMAGE_SIZE ? a - g_base : 0; }
static uint32_t Tick() { return GameTickAt(g_base); }

// The tick object (GameTickAt's path) and the battle hanging off it.
static uint32_t TickObj() {
    if (!g_base) return 0;
    uint32_t shell = Rd(g_base + 0x017C755C), mode = Rd(shell + 0x50), owner = Rd(mode);
    return Rd(owner + 0x1C);
}
static uint32_t Battle() { uint32_t t = TickObj(); return t ? Rd(t + TOBJ_OFF_BATTLE) : 0; }
static uint32_t Phase() { uint32_t b = Battle(); return b ? Rd(b + BAT_OFF_PHASE) : 0xFFFFFFFFu; }

// ---------------------------------------------------------------------------
// Fix state
// ---------------------------------------------------------------------------

static volatile LONG g_fixOn = 0;
static volatile LONG g_fBuildings = 0, g_fSkipped = 0, g_fDoors = 0, g_fDoorsKept = 0;
static bool g_keyWatched = false;
static volatile LONG g_testKeys = 0;
static uint8_t* g_toneOn = nullptr;
static uint8_t* g_toneOff = nullptr;
static uint8_t* g_toneRefused = nullptr;

// ---------------------------------------------------------------------------
// Probe files
// ---------------------------------------------------------------------------

static bool g_probe = false;
static char g_dir[MAX_PATH] = "";
static CRITICAL_SECTION g_cs;
static FILE* g_fPhase = nullptr;
static FILE* g_fEvents = nullptr;
static FILE* g_fPieces = nullptr;
static FILE* g_fMen = nullptr;
static FILE* g_fDeaths = nullptr;
static FILE* g_fNear = nullptr;
static LARGE_INTEGER g_qpf = {};
static DWORD g_lastFlush = 0;
static volatile LONG g_rows = 0;
static const LONG MAX_ROWS = 4000000;
static volatile LONG g_cPhase = 0, g_cSetter = 0, g_cDmgSet = 0, g_cCollapse = 0, g_cDamage = 0, g_cFort = 0;
static volatile LONG g_cHp = 0, g_cHpRows = 0, g_cDeaths = 0, g_cPieceRows = 0, g_cMenRows = 0;
static volatile LONG g_startTick = 0;   // tick of the change to phase 5 (0 = not yet)

static uint32_t Ms() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return g_qpf.QuadPart ? (uint32_t)(now.QuadPart * 1000 / g_qpf.QuadPart) : GetTickCount();
}
static bool RowBudget() { return InterlockedIncrement(&g_rows) < MAX_ROWS; }

static FILE* OpenCsv(const char* name, const char* header) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", g_dir, name);
    FILE* f = fopen(path, "w");
    if (!f) return nullptr;
    setvbuf(f, nullptr, _IOFBF, 1 << 20);
    fputs(header, f);
    return f;
}

static bool OpenProbe() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    char* slash = strrchr(exe, '\\');
    if (slash) slash[1] = 0;
    char root[MAX_PATH];
    snprintf(root, sizeof(root), "%scollapsefix_runs\\", exe);
    CreateDirectoryA(root, nullptr);
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_dir, sizeof(g_dir), "%s%04d%02d%02d_%02d%02d%02d\\", root, st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute, st.wSecond);
    if (!CreateDirectoryA(g_dir, nullptr)) { Log("collapsefix: cannot create %s", g_dir); return false; }
    g_fPhase = OpenCsv("collapse_phase.csv", "ms,tick,old,new,ret_rva,battle\n");
    g_fEvents = OpenCsv("collapse_events.csv",
                        "ms,tick,phase,event,ret_rva,piece,arg1,arg2,result,level0,level1,dmglev0,dmglev1,model0,model1,"
                        "dmg0,dmg1,flags0,flags1,wall,nlev,x,z,stack\n");
    g_fPieces = OpenCsv("collapse_pieces.csv",
                        "ms,tick,phase,reason,building,bi,piece,pi,level,nlev,dmglev,model,variant,dmg,thresh_last,flags,"
                        "wall,x,y,z\n");
    g_fMen = OpenCsv("collapse_men.csv", "ms,tick,phase,unit,men,groups,x,z\n");
    g_fDeaths = OpenCsv("collapse_deaths.csv", "ms,tick,phase,ret_rva,soldier,unit,group,hp0,hp1,flag,x,y,z,stack\n");
    g_fNear = OpenCsv("collapse_near.csv", "ms,tick,phase,when,piece,soldier,unit,group,in_group,group_men,x,y,z\n");
    if (!g_fPhase || !g_fEvents || !g_fPieces || !g_fMen || !g_fDeaths || !g_fNear) {
        Log("collapsefix: cannot open the probe files in %s", g_dir);
        return false;
    }
    Log("collapsefix: probe on, writing %s", g_dir);
    return true;
}

static void FlushAll() {
    FILE* fs[] = { g_fPhase, g_fEvents, g_fPieces, g_fMen, g_fDeaths, g_fNear };
    for (FILE* f : fs) if (f) fflush(f);
}
static void MaybeFlush() {
    DWORD now = GetTickCount();
    if (now - g_lastFlush < 2000) return;
    g_lastFlush = now;
    FlushAll();
}

// Module return addresses above a hook's frame (a call instruction right before each), as
// "rva/rva/...", at most `max`.
static void StackTrace(const uint8_t* ctx, char* out, int cap, int max) {
    uint32_t esp = R32(ctx + COLLAPSE_OFF_ESP_BEFORE_PUSHAD) + 4;   // above the hooked function's return address
    int k = 0, n = 0;
    out[0] = 0;
    for (int i = 0; i < 64 && n < max; i++) {
        uint32_t v = Rd(esp + 4 * i), r = Rva(v);
        if (r < 0x1000 || r >= 0x1580000) continue;
        uint8_t b[7] = {};
        if (!MemcpyTry(b, v - 7, 7)) continue;
        bool call = b[2] == 0xE8 || (b[4] == 0xFF && (b[5] & 0x38) == 0x10) || (b[5] == 0xFF && (b[6] & 0x38) == 0x10) ||
                    (b[1] == 0xFF && (b[2] & 0x38) == 0x10) || (b[0] == 0xFF && (b[1] & 0x38) == 0x10);
        if (!call) continue;
        k += snprintf(out + k, cap - k, n ? "/%X" : "%X", r);
        n++;
        if (k >= cap - 10) break;
    }
}

// ---------------------------------------------------------------------------
// Pieces
// ---------------------------------------------------------------------------

struct PieceState { uint32_t level, dmglev, model, variant, dmg, flags; };

static PieceState ReadPiece(uint32_t p) {
    PieceState s;
    s.level = Rd(p + PC_OFF_LEVEL);
    s.dmglev = Rd(p + PC_OFF_DMGLEV);
    s.model = Rd(p + PC_OFF_MODEL);
    s.variant = Rd(p + PC_OFF_VARIANT);
    s.dmg = Rd(p + PC_OFF_DMG);
    s.flags = Rd(p + PC_OFF_FLAGS);    // bytes F0..F3, F0 lowest
    return s;
}

static uint32_t LastThreshold(uint32_t p) {
    uint32_t t = Rd(p + PC_OFF_TMPL), n = Rd(t + TMPL_OFF_NLEV), lev = Rd(t + TMPL_OFF_LEV);
    if (!n || n > 64) return 0;
    return Rd(Rd(lev + 4 * (n >= 2 ? n - 2 : 0)) + LEV_OFF_THRESH);
}

static void EventRow(const uint8_t* ctx, const char* ev, uint32_t p, uint32_t a1, uint32_t a2, uint32_t result,
                     const PieceState& s0, const PieceState& s1) {
    if (!RowBudget()) return;
    char stack[160];
    StackTrace(ctx, stack, sizeof(stack), 8);
    uint32_t nlev = Rd(Rd(p + PC_OFF_TMPL) + TMPL_OFF_NLEV);
    EnterCriticalSection(&g_cs);
    if (g_fEvents)
        fprintf(g_fEvents, "%u,%u,%d,%s,%X,%X,%X,%X,%X,%u,%u,%u,%u,%X,%X,%.3f,%.3f,%08X,%08X,%u,%u,%.2f,%.2f,%s\n", Ms(),
                Tick(), (int)Phase(), ev, Rva(R32(ctx + COLLAPSE_OFF_RET)), p, a1, a2, result, s0.level, s1.level,
                s0.dmglev, s1.dmglev, s0.model, s1.model, AsF(s0.dmg), AsF(s1.dmg), s0.flags, s1.flags,
                RdB(p + PC_OFF_WALL), nlev, RdF(p + PC_OFF_POS), RdF(p + PC_OFF_POS + 8), stack);
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

// Last state seen by the sampler, per piece (open addressing on the piece pointer).
struct PieceRec { uint32_t p; PieceState s; };
static const int PIECE_TABLE = 8192;
static PieceRec* g_pieces = nullptr;
static CRITICAL_SECTION g_pieceCs;

static PieceRec* FindPiece(uint32_t p) {
    uint32_t h = ((p >> 3) * 2654435761u) & (PIECE_TABLE - 1);
    for (int i = 0; i < 256; i++) {
        PieceRec& r = g_pieces[(h + i) & (PIECE_TABLE - 1)];
        if (r.p == p) return &r;
        if (r.p == 0) { r.p = p; memset(&r.s, 0xFF, sizeof(r.s)); return &r; }
    }
    return nullptr;
}

// Every piece of every building: all of them (reason given) or only the changed ones (null).
static void SnapPieces(const char* reason) {
    uint32_t b = Battle();
    if (!PtrPlausible(b) || !g_pieces) return;
    uint32_t nb = Rd(b + BAT_OFF_NBLD), blds = Rd(b + BAT_OFF_BLD);
    if (nb == 0 || nb > 4096 || !PtrPlausible(blds)) return;
    uint32_t ms = Ms(), tick = Tick(), phase = Rd(b + BAT_OFF_PHASE);
    EnterCriticalSection(&g_pieceCs);
    for (uint32_t i = 0; i < nb; i++) {
        uint32_t bld = Rd(blds + 4 * i), np = Rd(bld + BLD_OFF_NPIECE), pcs = Rd(bld + BLD_OFF_PIECE);
        if (!PtrPlausible(bld) || np > 4096 || (np && !PtrPlausible(pcs))) continue;
        for (uint32_t j = 0; j < np; j++) {
            uint32_t p = Rd(pcs + 4 * j);
            if (!PtrPlausible(p)) continue;
            PieceState s = ReadPiece(p);
            PieceRec* r = FindPiece(p);
            bool changed = r && memcmp(&r->s, &s, sizeof(s)) != 0;
            if (r) r->s = s;
            if (!reason && !changed) continue;
            if (!RowBudget()) break;
            EnterCriticalSection(&g_cs);
            if (g_fPieces)
                fprintf(g_fPieces, "%u,%u,%u,%s,%X,%u,%X,%u,%u,%u,%u,%X,%u,%.3f,%d,%08X,%u,%.2f,%.2f,%.2f\n", ms, tick,
                        phase, reason ? reason : "change", bld, i, p, j, s.level, Rd(Rd(p + PC_OFF_TMPL) + TMPL_OFF_NLEV),
                        s.dmglev, s.model, s.variant, AsF(s.dmg), (int)LastThreshold(p), s.flags, RdB(p + PC_OFF_WALL),
                        RdF(p + PC_OFF_POS), RdF(p + PC_OFF_POS + 4), RdF(p + PC_OFF_POS + 8));
            LeaveCriticalSection(&g_cs);
            InterlockedIncrement(&g_cPieceRows);
        }
    }
    LeaveCriticalSection(&g_pieceCs);
}

// ---------------------------------------------------------------------------
// Units: men count of every unit (sum over its groups)
// ---------------------------------------------------------------------------

struct UnitRec { uint32_t u, men; };
static const int UNIT_TABLE = 1024;
static UnitRec g_units[UNIT_TABLE];

// The groups of a unit: main group, then its two group lists, without repeats.
static int UnitGroups(uint32_t u, uint32_t* out, int cap) {
    int n = 0;
    const uint32_t lists[2][2] = { { U_OFF_GA_N, U_OFF_GA }, { U_OFF_GB_N, U_OFF_GB } };
    uint32_t m = Rd(u + U_OFF_MAIN);
    if (PtrPlausible(m)) out[n++] = m;
    for (const auto& l : lists) {
        uint32_t c = Rd(u + l[0]), a = Rd(u + l[1]);
        if (c > 16 || (c && !PtrPlausible(a))) continue;
        for (uint32_t i = 0; i < c && n < cap; i++) {
            uint32_t g = Rd(a + 4 * i);
            bool dup = !PtrPlausible(g);
            for (int k = 0; k < n && !dup; k++) dup = out[k] == g;
            if (!dup) out[n++] = g;
        }
    }
    return n;
}

// Calls fn(unit, group, men list, count) for every group of every unit of the battle.
template <class F> static void ForEachGroup(F fn) {
    uint32_t b = Battle();
    if (!PtrPlausible(b)) return;
    uint32_t na = Rd(b + BAT_OFF_NALLY), allies = Rd(b + BAT_OFF_ALLY);
    if (na == 0 || na > 16 || !PtrPlausible(allies)) return;
    for (uint32_t a = 0; a < na; a++) {
        uint32_t al = Rd(allies + 4 * a), nr = Rd(al + ALLY_OFF_NARMY), armies = Rd(al + ALLY_OFF_ARMY);
        if (!PtrPlausible(al) || nr > 64 || (nr && !PtrPlausible(armies))) continue;
        for (uint32_t r = 0; r < nr; r++) {
            uint32_t army = Rd(armies + 4 * r), nu = Rd(army + ARMY_OFF_NUNIT), units = Rd(army + ARMY_OFF_UNIT);
            if (!PtrPlausible(army) || nu > 256 || (nu && !PtrPlausible(units))) continue;
            for (uint32_t k = 0; k < nu; k++) {
                uint32_t u = Rd(units + 4 * k);
                if (!PtrPlausible(u)) continue;
                uint32_t groups[40];
                int ng = UnitGroups(u, groups, 40);
                for (int g = 0; g < ng; g++) {
                    uint32_t n = Rd(groups[g] + GRP_OFF_COUNT), list = Rd(groups[g] + GRP_OFF_MEN);
                    if (n > MAX_MEN || (n && !PtrPlausible(list))) continue;
                    fn(u, groups[g], list, n);
                }
            }
        }
    }
}

// Sampler thread only (static buffers).
static void SnapMen() {
    uint32_t ms = Ms(), tick = Tick(), phase = Phase();
    static uint32_t cur[UNIT_TABLE], men[UNIT_TABLE], ngr[UNIT_TABLE];
    static float sx[UNIT_TABLE], sz[UNIT_TABLE];
    static bool pos[UNIT_TABLE];
    int nu = 0;
    ForEachGroup([&](uint32_t u, uint32_t g, uint32_t list, uint32_t n) {
        int i = 0;
        while (i < nu && cur[i] != u) i++;
        if (i == nu) {
            if (nu == UNIT_TABLE) return;
            cur[nu] = u; men[nu] = 0; ngr[nu] = 0; pos[nu] = false; nu++;
        }
        men[i] += n;
        ngr[i]++;
        uint32_t s0 = n ? Rd(list) : 0;
        if (PtrPlausible(s0) && !pos[i]) { sx[i] = RdF(s0 + SOL_OFF_POS); sz[i] = RdF(s0 + SOL_OFF_POS + 8); pos[i] = true; }
    });
    for (int i = 0; i < nu; i++) {
        uint32_t u = cur[i];
        uint32_t h = ((u >> 4) * 2654435761u) & (UNIT_TABLE - 1);
        UnitRec* rec = nullptr;
        for (int k = 0; k < 64; k++) {
            UnitRec& x = g_units[(h + k) & (UNIT_TABLE - 1)];
            if (x.u == u || x.u == 0) { rec = &x; break; }
        }
        if (!rec || (rec->u == u && rec->men == men[i])) continue;
        rec->u = u;
        rec->men = men[i];
        if (!RowBudget()) return;
        EnterCriticalSection(&g_cs);
        if (g_fMen)
            fprintf(g_fMen, "%u,%u,%u,%X,%u,%u,%.2f,%.2f\n", ms, tick, phase, u, men[i], ngr[i], pos[i] ? sx[i] : 0.f,
                    pos[i] ? sz[i] : 0.f);
        LeaveCriticalSection(&g_cs);
        InterlockedIncrement(&g_cMenRows);
    }
}

// Soldiers within 30 m (x, z) of a piece that collapses in battle, followed for 10 game seconds:
// still in their group, and where.
struct NearRec { uint32_t piece, s, u, g, tick, done; };
static const int NEAR_MAX = 4096;
static NearRec g_near[NEAR_MAX];
static int g_nNear = 0;
static CRITICAL_SECTION g_nearCs;

static bool InGroup(uint32_t s, uint32_t g, uint32_t* count) {
    uint32_t n = Rd(g + GRP_OFF_COUNT), list = Rd(g + GRP_OFF_MEN);
    *count = n;
    if (n > MAX_MEN || !PtrPlausible(list)) return false;
    for (uint32_t i = 0; i < n; i++) if (Rd(list + 4 * i) == s) return true;
    return false;
}

static void NearRow(const char* when, const NearRec& r) {
    uint32_t cnt = 0;
    bool in = InGroup(r.s, r.g, &cnt);
    if (!RowBudget()) return;
    EnterCriticalSection(&g_cs);
    if (g_fNear)
        fprintf(g_fNear, "%u,%u,%d,%s,%X,%X,%X,%X,%d,%u,%.2f,%.2f,%.2f\n", Ms(), Tick(), (int)Phase(), when, r.piece, r.s,
                r.u, r.g, in ? 1 : 0, cnt, RdF(r.s + SOL_OFF_POS), RdF(r.s + SOL_OFF_POS + 4), RdF(r.s + SOL_OFF_POS + 8));
    LeaveCriticalSection(&g_cs);
}

static void NoteNear(uint32_t piece) {
    float px = RdF(piece + PC_OFF_POS), pz = RdF(piece + PC_OFF_POS + 8);
    uint32_t tick = Tick();
    EnterCriticalSection(&g_nearCs);
    ForEachGroup([&](uint32_t u, uint32_t g, uint32_t list, uint32_t n) {
        for (uint32_t i = 0; i < n; i++) {
            uint32_t s = Rd(list + 4 * i);
            if (!PtrPlausible(s)) continue;
            float dx = RdF(s + SOL_OFF_POS) - px, dz = RdF(s + SOL_OFF_POS + 8) - pz;
            if (dx * dx + dz * dz > 30.f * 30.f || g_nNear >= NEAR_MAX) continue;
            NearRec& r = g_near[g_nNear++];
            r.piece = piece; r.s = s; r.u = u; r.g = g; r.tick = tick; r.done = 0;
            NearRow("collapse", r);
        }
    });
    LeaveCriticalSection(&g_nearCs);
}

static void FollowNear() {
    uint32_t tick = Tick();
    static const uint32_t after[] = { 5, 10, 20, 40, 100 };
    EnterCriticalSection(&g_nearCs);
    for (int i = 0; i < g_nNear; i++) {
        for (int k = 0; k < 5; k++) {
            if (((g_near[i].done >> k) & 1) || tick < g_near[i].tick + after[k]) continue;
            g_near[i].done |= 1u << k;
            char w[16];
            snprintf(w, sizeof(w), "t+%u", after[k]);
            NearRow(w, g_near[i]);
        }
    }
    LeaveCriticalSection(&g_nearCs);
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

typedef void (__fastcall* SetterFn)(uint32_t self, uint32_t edx, uint32_t level, uint32_t variant);
typedef void (__fastcall* DmgSetFn)(uint32_t self, uint32_t edx, uint32_t dmg);
typedef uint32_t (__fastcall* DamageFn)(uint32_t self, uint32_t edx, uint32_t dmg);
typedef void (__fastcall* FortFn)(uint32_t self, uint32_t edx, uint32_t data);

static uint32_t HandleSetter(const uint8_t* ctx) {
    if (!g_probe) return HOOK_CONTINUE;
    uint32_t p = R32(ctx + COLLAPSE_OFF_ECX), lev = R32(ctx + COLLAPSE_OFF_ARG1), var = R32(ctx + COLLAPSE_OFF_ARG2);
    InterlockedIncrement(&g_cSetter);
    PieceState s0 = ReadPiece(p);
    ((SetterFn)g_collapseTrampolines[H_SETTER])(p, 0, lev, var);
    PieceState s1 = ReadPiece(p);
    EventRow(ctx, "apply", p, lev, var, 0, s0, s1);
    return HOOK_RETURN(0);
}

static uint32_t HandleDmgSet(const uint8_t* ctx) {
    if (!g_probe) return HOOK_CONTINUE;
    uint32_t p = R32(ctx + COLLAPSE_OFF_ECX), dmg = R32(ctx + COLLAPSE_OFF_ARG1);
    InterlockedIncrement(&g_cDmgSet);
    PieceState s0 = ReadPiece(p);
    ((DmgSetFn)g_collapseTrampolines[H_DMGSET])(p, 0, dmg);
    PieceState s1 = ReadPiece(p);
    EventRow(ctx, "dmgset", p, dmg, 0, 0, s0, s1);
    return HOOK_RETURN(0);
}

static void HandleCollapse(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t p = R32(ctx + COLLAPSE_OFF_ECX);
    InterlockedIncrement(&g_cCollapse);
    // the effect list (+0x40 .. +0x44, next at +4, type at +0xC) and the two spawners it calls
    uint32_t tobj = Rd(Rd(p + 0xC) + 8), mgr = Rd(tobj + 0x98);
    uint32_t f2 = Rva(Rd(Rd(mgr + 0x534) + 8)), f3 = Rva(Rd(Rd(mgr + 0x94) + 8));   // embedded objects: vtable at +0
    uint32_t n2 = 0, n3 = 0, it = Rd(p + 0x40), end = Rd(p + 0x44);
    for (int i = 0; i < 256 && it && it != end; i++) {
        uint32_t ty = Rd(it + 0xC);
        if (ty == 2) n2++;
        else if (ty == 3) n3++;
        it = Rd(it + 4);
    }
    PieceState s = ReadPiece(p);
    EventRow(ctx, "collapse", p, (n2 << 16) | n3, f2, f3, s, s);
    uint32_t ph = Phase();
    if (ph >= 4 && ph <= 8) NoteNear(p);
}

static uint32_t HandleDamage(const uint8_t* ctx) {
    if (!g_probe) return HOOK_CONTINUE;
    uint32_t p = R32(ctx + COLLAPSE_OFF_ECX), dmg = R32(ctx + COLLAPSE_OFF_ARG1);
    InterlockedIncrement(&g_cDamage);
    PieceState s0 = ReadPiece(p);
    uint32_t r = ((DamageFn)g_collapseTrampolines[H_DAMAGE])(p, 0, dmg);
    PieceState s1 = ReadPiece(p);
    EventRow(ctx, "damage", p, dmg, 0, r, s0, s1);
    return HOOK_RETURN(r);
}

static uint32_t HandleFort(const uint8_t* ctx) {
    if (!g_probe) return HOOK_CONTINUE;
    uint32_t self = R32(ctx + COLLAPSE_OFF_ECX), data = R32(ctx + COLLAPSE_OFF_ARG1);
    InterlockedIncrement(&g_cFort);
    PieceState z = {};
    EventRow(ctx, "fort_begin", self, data, Rd(data + 4), Rd(data + 0x14), z, z);
    ((FortFn)g_collapseTrampolines[H_FORT])(self, 0, data);
    EventRow(ctx, "fort_end", self, data, Rd(data + 4), Rd(data + 0x14), z, z);
    return HOOK_RETURN(0);
}

static void HandlePhase(const uint8_t* ctx) {
    uint32_t bat = R32(ctx + COLLAPSE_OFF_ECX), nw = R32(ctx + COLLAPSE_OFF_ARG1);
    uint32_t old = Rd(bat + BAT_OFF_PHASE), tick = Tick();
    if (nw == 5 && old != 5) InterlockedExchange(&g_startTick, (LONG)(tick ? tick : 1));
    if (nw <= 3) InterlockedExchange(&g_startTick, 0);
    if (!g_probe) return;
    InterlockedIncrement(&g_cPhase);
    EnterCriticalSection(&g_cs);
    if (g_fPhase) fprintf(g_fPhase, "%u,%u,%u,%u,%X,%X\n", Ms(), tick, old, nw, Rva(R32(ctx + COLLAPSE_OFF_RET)), bat);
    FlushAll();
    LeaveCriticalSection(&g_cs);
    char why[32];
    snprintf(why, sizeof(why), "phase%u_to_%u", old, nw);
    SnapPieces(why);
}

static void HandleHp(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t s = R32(ctx + COLLAPSE_OFF_ARG1), hp = R32(ctx + COLLAPSE_OFF_ARG2), flag = R32(ctx + COLLAPSE_OFF_ARG3);
    InterlockedIncrement(&g_cHp);
    uint32_t tick = Tick(), phase = Phase(), start = (uint32_t)g_startTick;
    bool death = (int32_t)hp <= 0;
    if (death) InterlockedIncrement(&g_cDeaths);
    bool window = phase <= 4 || (start && tick <= start + 300);
    if (!death && !window) return;
    if (!RowBudget()) return;
    InterlockedIncrement(&g_cHpRows);
    char stack[160];
    StackTrace(ctx, stack, sizeof(stack), 8);
    uint32_t hp0 = Rd(s + 0x680);
    EnterCriticalSection(&g_cs);
    if (g_fDeaths)
        fprintf(g_fDeaths, "%u,%u,%d,%X,%X,%X,%X,%d,%d,%u,%.2f,%.2f,%.2f,%s\n", Ms(), tick, (int)phase,
                Rva(R32(ctx + COLLAPSE_OFF_RET)), s, Rd(s + SOL_OFF_UNIT), Rd(s + SOL_OFF_GROUP), (int)hp0, (int)hp,
                flag & 0xFF, RdF(s + SOL_OFF_POS), RdF(s + SOL_OFF_POS + 4), RdF(s + SOL_OFF_POS + 8), stack);
    MaybeFlush();
    LeaveCriticalSection(&g_cs);
}

// FUN_1043c870 (ECX = building; no stack arguments): the battle start's gate damage. Counts the
// intact gate doors the game is about to destroy; with the fix on, returns before it does.
static uint32_t HandleGates(const uint8_t* ctx) {
    uint32_t bld = R32(ctx + COLLAPSE_OFF_ECX);
    bool on = InterlockedCompareExchange(&g_fixOn, 0, 0) != 0;
    InterlockedIncrement(&g_fBuildings);
    uint32_t np = Rd(bld + BLD_OFF_NPIECE), pcs = Rd(bld + BLD_OFF_PIECE), doors = 0;
    if (np <= 4096 && PtrPlausible(pcs)) {
        for (uint32_t i = 0; i < np; i++) {
            uint32_t p = Rd(pcs + 4 * i);
            if (!PtrPlausible(p) || !RdB(p + PC_OFF_GATE) || RdB(p + PC_OFF_DESTROYED)) continue;
            uint32_t settings = Rd(Rd(Rd(p + PC_OFF_OWNER) + OWNER_OFF_TOBJ) + TOBJ_OFF_SETTINGS);
            if (!RdB(settings + SET_OFF_GATES_OPEN)) continue;
            doors++;
            if (g_probe) {
                PieceState st = ReadPiece(p);
                EventRow(ctx, on ? "gate_kept" : "gate_smash", p, bld, i, 0, st, st);
            }
        }
    }
    if (doors) {
        InterlockedExchangeAdd(&g_fDoors, (LONG)doors);
        if (on) InterlockedExchangeAdd(&g_fDoorsKept, (LONG)doors);
        Log("collapsefix: battle start, gates open to all: building %08X, %u intact gate door(s) %s", bld, doors,
            on ? "kept (fix ON)" : "destroyed by the game (fix OFF)");
    }
    if (!on) return HOOK_CONTINUE;
    InterlockedIncrement(&g_fSkipped);
    return HOOK_RETURN(0);
}

// ---------------------------------------------------------------------------
// Sampler: pieces and men every 50 ms while a battle runs
// ---------------------------------------------------------------------------

static HANDLE g_sampler = nullptr;
static volatile LONG g_stop = 0;

static DWORD WINAPI SamplerThread(LPVOID) {
    while (!InterlockedCompareExchange(&g_stop, 0, 0)) {
        Sleep(50);
        uint32_t ph = Phase();
        if (ph == 0xFFFFFFFFu || ph > 13) continue;
        SnapPieces(nullptr);
        SnapMen();
        FollowNear();
        EnterCriticalSection(&g_cs);
        MaybeFlush();
        LeaveCriticalSection(&g_cs);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Key, detours, harness exports
// ---------------------------------------------------------------------------

// I toggles the fix (dev build; focus only through proxy/keys). Harness: CollapseFixTestKeys bit 0.
extern "C" __declspec(dllexport) void __stdcall CollapseFixTestKeys(LONG bits) { InterlockedExchange(&g_testKeys, bits); }

static void ToggleKey() {
    bool on = InterlockedCompareExchange(&g_fixOn, 0, 0) == 0;
    InterlockedExchange(&g_fixOn, on ? 1 : 0);
    Log("collapsefix: I, fix %s", on ? "ON" : "OFF");
    Tone_Play(on ? g_toneOn : g_toneOff);
}

extern "C" uint32_t __stdcall CollapseHandler0(const uint8_t* ctx) { return HandleSetter(ctx); }
extern "C" uint32_t __stdcall CollapseHandler1(const uint8_t* ctx) { return HandleDmgSet(ctx); }
extern "C" void __stdcall CollapseHandler2(const uint8_t* ctx) { HandleCollapse(ctx); }
extern "C" uint32_t __stdcall CollapseHandler3(const uint8_t* ctx) { return HandleDamage(ctx); }
extern "C" uint32_t __stdcall CollapseHandler4(const uint8_t* ctx) { return HandleFort(ctx); }
extern "C" void __stdcall CollapseHandler5(const uint8_t* ctx) { HandlePhase(ctx); }
extern "C" void __stdcall CollapseHandler6(const uint8_t* ctx) { HandleHp(ctx); }
extern "C" uint32_t __stdcall CollapseHandler7(const uint8_t* ctx) { return HandleGates(ctx); }

HOOK_DETOUR_RET(CollapseDetour0, CollapseHandler0, g_collapseTrampolines, 0, 8)     // FUN_10477f40: __thiscall, 2 stack args
HOOK_DETOUR_RET(CollapseDetour1, CollapseHandler1, g_collapseTrampolines, 1, 4)     // FUN_10477ff0: __thiscall, 1 stack arg
HOOK_DETOUR(CollapseDetour2, CollapseHandler2, g_collapseTrampolines, 2)
HOOK_DETOUR_RET(CollapseDetour3, CollapseHandler3, g_collapseTrampolines, 3, 4)     // FUN_1047b370: __thiscall, 1 stack arg
HOOK_DETOUR_RET(CollapseDetour4, CollapseHandler4, g_collapseTrampolines, 4, 4)     // FUN_10426940: __thiscall, 1 stack arg
HOOK_DETOUR(CollapseDetour5, CollapseHandler5, g_collapseTrampolines, 5)
HOOK_DETOUR(CollapseDetour6, CollapseHandler6, g_collapseTrampolines, 6)
HOOK_DETOUR_RET(CollapseDetour7, CollapseHandler7, g_collapseTrampolines, 7, 0)     // FUN_1043c870: __thiscall, no stack args

static const HookDetour g_detours[HOOK_COUNT] = { CollapseDetour0, CollapseDetour1, CollapseDetour2, CollapseDetour3,
                                                  CollapseDetour4, CollapseDetour5, CollapseDetour6, CollapseDetour7 };

// Harness only (fixes/wall-collapse-fix/tests): fix and probe counters.
extern "C" __declspec(dllexport) void __stdcall CollapseFixTestCounters(long* out) {
    out[0] = g_probe ? 1 : 0;
    out[1] = g_fixOn;
    out[2] = g_base ? 1 : 0;
    out[3] = g_cPhase;
    out[4] = g_cSetter;
    out[5] = g_cDmgSet;
    out[6] = g_cCollapse;
    out[7] = g_cDamage;
    out[8] = g_cFort;
    out[9] = g_cHp;
    out[10] = g_cHpRows;
    out[11] = g_cDeaths;
    out[12] = g_cPieceRows;
    out[13] = g_cMenRows;
    out[14] = g_startTick;
    out[15] = g_fBuildings;
    out[16] = g_fSkipped;
    out[17] = g_fDoors;
    out[18] = g_fDoorsKept;
}

// ---------------------------------------------------------------------------
// Install / remove
// ---------------------------------------------------------------------------

static bool EnvIs(const char* name, char c) {
    char v[8] = "";
    return GetEnvironmentVariableA(name, v, sizeof(v)) > 0 && v[0] == c;
}

void CollapseFix_Install() {
    Log("collapsefix: wall collapse fix (dev build)");
    Mem_Init();
    static const ToneSeg on[] = { { 880, 160 } };
    static const ToneSeg off[] = { { 330, 300 } };
    static const ToneSeg refused[] = { { 200, 450 }, { 0, 150 }, { 200, 450 }, { 0, 150 }, { 200, 450 } };
    g_toneOn = Tone_Make(on, 1);
    g_toneOff = Tone_Make(off, 1);
    g_toneRefused = Tone_Make(refused, 5);
    uint32_t size = 0;
    uint8_t* base = Hook_GameModule(false, &size);
    if (!base) { Log("collapsefix: REFUSED, empire.retail.dll is not loaded"); Tone_Play(g_toneRefused); return; }
    if (size != HOOK_ANALYSED_IMAGE_SIZE) {
        Log("collapsefix: REFUSED, SizeOfImage 0x%08X is not the analysed build", size);
        Tone_Play(g_toneRefused);
        return;
    }
    for (int h = 0; h < HOOK_COUNT; h++) {
        if (!Hook_SiteMatches(base, g_hookDefs[h])) {
            Log("collapsefix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", g_hookDefs[h].rva,
                g_hookDefs[h].func);
            Tone_Play(g_toneRefused);
            return;
        }
    }
    g_base = (uint32_t)(uintptr_t)base;
    InitializeCriticalSection(&g_cs);
    InitializeCriticalSection(&g_pieceCs);
    InitializeCriticalSection(&g_nearCs);
    QueryPerformanceFrequency(&g_qpf);
#ifdef COLLAPSEFIX_START_OFF
    bool fixOff = !EnvIs("COLLAPSEFIX_ON", '1');      // live-check build: off at launch, I turns it on
    const char* offWhy = "OFF at launch (build option COLLAPSEFIX_START_OFF)";
#else
    bool fixOff = EnvIs("COLLAPSEFIX_OFF", '1');
    const char* offWhy = "OFF (COLLAPSEFIX_OFF=1)";
#endif
    if (EnvIs("COLLAPSEFIX_PROBE", '1')) {
        g_pieces = (PieceRec*)VirtualAlloc(nullptr, sizeof(PieceRec) * PIECE_TABLE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (g_pieces && OpenProbe()) g_probe = true;
        else Log("collapsefix: probe not started (no run folder or no memory)");
    }
    bool use[HOOK_COUNT];
    for (int h = 0; h < HOOK_COUNT; h++) use[h] = !g_hookDefs[h].probeOnly || g_probe;
    if (!Hook_Prepare(base, g_hookDefs, HOOK_COUNT, use, g_collapseTrampolines, g_slots)) {
        Log("collapsefix: REFUSED, no trampoline memory");
        Tone_Play(g_toneRefused);
        g_probe = false;
        g_base = 0;
        return;
    }
    InterlockedExchange(&g_fixOn, fixOff ? 0 : 1);
    if (g_probe) g_sampler = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    Hook_Patch(g_detours, g_slots, HOOK_COUNT);
    for (int h = 0; h < HOOK_COUNT; h++)
        if (g_slots[h].active)
            Log("collapsefix: hook %s (%s) installed at RVA 0x%06X", g_hookDefs[h].func, g_hookDefs[h].role, g_hookDefs[h].rva);
    Log("collapsefix: fix %s: the battle start no longer destroys the gates of a castle whose gates are open to all",
        fixOff ? offWhy : "ON");
    g_keyWatched = Keys_Watch('I', &g_testKeys, 1, ToggleKey);
    Log("collapsefix: I toggles the fix on / off%s", g_keyWatched ? "" : " (ERROR: key poll not started)");
}

void CollapseFix_Remove() {
    if (g_keyWatched) { Keys_Unwatch(ToggleKey); g_keyWatched = false; }
    Hook_Remove(g_slots, HOOK_COUNT);
    if (!g_base) return;
    Log("collapsefix: battle start gate damage calls %ld (skipped %ld); intact gate doors %ld, kept %ld", g_fBuildings,
        g_fSkipped, g_fDoors, g_fDoorsKept);
    if (g_sampler) {
        InterlockedExchange(&g_stop, 1);
        WaitForSingleObject(g_sampler, 2000);
        CloseHandle(g_sampler);
        g_sampler = nullptr;
    }
    if (g_probe) {
        EnterCriticalSection(&g_cs);
        FILE** fs[] = { &g_fPhase, &g_fEvents, &g_fPieces, &g_fMen, &g_fDeaths, &g_fNear };
        for (FILE** f : fs) if (*f) { fclose(*f); *f = nullptr; }
        g_probe = false;
        LeaveCriticalSection(&g_cs);
        Log("collapsefix: probe closed: phases %ld, level applies %ld, damage sets %ld, collapses %ld, damage adds %ld, "
            "fort damage %ld, hp sets %ld (rows %ld, deaths %ld), piece rows %ld, men rows %ld, rows %ld",
            g_cPhase, g_cSetter, g_cDmgSet, g_cCollapse, g_cDamage, g_cFort, g_cHp, g_cHpRows, g_cDeaths, g_cPieceRows,
            g_cMenRows, g_rows);
    }
}
