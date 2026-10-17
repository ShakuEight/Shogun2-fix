// forward_test.cpp -- offline check of the autorun version.dll proxy (tools/autorun/tests).
//
//   autorun_forward_test.exe <path to the built proxy version.dll>
//
// Loads the proxy by full path in this (non-game) process, so the proxy must only forward: no log
// file, no threads. Checks that all 17 exports exist, that the version queries on a system file
// give byte-identical results through the proxy and through the real system version.dll, and that
// no autorun_log.txt appeared next to this executable. Prints one line per check and
// "RESULT <passed> passed, <failed> failed".

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

static int g_pass = 0, g_fail = 0;

static void Check(bool ok, const char* name, const char* detail = "") {
    printf("%s %s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (ok) g_pass++; else g_fail++;
}

typedef DWORD(WINAPI* SizeW)(LPCWSTR, LPDWORD);
typedef BOOL(WINAPI* InfoW)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef BOOL(WINAPI* QueryW)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
typedef DWORD(WINAPI* LangW)(DWORD, LPWSTR, DWORD);

static const char* kNames[] = {
    "GetFileVersionInfoA",       "GetFileVersionInfoByHandle", "GetFileVersionInfoExA",
    "GetFileVersionInfoExW",     "GetFileVersionInfoSizeA",    "GetFileVersionInfoSizeExA",
    "GetFileVersionInfoSizeExW", "GetFileVersionInfoSizeW",    "GetFileVersionInfoW",
    "VerFindFileA",              "VerFindFileW",               "VerInstallFileA",
    "VerInstallFileW",           "VerLanguageNameA",           "VerLanguageNameW",
    "VerQueryValueA",            "VerQueryValueW",
};

struct Info {
    DWORD size = 0;
    std::vector<unsigned char> block;
    VS_FIXEDFILEINFO fixed = {};
    bool fixedOk = false;
};

static Info Query(HMODULE dll, const wchar_t* file) {
    Info r;
    SizeW sz = (SizeW)GetProcAddress(dll, "GetFileVersionInfoSizeW");
    InfoW gi = (InfoW)GetProcAddress(dll, "GetFileVersionInfoW");
    QueryW qv = (QueryW)GetProcAddress(dll, "VerQueryValueW");
    if (!sz || !gi || !qv) return r;
    DWORD h = 0;
    r.size = sz(file, &h);
    if (!r.size) return r;
    r.block.resize(r.size);
    if (!gi(file, 0, r.size, r.block.data())) return r;
    VS_FIXEDFILEINFO* f = nullptr;
    UINT n = 0;
    if (qv(r.block.data(), L"\\", (LPVOID*)&f, &n) && f && n >= sizeof(VS_FIXEDFILEINFO)) {
        r.fixed = *f;
        r.fixedOk = true;
    }
    return r;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: autorun_forward_test.exe <proxy version.dll>\n");
        return 2;
    }
    char exeDir[MAX_PATH];
    GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
    *(strrchr(exeDir, '\\') + 1) = 0;
    char logPath[MAX_PATH];
    snprintf(logPath, sizeof(logPath), "%sautorun_log.txt", exeDir);
    DeleteFileA(logPath);

    HMODULE proxy = LoadLibraryA(argv[1]);
    Check(proxy != nullptr, "F1 proxy loads", argv[1]);
    if (!proxy) {
        printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
        return 1;
    }
    char sys[MAX_PATH];
    GetSystemDirectoryA(sys, MAX_PATH);
    strcat(sys, "\\version.dll");
    HMODULE real = LoadLibraryA(sys);
    Check(real != nullptr && real != proxy, "F2 real version.dll is a different module", sys);

    int missing = 0;
    for (const char* n : kNames)
        if (!GetProcAddress(proxy, n)) { missing++; printf("     missing export %s\n", n); }
    char d[64];
    snprintf(d, sizeof(d), "%d of 17 present", 17 - missing);
    Check(missing == 0, "F3 all 17 exports", d);

    wchar_t file[MAX_PATH];
    GetSystemDirectoryW(file, MAX_PATH);
    wcscat(file, L"\\kernel32.dll");
    Info a = Query(proxy, file), b = Query(real, file);
    snprintf(d, sizeof(d), "size %lu vs %lu", a.size, b.size);
    Check(a.size != 0 && a.size == b.size, "F4 GetFileVersionInfoSizeW forwards", d);
    Check(!a.block.empty() && a.block == b.block, "F5 GetFileVersionInfoW block identical");
    snprintf(d, sizeof(d), "%u.%u.%u.%u", HIWORD(a.fixed.dwFileVersionMS), LOWORD(a.fixed.dwFileVersionMS),
             HIWORD(a.fixed.dwFileVersionLS), LOWORD(a.fixed.dwFileVersionLS));
    Check(a.fixedOk && b.fixedOk && memcmp(&a.fixed, &b.fixed, sizeof(a.fixed)) == 0,
          "F6 VerQueryValueW fixed info identical", d);

    // VerLanguageNameW is forwarded to kernel32 by the system DLL; the stub must reach it too.
    LangW lp = (LangW)GetProcAddress(proxy, "VerLanguageNameW");
    LangW lr = (LangW)GetProcAddress(real, "VerLanguageNameW");
    wchar_t n1[128] = L"", n2[128] = L"";
    DWORD c1 = lp ? lp(0x0409, n1, 128) : 0, c2 = lr ? lr(0x0409, n2, 128) : 0;
    Check(c1 != 0 && c1 == c2 && wcscmp(n1, n2) == 0, "F7 VerLanguageNameW (kernel32 forward) identical");

    Check(GetFileAttributesA(logPath) == INVALID_FILE_ATTRIBUTES, "F8 no autorun_log.txt in a non-game process");

    FreeLibrary(proxy);
    FreeLibrary(real);
    printf("RESULT %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
