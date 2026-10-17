#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef PATHFIX_NO_LOG
// Public release: no log file, no log strings in the binary.
inline void LogInit() {}
inline void LogClose() {}
#define Log(...) ((void)0)
inline const char* LogRunDir() { return ""; }
inline bool LogCaptureMode() { return false; }
inline bool LogRecordingStart(int, char* folderOut, size_t cap) { if (folderOut && cap) folderOut[0] = 0; return false; }
inline void LogRecordingStop() {}
#else
void LogInit();
void LogClose();
// Every line starts with the local time [hh:mm:ss.mmm].
void Log(const char* fmt, ...);

// Recording (dev build, P key): a new folder <game dir>\pathfix_runs\<YYYYMMDD_HHMMSS>\ becomes
// LogRunDir() and gets its own pathfix_log.txt, a copy of every line logged until
// LogRecordingStop(), which closes it and makes the game folder LogRunDir() again.
// Returns false (nothing changed) when the folder or its log cannot be created.
bool LogRecordingStart(int number, char* folderOut, size_t cap);
void LogRecordingStop();

// Folder (with trailing backslash) that this launch writes into. In capture
// mode (see LogCaptureMode) with PATHFIX_RUN_FOLDERS it is a new folder
// <game dir>\pathfix_runs\<YYYYMMDD_HHMMSS>\ created by LogInit; otherwise the
// game folder, where pathfix_log.txt is rewritten at every launch.
const char* LogRunDir();

// Capture mode: true when the process was started with the environment
// variable PATHFIX_CAPTURE=1 (read once by LogInit). The offline harnesses set
// it; normal play leaves it unset and nothing but the log is written.
bool LogCaptureMode();
#endif
