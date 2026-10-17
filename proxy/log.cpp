// log.cpp

#include "log.h"
#include <cstdio>
#include <cstdarg>

static FILE* g_logFile = nullptr;
static CRITICAL_SECTION g_logCS;
static bool g_csInit = false;
static char g_runDir[MAX_PATH] = "";
static bool g_captureMode = false;

const char* LogRunDir() { return g_runDir; }
bool LogCaptureMode() { return g_captureMode; }

#ifdef PATHFIX_RUN_FOLDERS
// <game dir>\pathfix_runs\<YYYYMMDD_HHMMSS>\ (suffix _2, _3, ... if taken).
// Nothing in an earlier run folder is ever touched. pathfix_runs\latest.txt
// names the newest folder (rewritten each launch; it only holds the name).
static void MakeRunDir(const char* gameDir, const SYSTEMTIME& st) {
    char root[MAX_PATH];
    snprintf(root, sizeof(root), "%spathfix_runs\\", gameDir);
    CreateDirectoryA(root, nullptr);
    char stamp[32];
    snprintf(stamp, sizeof(stamp), "%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    char name[48];
    for (int n = 1; n < 100; n++) {
        if (n == 1) snprintf(name, sizeof(name), "%s", stamp);
        else        snprintf(name, sizeof(name), "%s_%d", stamp, n);
        snprintf(g_runDir, sizeof(g_runDir), "%s%s\\", root, name);
        if (CreateDirectoryA(g_runDir, nullptr)) break;
        if (GetLastError() != ERROR_ALREADY_EXISTS) { strcpy(g_runDir, gameDir); return; }
    }
    char latest[MAX_PATH];
    snprintf(latest, sizeof(latest), "%slatest.txt", root);
    FILE* f = fopen(latest, "w");
    if (f) { fprintf(f, "%s\n", name); fclose(f); }
}
#endif

void LogInit() {
    InitializeCriticalSection(&g_logCS);
    g_csInit = true;

    char dir[MAX_PATH];
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) *(slash + 1) = 0;
    else dir[0] = 0;

    SYSTEMTIME st;
    GetLocalTime(&st);
    strcpy(g_runDir, dir);
    char env[8] = "";
    g_captureMode = GetEnvironmentVariableA("PATHFIX_CAPTURE", env, sizeof(env)) > 0 && strcmp(env, "1") == 0;
#ifdef PATHFIX_RUN_FOLDERS
    if (g_captureMode) MakeRunDir(dir, st);
#endif
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%spathfix_log.txt", g_runDir);

    g_logFile = fopen(path, "w");
    if (g_logFile) {
        fprintf(g_logFile, "=== Pathfix dinput8 Proxy v1 ===\n");
        fprintf(g_logFile, "=== %04d-%02d-%02d %02d:%02d:%02d ===\n\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        char exe[MAX_PATH];
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        fprintf(g_logFile, "=== process %s (pid %lu) ===\n", exe, GetCurrentProcessId());
        fprintf(g_logFile, "=== capture %s, writing into %s ===\n\n",
                g_captureMode ? "ON (PATHFIX_CAPTURE=1)" : "OFF", g_runDir);
        fflush(g_logFile);
    }
}

void LogClose() {
    if (g_logFile) {
        fprintf(g_logFile, "\n=== Proxy unloaded ===\n");
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    if (g_csInit) {
        DeleteCriticalSection(&g_logCS);
        g_csInit = false;
    }
}

void Log(const char* fmt, ...) {
    if (!g_logFile) return;
    EnterCriticalSection(&g_logCS);

    DWORD tick = GetTickCount();
    fprintf(g_logFile, "[%8u] ", tick);

    va_list args;
    va_start(args, fmt);
    vfprintf(g_logFile, fmt, args);
    va_end(args);

    fprintf(g_logFile, "\n");
    fflush(g_logFile);

    LeaveCriticalSection(&g_logCS);
}
