// autorun.cpp -- in-game side of the unattended replay runner (tools/autorun).
//
// Two threads, no code patches, no exception filters, no keyboard:
//   watcher  every 250 ms: find the game window, read the battle state through a fixed pointer
//            path (game_offsets.h, guarded reads), keep the window minimized when asked, queue
//            "event ..." lines on every change and a heartbeat every 5 s.
//   pipe     server of \\.\pipe\s2autorun_<pid> (one client at a time, byte mode, UTF-8 lines
//            ending in \n): answers commands and forwards the queued events.
//
// Commands (one per line)        Reply
//   ping                         ok ping
//   state                        ok state <STATE> tick=<n> phase=<n> iconic=<0|1> hwnd=0x...
//   minimize                     ok minimize     (ShowWindowAsync SW_SHOWMINNOACTIVE, once)
//   keep-minimized on|off        ok keep-minimized on|off
//   restore                      ok restore      (keep-minimized off, SW_SHOWNOACTIVATE)
//   close                        ok close        (posts WM_CLOSE to the game window)
//   cmd <console line>           err cmd ...     (no console executor in this build)
// Events: event hello pid=<n> hwnd=0x... state=<STATE> build=<id>
//         event state <STATE> tick=<n>       (STARTING, FRONTEND, LOADING, BATTLE, BATTLE_OVER, EXITING)
//         event phase <n> tick=<n>           (battle phase changed; decided = 7, 9, 10, 11, >= 12)
//         event stalled tick=<n> secs=<s>    (battle tick unchanged for 30 s, once per stall)
//         event minimized | event heartbeat state=<STATE> tick=<n> phase=<n> idle=<0|1> iconic=<0|1>
//
// Environment: AUTORUN_MINIMIZE=1 keeps the game window minimized (re-minimizes it whenever the
// game restores it, at most once a second). Everything sent and received is also logged to
// <game>\autorun_log.txt.

#include "autorun.h"
#include "game_offsets.h"
#include "log.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

enum State { ST_STARTING, ST_FRONTEND, ST_LOADING, ST_BATTLE, ST_BATTLE_OVER, ST_EXITING };

const char* StateName(int s) {
    switch (s) {
        case ST_STARTING:    return "STARTING";
        case ST_FRONTEND:    return "FRONTEND";
        case ST_LOADING:     return "LOADING";
        case ST_BATTLE:      return "BATTLE";
        case ST_BATTLE_OVER: return "BATTLE_OVER";
        case ST_EXITING:     return "EXITING";
    }
    return "?";
}

const DWORD POLL_MS = 250;
const DWORD HEARTBEAT_MS = 5000;
const DWORD STALL_MS = 30000;
const DWORD MINIMIZE_GAP_MS = 1000;

CRITICAL_SECTION g_cs;
DWORD g_pid = 0;

// Shared snapshot (g_cs).
struct Snapshot {
    int state;
    HWND hwnd;
    bool iconic;
    uint32_t tick;
    int phase;
};
Snapshot g_snap = {ST_STARTING, nullptr, false, 0, -1};
volatile LONG g_keepMinimized = 0;
volatile LONG g_closing = 0;

// Outgoing event lines (g_cs); dropped when no client is connected, oldest dropped when full.
const int QUEUE_LINES = 128;
const int LINE_CAP = 256;
char g_queue[QUEUE_LINES][LINE_CAP];
int g_qHead = 0, g_qCount = 0;
volatile LONG g_clientConnected = 0;

void Emit(const char* fmt, ...) {
    char line[LINE_CAP];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    line[sizeof(line) - 1] = 0;
    Log("pipe> %s", line);
    if (!g_clientConnected) return;
    EnterCriticalSection(&g_cs);
    if (g_qCount == QUEUE_LINES) {   // drop the oldest
        g_qHead = (g_qHead + 1) % QUEUE_LINES;
        g_qCount--;
    }
    strcpy(g_queue[(g_qHead + g_qCount) % QUEUE_LINES], line);
    g_qCount++;
    LeaveCriticalSection(&g_cs);
}

bool PopEvent(char* out) {
    bool got = false;
    EnterCriticalSection(&g_cs);
    if (g_qCount > 0) {
        strcpy(out, g_queue[g_qHead]);
        g_qHead = (g_qHead + 1) % QUEUE_LINES;
        g_qCount--;
        got = true;
    }
    LeaveCriticalSection(&g_cs);
    return got;
}

void ClearEvents() {
    EnterCriticalSection(&g_cs);
    g_qHead = g_qCount = 0;
    LeaveCriticalSection(&g_cs);
}

Snapshot GetSnap() {
    EnterCriticalSection(&g_cs);
    Snapshot s = g_snap;
    LeaveCriticalSection(&g_cs);
    return s;
}

// --- guarded reads: ReadProcessMemory on our own process never faults ------------------------

bool ReadMem(uintptr_t addr, void* out, size_t n) {
    if (addr < 0x10000) return false;
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, out, n, &got) && got == n;
}

bool Read32(uintptr_t addr, uint32_t* v) { return ReadMem(addr, v, 4); }

// Returns true and fills tick / phase / idle when the whole chain is valid (owner vtable matches,
// phase in range).
bool ReadBattle(uintptr_t base, uint32_t* tick, int* phase, int* idle) {
    uint32_t shell = 0, mode = 0, owner = 0, vt = 0, tobj = 0, sm = 0, ph = 0;
    uint8_t id = 0;
    if (!Read32(base + AR_SHELL_GLOBAL_RVA, &shell) || !shell) return false;
    if (!Read32(shell + AR_SHELL_OFF_MODE, &mode) || !mode) return false;
    if (!Read32(mode + AR_MODE_OFF_OWNER, &owner) || !owner) return false;
    if (!Read32(owner, &vt) || vt != base + AR_OWNER_VTABLE_RVA) return false;
    if (!Read32(owner + AR_OWNER_OFF_TICKOBJ, &tobj) || !tobj) return false;
    if (!Read32(tobj + AR_TICK_OFF_TICK, tick)) return false;
    if (!ReadMem(tobj + AR_TICK_OFF_IDLE, &id, 1)) return false;
    if (!Read32(tobj + AR_TICK_OFF_STATEOBJ, &sm) || !sm) return false;
    if (!Read32(sm + AR_STATE_OFF_PHASE, &ph) || ph > AR_PHASE_MAX) return false;
    *phase = (int)ph;
    *idle = id ? 1 : 0;
    return true;
}

// --- game window -----------------------------------------------------------------------------

struct FindCtx { DWORD pid; HWND found; };

BOOL CALLBACK FindProc(HWND h, LPARAM lp) {
    FindCtx* c = (FindCtx*)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != c->pid) return TRUE;
    char cls[64] = "";
    GetClassNameA(h, cls, sizeof(cls));
    if (strcmp(cls, "SHOGUN 2") != 0) return TRUE;
    c->found = h;
    return FALSE;
}

HWND FindGameWindow() {
    FindCtx c = {g_pid, nullptr};
    EnumWindows(FindProc, (LPARAM)&c);
    return c.found;
}

// --- module check ----------------------------------------------------------------------------

bool ModuleSupported(uintptr_t base) {
    uint32_t peOff = 0, size = 0;
    if (!Read32(base + 0x3C, &peOff)) return false;
    if (!Read32(base + peOff + 0x50, &size)) return false;   // OptionalHeader.SizeOfImage
    Log("empire.retail.dll at 0x%08X, SizeOfImage 0x%08X (supported 0x%08X)", (unsigned)base, size,
        AR_IMAGE_SIZE);
    return size == AR_IMAGE_SIZE;
}

// --- watcher thread --------------------------------------------------------------------------

DWORD WINAPI WatcherThread(LPVOID) {
    uintptr_t base = 0;
    bool supported = false;
    HWND hwnd = nullptr;
    int state = ST_STARTING;
    uint32_t lastTick = 0;
    DWORD lastTickChange = GetTickCount(), lastHeartbeat = GetTickCount(), lastMinimize = 0;
    bool stallReported = false;
    bool chainEverValid = false;
    int lastPhase = -1;
    int decidedPolls = 0;   // consecutive polls with a decided phase (BATTLE_OVER needs 2)

    for (;;) {
        DWORD now = GetTickCount();
        if (!base) {
            base = (uintptr_t)GetModuleHandleA("empire.retail.dll");
            if (base) {
                supported = ModuleSupported(base);
                if (!supported)
                    Log("unsupported empire.retail.dll build: battle state is not read (state stays FRONTEND)");
            }
        }
        if (!hwnd || !IsWindow(hwnd)) {
            HWND h = FindGameWindow();
            if (h != hwnd) {
                hwnd = h;
                if (hwnd) Log("game window 0x%p found", hwnd);
            }
        }

        uint32_t tick = 0;
        int phase = -1, idle = 0;
        bool battle = supported && ReadBattle(base, &tick, &phase, &idle);
        if (battle && !chainEverValid) {
            chainEverValid = true;
            Log("battle chain valid: tick %u, phase %d, idle byte %d", tick, phase, idle);
        }
        if (battle && phase != lastPhase) {
            Emit("event phase %d tick=%u", phase, tick);
            lastPhase = phase;
        }
        if (!battle) lastPhase = -1;
        decidedPolls = (battle && AR_PHASE_DECIDED(phase)) ? decidedPolls + 1 : 0;

        int next = state;
        if (g_closing) next = ST_EXITING;
        else if (!hwnd) next = ST_STARTING;
        else if (!battle) next = ST_FRONTEND;
        // The battle objects exist about 20 s before the clock starts (tick 0 while loading).
        else if (decidedPolls >= 2) next = ST_BATTLE_OVER;
        else if (decidedPolls == 1) next = state;          // wait for the second reading
        else next = tick == 0 ? ST_LOADING : ST_BATTLE;

        bool iconic = hwnd && IsIconic(hwnd);
        EnterCriticalSection(&g_cs);
        g_snap.state = next;
        g_snap.hwnd = hwnd;
        g_snap.iconic = iconic;
        g_snap.tick = tick;
        g_snap.phase = battle ? phase : -1;
        LeaveCriticalSection(&g_cs);

        if (next != state) {
            Emit("event state %s tick=%u", StateName(next), tick);
            state = next;
            lastTickChange = now;
            stallReported = false;
        }

        if (state == ST_BATTLE) {
            if (tick != lastTick) {
                lastTick = tick;
                lastTickChange = now;
                stallReported = false;
            } else if (!stallReported && now - lastTickChange >= STALL_MS) {
                stallReported = true;
                Emit("event stalled tick=%u secs=%lu", tick, (now - lastTickChange) / 1000);
            }
        }

        if (g_keepMinimized && hwnd && !iconic && !g_closing && now - lastMinimize >= MINIMIZE_GAP_MS) {
            lastMinimize = now;
            ShowWindowAsync(hwnd, SW_SHOWMINNOACTIVE);
            Emit("event minimized");
        }

        if (now - lastHeartbeat >= HEARTBEAT_MS) {
            lastHeartbeat = now;
            Emit("event heartbeat state=%s tick=%u phase=%d idle=%d iconic=%d", StateName(state), tick,
                 battle ? phase : -1, idle, iconic ? 1 : 0);
        }
        Sleep(POLL_MS);
    }
}

// --- pipe thread -----------------------------------------------------------------------------

bool WriteLine(HANDLE pipe, const char* line) {
    char buf[LINE_CAP + 2];
    int n = _snprintf(buf, sizeof(buf) - 1, "%s\n", line);
    if (n < 0) n = (int)sizeof(buf) - 1;
    DWORD wrote = 0;
    return WriteFile(pipe, buf, (DWORD)n, &wrote, nullptr) && wrote == (DWORD)n;
}

bool Reply(HANDLE pipe, const char* fmt, ...) {
    char line[LINE_CAP];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    line[sizeof(line) - 1] = 0;
    Log("pipe> %s", line);
    return WriteLine(pipe, line);
}

bool HandleCommand(HANDLE pipe, char* line) {
    size_t len = strlen(line);
    while (len && (line[len - 1] == '\r' || line[len - 1] == ' ')) line[--len] = 0;
    if (!len) return true;
    Log("pipe< %s", line);
    Snapshot s = GetSnap();
    if (strcmp(line, "ping") == 0) return Reply(pipe, "ok ping");
    if (strcmp(line, "state") == 0)
        return Reply(pipe, "ok state %s tick=%u phase=%d iconic=%d hwnd=0x%p", StateName(s.state), s.tick, s.phase,
                     s.iconic ? 1 : 0, s.hwnd);
    if (strcmp(line, "minimize") == 0) {
        if (!s.hwnd) return Reply(pipe, "err minimize no game window yet");
        ShowWindowAsync(s.hwnd, SW_SHOWMINNOACTIVE);
        return Reply(pipe, "ok minimize");
    }
    if (strcmp(line, "keep-minimized on") == 0) {
        InterlockedExchange(&g_keepMinimized, 1);
        return Reply(pipe, "ok keep-minimized on");
    }
    if (strcmp(line, "keep-minimized off") == 0) {
        InterlockedExchange(&g_keepMinimized, 0);
        return Reply(pipe, "ok keep-minimized off");
    }
    if (strcmp(line, "restore") == 0) {
        InterlockedExchange(&g_keepMinimized, 0);
        if (!s.hwnd) return Reply(pipe, "err restore no game window yet");
        ShowWindowAsync(s.hwnd, SW_SHOWNOACTIVATE);
        return Reply(pipe, "ok restore");
    }
    if (strcmp(line, "close") == 0) {
        if (!s.hwnd) return Reply(pipe, "err close no game window yet");
        InterlockedExchange(&g_closing, 1);
        PostMessageA(s.hwnd, WM_CLOSE, 0, 0);
        return Reply(pipe, "ok close");
    }
    if (strncmp(line, "cmd ", 4) == 0 || strcmp(line, "cmd") == 0)
        return Reply(pipe, "err cmd console commands are not supported by this build "
                           "(put startup commands into user.script.txt)");
    char verb[32] = "";
    sscanf(line, "%31s", verb);
    return Reply(pipe, "err %s unknown command", verb);
}

DWORD WINAPI PipeThread(LPVOID) {
    char name[64];
    _snprintf(name, sizeof(name) - 1, "\\\\.\\pipe\\s2autorun_%lu", g_pid);
    name[sizeof(name) - 1] = 0;
    Log("pipe server %s", name);
    for (;;) {
        HANDLE pipe = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       1, 4096, 4096, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            Log("CreateNamedPipe failed (error %lu), retrying in 2 s", GetLastError());
            Sleep(2000);
            continue;
        }
        BOOL ok = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!ok) {
            CloseHandle(pipe);
            continue;
        }
        ClearEvents();
        InterlockedExchange(&g_clientConnected, 1);
        Log("pipe client connected");
        Snapshot s = GetSnap();
        bool alive = Reply(pipe, "event hello pid=%lu hwnd=0x%p state=%s build=%s", g_pid, s.hwnd,
                           StateName(s.state), __DATE__ " " __TIME__);

        char in[1024];
        size_t inLen = 0;
        while (alive) {
            DWORD avail = 0;
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &avail, nullptr)) break;
            if (avail) {
                DWORD got = 0;
                DWORD want = (DWORD)(sizeof(in) - 1 - inLen);
                if (want == 0) inLen = 0, want = sizeof(in) - 1;   // overlong line: drop it
                if (!ReadFile(pipe, in + inLen, avail < want ? avail : want, &got, nullptr)) break;
                inLen += got;
                in[inLen] = 0;
                char* nl;
                while (alive && (nl = strchr(in, '\n')) != nullptr) {
                    *nl = 0;
                    alive = HandleCommand(pipe, in);
                    size_t rest = inLen - (size_t)(nl + 1 - in);
                    memmove(in, nl + 1, rest + 1);
                    inLen = rest;
                }
            }
            char line[LINE_CAP];
            while (alive && PopEvent(line)) alive = WriteLine(pipe, line);
            if (alive) Sleep(50);
        }
        InterlockedExchange(&g_clientConnected, 0);
        Log("pipe client gone");
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

}  // namespace

bool Autorun_IsGameProcess() {
    char exe[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    const char* base = strrchr(exe, '\\');
    base = base ? base + 1 : exe;
    return _stricmp(base, "shogun2.exe") == 0 || _stricmp(base, "shogun2.retail.exe") == 0;
}

void Autorun_Start(HMODULE) {
    InitializeCriticalSection(&g_cs);
    g_pid = GetCurrentProcessId();
    char env[8] = "";
    if (GetEnvironmentVariableA("AUTORUN_MINIMIZE", env, sizeof(env)) > 0 && strcmp(env, "1") == 0)
        g_keepMinimized = 1;
    Log("autorun: pid %lu, keep minimized %s (AUTORUN_MINIMIZE), build %s %s", g_pid, g_keepMinimized ? "ON" : "OFF",
        __DATE__, __TIME__);
    HANDLE t1 = CreateThread(nullptr, 0, WatcherThread, nullptr, 0, nullptr);
    HANDLE t2 = CreateThread(nullptr, 0, PipeThread, nullptr, 0, nullptr);
    if (!t1 || !t2) Log("autorun: CreateThread failed (error %lu)", GetLastError());
    if (t1) CloseHandle(t1);
    if (t2) CloseHandle(t2);
}
