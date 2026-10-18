// keys.cpp -- dev-build keys and tones (see keys.h).

#include "keys.h"
#include <mmsystem.h>
#include <cmath>
#include <cstring>
#pragma comment(lib, "winmm.lib")

bool Keys_GameHasFocus() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

bool Keys_RealDown(int vk) {
    return vk && (GetAsyncKeyState(vk) & 0x8000) != 0 && Keys_GameHasFocus();
}

bool Keys_Edge(bool now, bool* down) {
    bool edge = now && !*down;
    *down = now;
    return edge;
}

// ---------------------------------------------------------------------------
// Shared poll thread
// ---------------------------------------------------------------------------

struct KeyWatch {
    int vk;
    volatile LONG* testBits;
    LONG testMask;
    KeyPressFn onPress;
    bool down;
};

static const int MAX_WATCH = 16;
static KeyWatch g_watch[MAX_WATCH];
static volatile LONG g_watchCount = 0;
static CRITICAL_SECTION g_cs;
static volatile LONG g_csInit = 0;
static HANDLE g_thread = nullptr;
static volatile LONG g_quit = 0;

static void EnsureLock() {
    // first caller initialises; fixes register from the install thread, so no race in practice
    if (InterlockedCompareExchange(&g_csInit, 1, 0) == 0) InitializeCriticalSection(&g_cs);
}

static DWORD WINAPI PollThread(LPVOID) {
    while (!g_quit) {
        Sleep(50);
        KeyPressFn fire[MAX_WATCH];
        int nFire = 0;
        EnterCriticalSection(&g_cs);
        for (int i = 0; i < g_watchCount; i++) {
            KeyWatch& w = g_watch[i];
            bool now = (w.testBits && (*w.testBits & w.testMask) != 0) || Keys_RealDown(w.vk);
            if (Keys_Edge(now, &w.down)) fire[nFire++] = w.onPress;
        }
        LeaveCriticalSection(&g_cs);
        for (int i = 0; i < nFire; i++) fire[i]();   // outside the lock: a callback may log or beep
    }
    return 0;
}

bool Keys_Watch(int vk, volatile LONG* testBits, LONG testMask, KeyPressFn onPress) {
    EnsureLock();
    EnterCriticalSection(&g_cs);
    bool ok = g_watchCount < MAX_WATCH;
    if (ok) {
        KeyWatch& w = g_watch[g_watchCount];
        w.vk = vk; w.testBits = testBits; w.testMask = testMask; w.onPress = onPress; w.down = false;
        InterlockedIncrement(&g_watchCount);
    }
    LeaveCriticalSection(&g_cs);
    if (ok && !g_thread) {
        InterlockedExchange(&g_quit, 0);
        g_thread = CreateThread(nullptr, 0, PollThread, nullptr, 0, nullptr);
        if (!g_thread) return false;
    }
    return ok;
}

void Keys_Unwatch(KeyPressFn onPress) {
    if (!g_csInit) return;
    EnterCriticalSection(&g_cs);
    int j = 0;
    for (int i = 0; i < g_watchCount; i++)
        if (g_watch[i].onPress != onPress) g_watch[j++] = g_watch[i];
    InterlockedExchange(&g_watchCount, j);
    LeaveCriticalSection(&g_cs);
    if (j == 0 && g_thread) {
        InterlockedExchange(&g_quit, 1);
        WaitForSingleObject(g_thread, 1000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Tones
// ---------------------------------------------------------------------------

uint8_t* Tone_Make(const ToneSeg* seg, int n) {
    const int rate = 22050;
    int samples = 0;
    for (int i = 0; i < n; i++) samples += rate * seg[i].ms / 1000;
    uint32_t dataBytes = (uint32_t)samples * 2;
    uint8_t* w = (uint8_t*)VirtualAlloc(nullptr, 44 + dataBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!w) return nullptr;
    auto put32 = [&](int off, uint32_t v) { memcpy(w + off, &v, 4); };
    auto put16 = [&](int off, uint16_t v) { memcpy(w + off, &v, 2); };
    memcpy(w, "RIFF", 4); put32(4, 36 + dataBytes); memcpy(w + 8, "WAVE", 4);
    memcpy(w + 12, "fmt ", 4); put32(16, 16); put16(20, 1); put16(22, 1);
    put32(24, rate); put32(28, rate * 2); put16(32, 2); put16(34, 16);
    memcpy(w + 36, "data", 4); put32(40, dataBytes);
    int16_t* pcm = (int16_t*)(w + 44);
    int at = 0;
    const int fade = rate * 5 / 1000;   // 5 ms ramps, no clicks
    for (int i = 0; i < n; i++) {
        int len = rate * seg[i].ms / 1000;
        for (int k = 0; k < len; k++) {
            float v = 0.0f;
            if (seg[i].hz) {
                float env = 1.0f;
                if (k < fade) env = (float)k / fade;
                if (len - 1 - k < fade) env = (float)(len - 1 - k) / fade;
                v = 0.5f * env * sinf(6.2831853f * seg[i].hz * (float)k / rate);
            }
            pcm[at++] = (int16_t)(v * 32767.0f);
        }
    }
    return w;
}

bool Tone_Play(const uint8_t* wav) {
    if (!wav) return false;
    return PlaySoundA((LPCSTR)wav, nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT) != FALSE;
}
