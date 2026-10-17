// log.cpp

#include "log.h"
#include <cstdio>
#include <cstdarg>

static FILE* g_logFile = nullptr;
static FILE* g_recFile = nullptr;      // copy of every line while a recording is on (its folder's log)
static CRITICAL_SECTION g_logCS;
static bool g_csInit = false;
static char g_runDir[MAX_PATH] = "";
static char g_gameDir[MAX_PATH] = "";
static bool g_captureMode = false;

const char* LogRunDir() { return g_runDir; }
bool LogCaptureMode() { return g_captureMode; }

#ifdef PATHFIX_RUN_FOLDERS
// <game dir>\pathfix_runs\<YYYYMMDD_HHMMSS>\ (suffix _2, _3, ... if taken).
// Nothing in an earlier run folder is ever touched. pathfix_runs\latest.txt
// names the newest folder (rewritten each time; it only holds the name).
// Returns false (g_runDir = the game folder) when no folder could be made.
static bool MakeRunDir(const char* gameDir, const SYSTEMTIME& st) {
    char root[MAX_PATH];
    snprintf(root, sizeof(root), "%spathfix_runs\\", gameDir);
    CreateDirectoryA(root, nullptr);
    char stamp[32];
    snprintf(stamp, sizeof(stamp), "%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    char name[48];
    bool made = false;
    for (int n = 1; n < 100; n++) {
        if (n == 1) snprintf(name, sizeof(name), "%s", stamp);
        else        snprintf(name, sizeof(name), "%s_%d", stamp, n);
        snprintf(g_runDir, sizeof(g_runDir), "%s%s\\", root, name);
        if (CreateDirectoryA(g_runDir, nullptr)) { made = true; break; }
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    if (!made) { strcpy(g_runDir, gameDir); return false; }
    char latest[MAX_PATH];
    snprintf(latest, sizeof(latest), "%slatest.txt", root);
    FILE* f = fopen(latest, "w");
    if (f) { fprintf(f, "%s\n", name); fclose(f); }
    return true;
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
    strcpy(g_gameDir, dir);

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
        fprintf(g_logFile, "=== capture %s, writing into %s ===\n",
                g_captureMode ? "ON (PATHFIX_CAPTURE=1)" : "OFF", g_runDir);
        fprintf(g_logFile, "=== each line starts with the local time [hh:mm:ss.mmm] ===\n\n");
        fflush(g_logFile);
    }
}

void LogClose() {
    if (g_csInit) EnterCriticalSection(&g_logCS);
    if (g_recFile) {
        fprintf(g_recFile, "\n=== Proxy unloaded ===\n");
        fclose(g_recFile);
        g_recFile = nullptr;
    }
    if (g_logFile) {
        fprintf(g_logFile, "\n=== Proxy unloaded ===\n");
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    if (g_csInit) {
        LeaveCriticalSection(&g_logCS);
        DeleteCriticalSection(&g_logCS);
        g_csInit = false;
    }
}

#ifdef PATHFIX_RUN_FOLDERS
bool LogRecordingStart(int number, char* folderOut, size_t cap) {
    if (!g_csInit) return false;
    EnterCriticalSection(&g_logCS);
    bool ok = false;
    if (!g_recFile) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        if (MakeRunDir(g_gameDir, st)) {
            char path[MAX_PATH];
            snprintf(path, sizeof(path), "%spathfix_log.txt", g_runDir);
            g_recFile = fopen(path, "w");
            if (g_recFile) {
                char exe[MAX_PATH];
                GetModuleFileNameA(nullptr, exe, MAX_PATH);
                fprintf(g_recFile, "=== Pathfix dinput8 Proxy v1: recording %d ===\n", number);
                fprintf(g_recFile, "=== started %04d-%02d-%02d %02d:%02d:%02d.%03d (timestamp_ms 0 in the CSV files) ===\n",
                        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
                fprintf(g_recFile, "=== process %s (pid %lu) ===\n", exe, GetCurrentProcessId());
                fprintf(g_recFile, "=== session log: %spathfix_log.txt ===\n\n", g_gameDir);
                fflush(g_recFile);
                ok = true;
            } else {
                strcpy(g_runDir, g_gameDir);
            }
        }
    }
    if (folderOut && cap) snprintf(folderOut, cap, "%s", g_runDir);
    LeaveCriticalSection(&g_logCS);
    return ok;
}

void LogRecordingStop() {
    if (!g_csInit) return;
    EnterCriticalSection(&g_logCS);
    if (g_recFile) {
        fprintf(g_recFile, "\n=== recording stopped ===\n");
        fclose(g_recFile);
        g_recFile = nullptr;
    }
    strcpy(g_runDir, g_gameDir);
    LeaveCriticalSection(&g_logCS);
}
#else
bool LogRecordingStart(int, char* folderOut, size_t cap) { if (folderOut && cap) folderOut[0] = 0; return false; }
void LogRecordingStop() {}
#endif

static void Line(FILE* f, const char* stamp, const char* fmt, va_list args) {
    fputs(stamp, f);
    vfprintf(f, fmt, args);
    fputc('\n', f);
    fflush(f);
}

void Log(const char* fmt, ...) {
    if (!g_logFile) return;
    EnterCriticalSection(&g_logCS);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char stamp[24];
    snprintf(stamp, sizeof(stamp), "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, fmt);
    Line(g_logFile, stamp, fmt, args);
    va_end(args);
    if (g_recFile) {
        va_start(args, fmt);
        Line(g_recFile, stamp, fmt, args);
        va_end(args);
    }

    LeaveCriticalSection(&g_logCS);
}
