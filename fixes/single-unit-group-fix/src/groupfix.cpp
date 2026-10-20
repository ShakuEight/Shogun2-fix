// groupfix.cpp -- single unit group fix for Total War: Shogun 2 (original x86 Empire.Retail.dll).
// Folder fixes/single-unit-group-fix/, code name groupfix.
//
// Dev build. Requested feature, not a bug: in battle, G groups the selected units (a group card
// and a number), Ctrl+number groups them under that number, Ctrl+G locks a group's formation. The
// game refuses all of it for a single selected unit: a group needs two or more.
//
// Engine decode (docs/re/unit_groups_x86.md):
// - The battle UI's selection manager (singleton [0x11bbead8]) holds the selected units in a list
//   at +0x24 (count +0x28, array +0x2C) and its unit groups at +0x40 (count) / +0x44 (array;
//   a group entry: unit count +0x0C, units +0x10, number slot +0x14).
// - FUN_10f89a40 (ECX = manager, slot or -1) makes a group of the selection: it starts with
//   CMP dword [ESI+0x28],1 / JBE return, so one selected unit returns before anything happens.
//   With two or more it calls the manager's virtual +0x20 (the group create, which issues the
//   battle command BCQ_ARMY_CREATE_UNIT_GROUP), records the group under its number and refreshes
//   the cards. Its callers: FUN_10f9de40 (G: ungroup when the selection is exactly a group, else a
//   group in the first free number slot) and FUN_10f91e40 (Ctrl+number: that slot).
// - The engine side has no unit count check: the command's executor FUN_102a5e40 makes the group
//   (FUN_10166e40), adds each unit (group virtual +0x70, FUN_1041dcc0) and finalises it
//   (FUN_101ffa70); removing units (+0x74, FUN_10472ed0) never disbands a group. The two script
//   bindings that refuse groups of one ("group must have 2 or more units") are the Lua
//   unit_controller's, not the player's.
//
// - The group frame (the yellow box with the number around a group's unit cards): the selection
//   manager's FUN_110e7280 keeps one "group_frame" component per UI group and places it with
//   FUN_11223470 (ECX = frame, the group's unit list, the two-row card layout flag), which starts
//   with CMP dword [EBP+4],1 / JA: one unit or none hides the frame and returns. Its two-row
//   branch for an odd count reads a second card while looking for the row break.
// - G reaches the group through the button "button_group" (default_keys.xml maps toggle_group to
//   it); an inactive button swallows its key. The land orders panel's button state table builder
//   FUN_111016d0 sets the group button (table entry +0x240) active only when the selected unit
//   count (the list FUN_110ee490 fills, [ESP+0x38]) is above 1 and there is room for another
//   group, else inactive (CMP dword [ESP+0x38],1 / JBE at +0x366). Found with the probe in a live
//   session: the state setter FUN_110e22f0 called from FUN_111024e0 set "inactive" at one selected
//   unit and "active" at two.
//
// The fix (on from launch; GROUPFIX_OFF=1 starts off, I toggles; a build with the CMake option
// GROUPFIX_START_OFF starts off unless GROUPFIX_ON=1):
// - the immediates of the three count tests become 0 (selection, frame, group button state): only
//   an empty selection or group is refused. Each whole test is byte-checked first; only the immediate is written
//   (0 = fix on, 1 = the game's own value);
// - an entry hook on FUN_11223470 clears the two-row flag for a one-unit group, so its frame takes
//   the single-row branch (sized from the first card and the count) and never reads a second card.
//
// Dev log: every group made from a single selected unit is logged (a hook right after the
// selection test, FUN_10f89a40+0x9, installed in every dev build), and the first one-unit frame.
//
// The research probe (GROUPFIX_PROBE=1; on by default in a GROUPFIX_PROBE_DEFAULT build, where
// GROUPFIX_PROBE=0 turns it off) writes <game>\groupfix_runs\<stamp>\group_events.csv: the G and
// Ctrl+number shortcuts, the lock button, every group made from the selection, the engine's group
// finalise, unit add / remove and group destroy, the group frame rebuild, the UI events
// CreateGroup / GroupSelectionChanged, the group button's construction, shortcut (with the flag
// that decides whether its click runs), state and visibility changes, the card manager's
// shortcuts and (sampler, 50 ms) the selection manager's unit groups on every change, with the
// selection count, the group's unit counts, the caller and the module return addresses found on
// the stack.

#include "log.h"
#include "hook.h"
#include "gamemem.h"
#include "keys.h"
#include "stack_offsets_group.h"
#include "stack_offsets_group_mid.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static uint32_t g_base = 0;

// ---------------------------------------------------------------------------
// The fix sites: three unit-count tests "more than 1" (docs/re/prologues_groupfix.txt,
// prologues_groupfix_fix2.txt, prologues_groupfix_fix3.txt). Each is byte-checked as a whole; only
// its immediate is written, 1 (the game) or 0 (the fix). Never hooked.
// ---------------------------------------------------------------------------

enum { F_SELECTION = 0, F_FRAME, F_BUTTON, FIX_SITE_COUNT };
static const HookDef g_fixSites[FIX_SITE_COUNT] = {
    { "FUN_10f89a40+0x3", "group of the selection: more than 1 unit", 0xF89A43, 6,
      { 0x83, 0x7E, 0x28, 0x01, 0x76, 0x3E }, -1, false },          // CMP dword [ESI+0x28],1; JBE
    { "FUN_11223470+0xF", "group frame shown: more than 1 unit", 0x122347F, 7,
      { 0x83, 0x7D, 0x04, 0x01, 0x8B, 0x4B, 0x04 }, -1, false },    // CMP dword [EBP+4],1; MOV ECX,[EBX+4] (JA follows)
    { "FUN_111016d0+0x366", "group button active: more than 1 unit selected", 0x1101A36, 7,
      { 0x83, 0x7C, 0x24, 0x38, 0x01, 0x76, 0x21 }, -1, false },    // CMP dword [ESP+0x38],1; JBE (inactive)
};
static const int kImmOff[FIX_SITE_COUNT] = { 3, 3, 4 };       // offset of the immediate in each site
static const uint8_t IMM_GAME = 0x01, IMM_FIX = 0x00;

// ---------------------------------------------------------------------------
// Hooks (stolen bytes: docs/re/prologues_groupfix.txt; whole instructions, no branch into the
// range)
// ---------------------------------------------------------------------------

enum {
    H_CREATED = 0,  // FUN_10f89a40+0x9: past the count test (ESI = manager; one push done, slot at the mid ARG1)
    H_TOGGLE,       // FUN_10f9de40: G (ECX = manager)
    H_USEGROUP,     // FUN_10f91e40: Ctrl+number (ECX = manager, slot)
    H_LOCKBTN,      // 0x11211d20: lock formation button / Ctrl+G (ECX = button)
    H_FINALISE,     // FUN_101ffa70: engine group finalised after create (ECX = group)
    H_REMOVE,       // FUN_10472ed0: unit removed from a group (ECX = group, unit)
    H_ADD,          // FUN_1041dcc0: unit added to a group (ECX = group, unit)
    H_DESTROY,      // FUN_10169170: army destroys a group (ECX = army, group id)
    H_FRAMES,       // FUN_110e7280: group frames of the unit cards rebuilt (ECX = card panel)
    H_EVCREATE,     // FUN_10f93fd0: UI script event CreateGroup
    H_EVSELCHG,     // FUN_10f8de60: UI script event GroupSelectionChanged
    H_BTNKEY,       // FUN_11065720: a battle button's shortcut (ECX = button, 3 args); logged for the group button
    H_CARDKEY,      // FUN_10fa37e0: the card manager's shortcut handler (shortcut name, 2 args)
    H_FRAMESIZE,    // FUN_11223470: one group frame placed around its cards (ECX = frame, units list, two-row flag); fix
    H_SETSTATE,     // FUN_10f9bf20: UI component state set by name (ECX = component, name); logged for the group button
    H_SETVIS,       // FUN_10f9c360: UI component shown / hidden (ECX = component, bool); logged for the group button
    H_GBTNCTOR,     // FUN_11216b20: BattleButtonGroup constructed (ECX = button, its UI component)
    HOOK_COUNT
};

static const HookDef g_hookDefs[HOOK_COUNT] = {
    { "FUN_10f89a40+0x9", "group of the selection made", 0xF89A49, 8,
      { 0x8B, 0x06, 0x53, 0x57, 0x8B, 0x7C, 0x24, 0x10 }, -1, false },              // MOV EAX,[ESI]; PUSH EBX; PUSH EDI; MOV EDI,[ESP+0x10]
    { "FUN_10f9de40", "G shortcut", 0xF9DE40, 7,
      { 0x56, 0x8B, 0xF1, 0x80, 0x7E, 0x20, 0x00 }, -1, true },                     // PUSH ESI; MOV ESI,ECX; CMP byte [ESI+0x20],0
    { "FUN_10f91e40", "Ctrl+number shortcut", 0xF91E40, 10,
      { 0x83, 0xEC, 0x10, 0x80, 0x3D, 0x4F, 0xE8, 0xBB, 0x11, 0x01 }, 5, true },    // SUB ESP,0x10; CMP byte [0x11bbe84f],1
    { "0x11211d20", "lock formation button", 0x1211D20, 5,
      { 0xA1, 0x80, 0x81, 0xBC, 0x11 }, 1, true },                                  // MOV EAX,[0x11bc8180]
    { "FUN_101ffa70", "engine group finalised", 0x1FFA70, 6,
      { 0x83, 0xEC, 0x34, 0x56, 0x8B, 0xF1 }, -1, true },                           // SUB ESP,0x34; PUSH ESI; MOV ESI,ECX
    { "FUN_10472ed0", "unit removed from a group", 0x472ED0, 8,
      { 0x83, 0xEC, 0x0C, 0x53, 0x8B, 0x5C, 0x24, 0x14 }, -1, true },               // SUB ESP,0xC; PUSH EBX; MOV EBX,[ESP+0x14]
    { "FUN_1041dcc0", "unit added to a group", 0x41DCC0, 5,
      { 0x56, 0x8B, 0x74, 0x24, 0x08 }, -1, true },                                 // PUSH ESI; MOV ESI,[ESP+8]
    { "FUN_10169170", "army destroys a group", 0x169170, 5,
      { 0x53, 0x55, 0x8B, 0xD9, 0x56 }, -1, true },                                 // PUSH EBX; PUSH EBP; MOV EBX,ECX; PUSH ESI
    // docs/re/prologues_groupfix_ui.txt
    { "FUN_110e7280", "group frames rebuilt", 0x10E7280, 5,
      { 0x83, 0xEC, 0x34, 0x53, 0x56 }, -1, true },                                 // SUB ESP,0x34; PUSH EBX; PUSH ESI
    { "FUN_10f93fd0", "UI event CreateGroup", 0xF93FD0, 6,
      { 0x83, 0xEC, 0x48, 0x56, 0x6A, 0x00 }, -1, true },                           // SUB ESP,0x48; PUSH ESI; PUSH 0
    { "FUN_10f8de60", "UI event GroupSelectionChanged", 0xF8DE60, 5,
      { 0x83, 0xEC, 0x24, 0x6A, 0x00 }, -1, true },                                 // SUB ESP,0x24; PUSH 0
    { "FUN_11065720", "battle button shortcut", 0x1065720, 8,
      { 0x83, 0xEC, 0x08, 0x80, 0x7C, 0x24, 0x14, 0x00 }, -1, true },               // SUB ESP,8; CMP byte [ESP+0x14],0
    { "FUN_10fa37e0", "card manager shortcut", 0xFA37E0, 6,
      { 0x83, 0xEC, 0x10, 0x53, 0x33, 0xDB }, -1, true },                           // SUB ESP,0x10; PUSH EBX; XOR EBX,EBX
    // docs/re/prologues_groupfix_fix2.txt
    { "FUN_11223470", "group frame placed (fix: one card, single row)", 0x1223470, 5,
      { 0x83, 0xEC, 0x50, 0x53, 0x55 }, -1, false },                                // SUB ESP,0x50; PUSH EBX; PUSH EBP
    { "FUN_10f9bf20", "UI component state set", 0xF9BF20, 5,
      { 0x83, 0xEC, 0x18, 0x53, 0x55 }, -1, true },                                 // SUB ESP,0x18; PUSH EBX; PUSH EBP
    { "FUN_10f9c360", "UI component visible", 0xF9C360, 8,
      { 0x83, 0xEC, 0x0C, 0x53, 0x8B, 0x5C, 0x24, 0x14 }, -1, true },               // SUB ESP,0xC; PUSH EBX; MOV EBX,[ESP+0x14]
    { "FUN_11216b20", "group button constructed", 0x1216B20, 5,
      { 0x83, 0xEC, 0x30, 0x56, 0x57 }, -1, true },                                 // SUB ESP,0x30; PUSH ESI; PUSH EDI
};

static HookSlot g_slots[HOOK_COUNT] = {};
extern "C" uint8_t* g_groupTrampolines[HOOK_COUNT] = {};

// ---------------------------------------------------------------------------
// Engine layout (see the header)
// ---------------------------------------------------------------------------

static const uint32_t RVA_SELMGR = 0x01BBEAD8;              // [0x11bbead8] the selection manager
static const uint32_t RVA_BATTLE_UI = 0x01BC8180;           // [0x11bc8180] +0x5B4 the battle UI object
static const uint32_t UI_OFF_PANEL = 0x5B4, PANEL_OFF_N = 0x34;
static const uint32_t SEL_OFF_N = 0x28, SEL_OFF_LOCKMODE = 0x20, SEL_OFF_NGROUPS = 0x40;
static const uint32_t GRP_OFF_NA = 0x20, GRP_OFF_NB = 0x60;  // engine group: two unit lists (count, array at +4)
static const uint32_t SEL_OFF_GROUPS = 0x44, UIG_OFF_N = 0x0C, UIG_OFF_SLOT = 0x14;  // UI group entries
static const uint32_t RVA_VT_GROUP_BUTTON = 0x01725208;     // BattleButtonGroup vtable 0x11725208
static const uint32_t FRAMES_OFF_N = 0x70;                  // card panel: entries of its group frame map

static inline uint32_t R32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint32_t Rd(uint32_t addr) { uint32_t v = 0; ReadDword(addr, &v); return v; }
static inline uint8_t RdB(uint32_t addr) { uint8_t v = 0; ReadByte(addr, &v); return v; }
static inline uint32_t Rva(uint32_t a) { return a >= g_base && a < g_base + HOOK_ANALYSED_IMAGE_SIZE ? a - g_base : 0; }
static uint32_t Tick() { return GameTickAt(g_base); }
static uint32_t SelMgr() { return g_base ? Rd(g_base + RVA_SELMGR) : 0; }
static uint32_t SelCount(uint32_t mgr) { return PtrPlausible(mgr) ? Rd(mgr + SEL_OFF_N) : 0xFFFFFFFFu; }

// ---------------------------------------------------------------------------
// Fix state
// ---------------------------------------------------------------------------

static volatile LONG g_fixOn = 0;
static volatile LONG g_fCreated = 0, g_fSingles = 0;
static bool g_installed = false;
static bool g_keyWatched = false;
static volatile LONG g_testKeys = 0;
static uint8_t* g_toneOn = nullptr;
static uint8_t* g_toneOff = nullptr;
static uint8_t* g_toneRefused = nullptr;

// Writes the immediates of every fix site for `on`; false when a code page cannot be written
// (the sites already written are put back to the game's value).
static bool ApplyImm(bool on) {
    uint8_t v = on ? IMM_FIX : IMM_GAME, back = on ? IMM_GAME : IMM_FIX;
    for (int i = 0; i < FIX_SITE_COUNT; i++) {
        if (Hook_WriteCode((uint8_t*)(uintptr_t)(g_base + g_fixSites[i].rva + kImmOff[i]), &v, 1)) continue;
        for (int k = 0; k < i; k++)
            Hook_WriteCode((uint8_t*)(uintptr_t)(g_base + g_fixSites[k].rva + kImmOff[k]), &back, 1);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Probe file
// ---------------------------------------------------------------------------

static bool g_probe = false;
static char g_dir[MAX_PATH] = "";
static CRITICAL_SECTION g_cs;
static FILE* g_fEvents = nullptr;
static LARGE_INTEGER g_qpf = {};
static volatile LONG g_rows = 0;
static const LONG MAX_ROWS = 1000000;
static volatile LONG g_cToggle = 0, g_cUse = 0, g_cLock = 0, g_cFinal = 0, g_cRemove = 0, g_cAdd = 0, g_cDestroy = 0;
static volatile LONG g_cFrames = 0, g_cEvCreate = 0, g_cEvSel = 0, g_cBtnKey = 0, g_cCardKey = 0, g_cUiRows = 0;
static volatile LONG g_cState = 0, g_cVis = 0, g_cBtnCtor = 0;
static volatile LONG g_fFrames1 = 0;                 // frames of one-unit groups placed (fix on), single-row forced
// The UI components of the BattleButtonGroup objects (one per orders panel, land and naval).
static const int MAX_GROUP_COMPS = 8;
static volatile LONG g_groupComps[MAX_GROUP_COMPS] = {};
static void NoteGroupComp(uint32_t comp) {
    if (!comp) return;
    for (int i = 0; i < MAX_GROUP_COMPS; i++) {
        LONG prev = InterlockedCompareExchange(&g_groupComps[i], (LONG)comp, 0);
        if (prev == 0 || (uint32_t)prev == comp) return;
    }
}
static bool IsGroupComp(uint32_t comp) {
    if (!comp) return false;
    for (int i = 0; i < MAX_GROUP_COMPS; i++)
        if ((uint32_t)g_groupComps[i] == comp) return true;
    return false;
}

static uint32_t Ms() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return g_qpf.QuadPart ? (uint32_t)(now.QuadPart * 1000 / g_qpf.QuadPart) : GetTickCount();
}

static bool OpenProbe() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    char* slash = strrchr(exe, '\\');
    if (slash) slash[1] = 0;
    char root[MAX_PATH];
    snprintf(root, sizeof(root), "%sgroupfix_runs\\", exe);
    CreateDirectoryA(root, nullptr);
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(g_dir, sizeof(g_dir), "%s%04d%02d%02d_%02d%02d%02d\\", root, st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute, st.wSecond);
    if (!CreateDirectoryA(g_dir, nullptr)) { Log("groupfix: cannot create %s", g_dir); return false; }
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%sgroup_events.csv", g_dir);
    g_fEvents = fopen(path, "w");
    if (!g_fEvents) { Log("groupfix: cannot open %s", path); return false; }
    fputs("ms,tick,event,ret_rva,self,arg1,arg2,selected,count_a,count_b,fix,stack\n", g_fEvents);
    fflush(g_fEvents);
    Log("groupfix: probe on, writing %s", g_dir);
    return true;
}

// Module return addresses above a hook's frame (a call instruction right before each), as
// "rva/rva/...", at most `max`. `esp` = the stack pointer at the hook point.
static void StackTrace(uint32_t esp, char* out, int cap, int max) {
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

static void Row(const char* ev, uint32_t retRva, uint32_t esp, uint32_t self, uint32_t a1, uint32_t a2, uint32_t sel,
                uint32_t na, uint32_t nb) {
    if (!g_probe || InterlockedIncrement(&g_rows) >= MAX_ROWS) return;
    char stack[160];
    StackTrace(esp, stack, sizeof(stack), 8);
    EnterCriticalSection(&g_cs);
    if (g_fEvents) {
        fprintf(g_fEvents, "%u,%u,%s,%X,%X,%X,%X,%d,%d,%d,%d,%s\n", Ms(), Tick(), ev, retRva, self, a1, a2, (int)sel,
                (int)na, (int)nb, (int)g_fixOn, stack);
        fflush(g_fEvents);   // a handful of rows per minute: flush each, nothing lost on a crash
    }
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

// Entry hooks: ESP at the hook point is ESP_BEFORE_PUSHAD; the return address is at its +0.
static uint32_t EntryEsp(const uint8_t* ctx) { return R32(ctx + GROUP_OFF_ESP_BEFORE_PUSHAD); }

// FUN_10f89a40+0x9: the selection passed the count test; the group is made next.
static void HandleCreated(const uint8_t* ctx) {
    uint32_t mgr = R32(ctx + GROUPMID_OFF_ESI), slot = R32(ctx + GROUPMID_OFF_ARG1), n = SelCount(mgr);
    InterlockedIncrement(&g_fCreated);
    if (n == 1) {
        InterlockedIncrement(&g_fSingles);
        Log("groupfix: a group of one unit made from the selection (slot %d)", (int)slot);
    }
    Row("group_made", Rva(R32(ctx + GROUPMID_OFF_RET)), R32(ctx + GROUPMID_OFF_ESP_BEFORE_PUSHAD), mgr, slot, 0, n, 0, 0);
}

static void HandleToggle(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cToggle);
    uint32_t mgr = R32(ctx + GROUP_OFF_ECX);
    Row("key_G", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), mgr, RdB(mgr + SEL_OFF_LOCKMODE), Rd(mgr + SEL_OFF_NGROUPS),
        SelCount(mgr), 0, 0);
}

static void HandleUseGroup(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cUse);
    uint32_t mgr = R32(ctx + GROUP_OFF_ECX);
    Row("key_ctrl_number", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), mgr, R32(ctx + GROUP_OFF_ARG1),
        Rd(mgr + SEL_OFF_NGROUPS), SelCount(mgr), 0, 0);
}

static void HandleLock(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cLock);
    uint32_t panel = Rd(Rd(g_base + RVA_BATTLE_UI) + UI_OFF_PANEL);
    Row("lock_button", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), R32(ctx + GROUP_OFF_ECX), panel,
        PtrPlausible(panel) ? Rd(panel + PANEL_OFF_N) : 0, SelCount(SelMgr()), 0, 0);
}

static void GroupRow(const uint8_t* ctx, const char* ev, uint32_t a1) {
    uint32_t g = R32(ctx + GROUP_OFF_ECX);
    Row(ev, Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), g, a1, 0, SelCount(SelMgr()), Rd(g + GRP_OFF_NA),
        Rd(g + GRP_OFF_NB));
}

static void HandleFinalise(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cFinal);
    GroupRow(ctx, "engine_group_finalise", 0);
}

static void HandleRemove(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cRemove);
    GroupRow(ctx, "engine_unit_remove", R32(ctx + GROUP_OFF_ARG1));
}

static void HandleAdd(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cAdd);
    GroupRow(ctx, "engine_unit_add", R32(ctx + GROUP_OFF_ARG1));
}

static void HandleDestroy(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cDestroy);
    Row("engine_group_destroy", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), R32(ctx + GROUP_OFF_ECX),
        R32(ctx + GROUP_OFF_ARG1), 0, SelCount(SelMgr()), 0, 0);
}

static void HandleFrames(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cFrames);
    uint32_t panel = R32(ctx + GROUP_OFF_ECX);
    Row("ui_group_frames", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), panel, Rd(panel + FRAMES_OFF_N), 0,
        SelCount(SelMgr()), 0, 0);
}

static void HandleEvCreate(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cEvCreate);
    Row("ui_event_create_group", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), R32(ctx + GROUP_OFF_ECX),
        R32(ctx + GROUP_OFF_ARG1), R32(ctx + GROUP_OFF_ARG2), SelCount(SelMgr()), 0, 0);
}

static void HandleEvSel(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cEvSel);
    Row("ui_event_group_selection", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), R32(ctx + GROUP_OFF_ECX),
        R32(ctx + GROUP_OFF_ARG1), R32(ctx + GROUP_OFF_ARG2), SelCount(SelMgr()), 0, 0);
}

// Every battle button's shortcut passes here; only the group button (G) is logged. Its third
// argument decides whether the click runs.
static void HandleBtnKey(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t btn = R32(ctx + GROUP_OFF_ECX);
    if (Rd(btn) != g_base + RVA_VT_GROUP_BUTTON) return;
    InterlockedIncrement(&g_cBtnKey);
    NoteGroupComp(Rd(btn + 4));   // base class: the UI component at +4
    Row("key_G_button", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), btn, R32(ctx + GROUP_OFF_ARG1),
        R32(ctx + GROUP_OFF_ARG3) & 0xFF, SelCount(SelMgr()), R32(ctx + GROUP_OFF_ARG2), 0);
}

static void HandleCardKey(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cCardKey);
    Row("key_card_manager", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), R32(ctx + GROUP_OFF_ECX),
        R32(ctx + GROUP_OFF_ARG1), R32(ctx + GROUP_OFF_ARG2), SelCount(SelMgr()), 0, 0);
}

// FUN_11223470 (ECX = the group's frame, arg1 = the group's unit list {?, count, array}, arg2 =
// the two-row card layout flag): the frame of a one-unit group. With the fix on the count test
// lets it through (fix site F_FRAME); the two-row layout's odd-count branch reads a second card,
// so a one-unit group always takes the single-row branch (arg2 = 0), which sizes the frame from
// the first card and the count.
static void HandleFrameSize(uint8_t* ctx) {
    if (!InterlockedCompareExchange(&g_fixOn, 0, 0)) return;
    uint32_t list = R32(ctx + GROUP_OFF_ARG1);
    if (Rd(list + 4) != 1) return;
    uint32_t a2 = R32(ctx + GROUP_OFF_ARG2);
    if (a2 & 0xFF) {
        uint32_t esp = R32(ctx + GROUP_OFF_ESP_BEFORE_PUSHAD);   // the caller's argument slot: rewritten in place
        WriteDwordTry(esp + 8, a2 & ~0xFFu);
    }
    if (InterlockedIncrement(&g_fFrames1) == 1)
        Log("groupfix: first frame of a one-unit group placed (two-row flag was %u)", a2 & 0xFF);
}

static void StateName(uint32_t strObj, char* out, int cap) {
    out[0] = 0;
    uint32_t len = Rd(strObj), data = Rd(strObj + 8);
    if (!PtrPlausible(data) || len == 0 || len > 60) return;
    if ((int)len >= cap) len = cap - 1;
    if (!MemcpyTry(out, data, len)) { out[0] = 0; return; }
    out[len] = 0;
    for (uint32_t i = 0; i < len; i++) if (out[i] == ',' || out[i] < 0x20) out[i] = '_';
}

static void HandleSetState(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t comp = R32(ctx + GROUP_OFF_ECX);
    if (!IsGroupComp(comp)) return;
    InterlockedIncrement(&g_cState);
    char name[64];
    StateName(R32(ctx + GROUP_OFF_ARG1), name, sizeof(name));
    char ev[96];
    snprintf(ev, sizeof(ev), "group_button_state_%s", name[0] ? name : "unknown");
    Row(ev, Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), comp, R32(ctx + GROUP_OFF_ARG1), 0, SelCount(SelMgr()), 0, 0);
}

static void HandleSetVis(const uint8_t* ctx) {
    if (!g_probe) return;
    uint32_t comp = R32(ctx + GROUP_OFF_ECX);
    if (!IsGroupComp(comp)) return;
    InterlockedIncrement(&g_cVis);
    Row("group_button_visible", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), comp, R32(ctx + GROUP_OFF_ARG1) & 0xFF, 0,
        SelCount(SelMgr()), 0, 0);
}

static void HandleBtnCtor(const uint8_t* ctx) {
    if (!g_probe) return;
    InterlockedIncrement(&g_cBtnCtor);
    NoteGroupComp(R32(ctx + GROUP_OFF_ARG1));
    Row("group_button_created", Rva(R32(ctx + GROUP_OFF_RET)), EntryEsp(ctx), R32(ctx + GROUP_OFF_ECX),
        R32(ctx + GROUP_OFF_ARG1), 0, SelCount(SelMgr()), 0, 0);
}

// ---------------------------------------------------------------------------
// Sampler (probe): the selection manager's unit groups, logged on every change
// ---------------------------------------------------------------------------

static HANDLE g_sampler = nullptr;
static volatile LONG g_stop = 0;

static void SnapUiGroups() {
    static char last[512] = "";
    char now[512];
    uint32_t mgr = SelMgr();
    if (!PtrPlausible(mgr)) {
        now[0] = 0;
    } else {
        uint32_t n = Rd(mgr + SEL_OFF_NGROUPS), arr = Rd(mgr + SEL_OFF_GROUPS);
        int k = snprintf(now, sizeof(now), "sel=%d groups=%d", (int)Rd(mgr + SEL_OFF_N), (int)n);
        if (n <= 64 && (n == 0 || PtrPlausible(arr)))
            for (uint32_t i = 0; i < n && k < (int)sizeof(now) - 40; i++) {
                uint32_t g = Rd(arr + 4 * i);
                k += snprintf(now + k, sizeof(now) - k, " [%X n%d s%d]", g, (int)Rd(g + UIG_OFF_N), (int)Rd(g + UIG_OFF_SLOT));
            }
    }
    if (strcmp(now, last) == 0) return;
    strcpy(last, now);
    if (!g_probe || InterlockedIncrement(&g_rows) >= MAX_ROWS) return;
    InterlockedIncrement(&g_cUiRows);
    EnterCriticalSection(&g_cs);
    if (g_fEvents) {
        fprintf(g_fEvents, "%u,%u,ui_groups,0,%X,0,0,%d,0,0,%d,%s\n", Ms(), Tick(), mgr, (int)SelCount(mgr), (int)g_fixOn,
                now);
        fflush(g_fEvents);
    }
    LeaveCriticalSection(&g_cs);
}

static DWORD WINAPI SamplerThread(LPVOID) {
    while (!InterlockedCompareExchange(&g_stop, 0, 0)) {
        Sleep(50);
        SnapUiGroups();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Key, detours, harness exports
// ---------------------------------------------------------------------------

// I toggles the fix (dev build; focus only through proxy/keys). Harness: GroupFixTestKeys bit 0.
extern "C" __declspec(dllexport) void __stdcall GroupFixTestKeys(LONG bits) { InterlockedExchange(&g_testKeys, bits); }

static void ToggleKey() {
    bool on = InterlockedCompareExchange(&g_fixOn, 0, 0) == 0;
    if (!ApplyImm(on)) {
        Log("groupfix: I, cannot write the code page; fix stays %s", on ? "OFF" : "ON");
        Tone_Play(g_toneRefused);
        return;
    }
    InterlockedExchange(&g_fixOn, on ? 1 : 0);
    Log("groupfix: I, fix %s", on ? "ON (a single unit can be grouped)" : "OFF (the game's two-unit minimum)");
    Tone_Play(on ? g_toneOn : g_toneOff);
}

extern "C" void __stdcall GroupHandler0(const uint8_t* ctx) { HandleCreated(ctx); }
extern "C" void __stdcall GroupHandler1(const uint8_t* ctx) { HandleToggle(ctx); }
extern "C" void __stdcall GroupHandler2(const uint8_t* ctx) { HandleUseGroup(ctx); }
extern "C" void __stdcall GroupHandler3(const uint8_t* ctx) { HandleLock(ctx); }
extern "C" void __stdcall GroupHandler4(const uint8_t* ctx) { HandleFinalise(ctx); }
extern "C" void __stdcall GroupHandler5(const uint8_t* ctx) { HandleRemove(ctx); }
extern "C" void __stdcall GroupHandler6(const uint8_t* ctx) { HandleAdd(ctx); }
extern "C" void __stdcall GroupHandler7(const uint8_t* ctx) { HandleDestroy(ctx); }
extern "C" void __stdcall GroupHandler8(const uint8_t* ctx) { HandleFrames(ctx); }
extern "C" void __stdcall GroupHandler9(const uint8_t* ctx) { HandleEvCreate(ctx); }
extern "C" void __stdcall GroupHandler10(const uint8_t* ctx) { HandleEvSel(ctx); }
extern "C" void __stdcall GroupHandler11(const uint8_t* ctx) { HandleBtnKey(ctx); }
extern "C" void __stdcall GroupHandler12(const uint8_t* ctx) { HandleCardKey(ctx); }
extern "C" void __stdcall GroupHandler13(uint8_t* ctx) { HandleFrameSize(ctx); }
extern "C" void __stdcall GroupHandler14(const uint8_t* ctx) { HandleSetState(ctx); }
extern "C" void __stdcall GroupHandler15(const uint8_t* ctx) { HandleSetVis(ctx); }
extern "C" void __stdcall GroupHandler16(const uint8_t* ctx) { HandleBtnCtor(ctx); }

HOOK_DETOUR(GroupDetour0, GroupHandler0, g_groupTrampolines, 0)
HOOK_DETOUR(GroupDetour1, GroupHandler1, g_groupTrampolines, 1)
HOOK_DETOUR(GroupDetour2, GroupHandler2, g_groupTrampolines, 2)
HOOK_DETOUR(GroupDetour3, GroupHandler3, g_groupTrampolines, 3)
HOOK_DETOUR(GroupDetour4, GroupHandler4, g_groupTrampolines, 4)
HOOK_DETOUR(GroupDetour5, GroupHandler5, g_groupTrampolines, 5)
HOOK_DETOUR(GroupDetour6, GroupHandler6, g_groupTrampolines, 6)
HOOK_DETOUR(GroupDetour7, GroupHandler7, g_groupTrampolines, 7)
HOOK_DETOUR(GroupDetour8, GroupHandler8, g_groupTrampolines, 8)
HOOK_DETOUR(GroupDetour9, GroupHandler9, g_groupTrampolines, 9)
HOOK_DETOUR(GroupDetour10, GroupHandler10, g_groupTrampolines, 10)
HOOK_DETOUR(GroupDetour11, GroupHandler11, g_groupTrampolines, 11)
HOOK_DETOUR(GroupDetour12, GroupHandler12, g_groupTrampolines, 12)
HOOK_DETOUR(GroupDetour13, GroupHandler13, g_groupTrampolines, 13)
HOOK_DETOUR(GroupDetour14, GroupHandler14, g_groupTrampolines, 14)
HOOK_DETOUR(GroupDetour15, GroupHandler15, g_groupTrampolines, 15)
HOOK_DETOUR(GroupDetour16, GroupHandler16, g_groupTrampolines, 16)

static const HookDetour g_detours[HOOK_COUNT] = { GroupDetour0, GroupDetour1, GroupDetour2, GroupDetour3,
                                                  GroupDetour4, GroupDetour5, GroupDetour6, GroupDetour7,
                                                  GroupDetour8, GroupDetour9, GroupDetour10, GroupDetour11,
                                                  GroupDetour12, GroupDetour13, GroupDetour14, GroupDetour15,
                                                  GroupDetour16 };

// Harness only (fixes/single-unit-group-fix/tests): fix and probe counters.
extern "C" __declspec(dllexport) void __stdcall GroupFixTestCounters(long* out) {
    out[0] = g_probe ? 1 : 0;
    out[1] = g_fixOn;
    out[2] = g_installed ? 1 : 0;
    out[3] = g_fCreated;
    out[4] = g_fSingles;
    out[5] = g_cToggle;
    out[6] = g_cUse;
    out[7] = g_cLock;
    out[8] = g_cFinal;
    out[9] = g_cRemove;
    out[10] = g_cAdd;
    out[11] = g_cDestroy;
    out[12] = g_rows;
    out[13] = g_cFrames;
    out[14] = g_cEvCreate;
    out[15] = g_cEvSel;
    out[16] = g_cBtnKey;
    out[17] = g_cCardKey;
    out[18] = g_cUiRows;
    out[19] = g_fFrames1;
    out[20] = g_cState;
    out[21] = g_cVis;
    out[22] = g_cBtnCtor;
}

// ---------------------------------------------------------------------------
// Install / remove
// ---------------------------------------------------------------------------

static bool EnvIs(const char* name, char c) {
    char v[8] = "";
    return GetEnvironmentVariableA(name, v, sizeof(v)) > 0 && v[0] == c;
}

void GroupFix_Install() {
    Log("groupfix: single unit group fix (dev build)");
    Mem_Init();
    static const ToneSeg on[] = { { 880, 160 } };
    static const ToneSeg off[] = { { 330, 300 } };
    static const ToneSeg refused[] = { { 200, 450 }, { 0, 150 }, { 200, 450 }, { 0, 150 }, { 200, 450 } };
    g_toneOn = Tone_Make(on, 1);
    g_toneOff = Tone_Make(off, 1);
    g_toneRefused = Tone_Make(refused, 5);
    uint32_t size = 0;
    uint8_t* base = Hook_GameModule(false, &size);
    if (!base) { Log("groupfix: REFUSED, empire.retail.dll is not loaded"); Tone_Play(g_toneRefused); return; }
    if (size != HOOK_ANALYSED_IMAGE_SIZE) {
        Log("groupfix: REFUSED, SizeOfImage 0x%08X is not the analysed build", size);
        Tone_Play(g_toneRefused);
        return;
    }
    for (int i = 0; i < FIX_SITE_COUNT; i++) {
        if (!Hook_SiteMatches(base, g_fixSites[i])) {
            Log("groupfix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", g_fixSites[i].rva,
                g_fixSites[i].func);
            Tone_Play(g_toneRefused);
            return;
        }
    }
    for (int h = 0; h < HOOK_COUNT; h++) {
        if (!Hook_SiteMatches(base, g_hookDefs[h])) {
            Log("groupfix: REFUSED, bytes at RVA 0x%06X (%s) differ from the analysed binary", g_hookDefs[h].rva,
                g_hookDefs[h].func);
            Tone_Play(g_toneRefused);
            return;
        }
    }
    g_base = (uint32_t)(uintptr_t)base;
    InitializeCriticalSection(&g_cs);
    QueryPerformanceFrequency(&g_qpf);
#ifdef GROUPFIX_START_OFF
    bool fixOff = !EnvIs("GROUPFIX_ON", '1');      // live-check build: off at launch, I turns it on
    const char* offWhy = "OFF at launch (build option GROUPFIX_START_OFF)";
#else
    bool fixOff = EnvIs("GROUPFIX_OFF", '1');
    const char* offWhy = "OFF (GROUPFIX_OFF=1)";
#endif
#ifdef GROUPFIX_PROBE_DEFAULT
    bool wantProbe = !EnvIs("GROUPFIX_PROBE", '0');   // live-check build: probe on unless GROUPFIX_PROBE=0
#else
    bool wantProbe = EnvIs("GROUPFIX_PROBE", '1');
#endif
    if (wantProbe) {
        if (OpenProbe()) g_probe = true;
        else Log("groupfix: probe not started (no run folder)");
    }
    bool use[HOOK_COUNT];
    for (int h = 0; h < HOOK_COUNT; h++) use[h] = !g_hookDefs[h].probeOnly || g_probe;
    if (!Hook_Prepare(base, g_hookDefs, HOOK_COUNT, use, g_groupTrampolines, g_slots)) {
        Log("groupfix: REFUSED, no trampoline memory");
        Tone_Play(g_toneRefused);
        g_probe = false;
        g_base = 0;
        return;
    }
    if (!fixOff && !ApplyImm(true)) {
        Log("groupfix: cannot write the code pages of the fix sites; fix OFF");
        Tone_Play(g_toneRefused);
        fixOff = true;
        offWhy = "OFF (code page not writable)";
    }
    InterlockedExchange(&g_fixOn, fixOff ? 0 : 1);
    g_installed = true;
    if (g_probe) g_sampler = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    Hook_Patch(g_detours, g_slots, HOOK_COUNT);
    for (int h = 0; h < HOOK_COUNT; h++)
        if (g_slots[h].active)
            Log("groupfix: hook %s (%s) installed at RVA 0x%06X", g_hookDefs[h].func, g_hookDefs[h].role, g_hookDefs[h].rva);
    Log("groupfix: fix %s: G, Ctrl+G and Ctrl+number accept a single unit, its group frame is shown", fixOff ? offWhy : "ON");
    g_keyWatched = Keys_Watch('I', &g_testKeys, 1, ToggleKey);
    Log("groupfix: I toggles the fix on / off%s", g_keyWatched ? "" : " (ERROR: key poll not started)");
}

void GroupFix_Remove() {
    if (g_keyWatched) { Keys_Unwatch(ToggleKey); g_keyWatched = false; }
    Hook_Remove(g_slots, HOOK_COUNT);
    if (!g_base) return;
    if (g_installed) ApplyImm(false);    // the game's own immediate back
    g_installed = false;
    Log("groupfix: groups made from the selection %ld, of them single units %ld; frames of one-unit groups placed %ld",
        g_fCreated, g_fSingles, g_fFrames1);
    if (g_sampler) {
        InterlockedExchange(&g_stop, 1);
        WaitForSingleObject(g_sampler, 2000);
        CloseHandle(g_sampler);
        g_sampler = nullptr;
    }
    if (g_probe) {
        EnterCriticalSection(&g_cs);
        if (g_fEvents) { fclose(g_fEvents); g_fEvents = nullptr; }
        g_probe = false;
        LeaveCriticalSection(&g_cs);
        Log("groupfix: probe closed: G %ld, Ctrl+number %ld, lock button %ld, engine finalise %ld, unit add %ld, "
            "unit remove %ld, group destroy %ld, frames %ld, UI create %ld, UI group selection %ld, "
            "G button key %ld, card manager key %ld, UI group rows %ld, group button: created %ld, states %ld, "
            "visible %ld; rows %ld",
            g_cToggle, g_cUse, g_cLock, g_cFinal, g_cAdd, g_cRemove, g_cDestroy, g_cFrames, g_cEvCreate, g_cEvSel,
            g_cBtnKey, g_cCardKey, g_cUiRows, g_cBtnCtor, g_cState, g_cVis, g_rows);
    }
}
