# Shogun 2 fixes: one dinput8.dll proxy that hosts several game fixes.
#
# 32-bit only: the target is the ORIGINAL x86 Empire.Retail.dll of Total War: Shogun 2.
# Configure from an x86 MSVC environment, or run build.bat / release.bat:
#
#   dev build      cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
#   public DLL     cmake -S . -B build_release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DPUBLIC_RELEASE=ON
#
# Dev build: build/dinput8.dll (logging, per-fix dev sources) and each fix's harness.
# Public DLL: build_release/dinput8.dll (no logging, per-fix release sources, no harness).
# Each fix is compiled in when its FIX_<NAME> option is ON and adds itself through
# fixes/<name>/<name>.cmake. Adding a fix: see fixes/README.md.
cmake_minimum_required(VERSION 3.15)
project(shogun2_fixes CXX RC)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "This DLL must be built 32-bit (x86). Use an x86 MSVC environment.")
endif()

option(PUBLIC_RELEASE "Build the public release DLL: release sources only, no log, no capture, no keys" OFF)
option(BUILD_TESTS "Build the offline harness executables (dev build only)" ON)
option(FIX_PATHFIX "Path fix: units keep formation through gates, doorways and corners (fixes/pathfix)" ON)
option(FIX_AIMFIX "Aim fix: artillery aims at the middle of the target unit (fixes/aimfix)" ON)
option(TOOL_AUTORUN "Dev tool: version.dll proxy for unattended replay runs (tools/autorun); ignored with PUBLIC_RELEASE" OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# --- the proxy -----------------------------------------------------------------------------
add_library(dinput8 SHARED
    proxy/dllmain.cpp
    proxy/dinput8_proxy.cpp
    proxy/fixes.cpp
    proxy/dinput8.def)
target_include_directories(dinput8 PRIVATE proxy)
target_compile_definitions(dinput8 PRIVATE _CRT_SECURE_NO_WARNINGS)
if(PUBLIC_RELEASE)
    target_compile_definitions(dinput8 PRIVATE PATHFIX_NO_LOG)          # proxy/log.h: logging compiled out
else()
    target_sources(dinput8 PRIVATE proxy/log.cpp)
    # every launch logs into <game>\pathfix_log.txt, or with PATHFIX_CAPTURE=1 into
    # <game>\pathfix_runs\<YYYYMMDD_HHMMSS>\ (proxy/log.cpp)
    target_compile_definitions(dinput8 PRIVATE PATHFIX_RUN_FOLDERS)
endif()
target_compile_options(dinput8 PRIVATE /W3 /GR- /O2)
target_link_options(dinput8 PRIVATE /MACHINE:X86 /SAFESEH:NO)
target_link_libraries(dinput8 PRIVATE user32 kernel32)
set_target_properties(dinput8 PROPERTIES PREFIX "" OUTPUT_NAME "dinput8")

# --- fixes (one line per fix) --------------------------------------------------------------
if(FIX_PATHFIX)
    include(fixes/pathfix/pathfix.cmake)
endif()
if(FIX_AIMFIX AND NOT PUBLIC_RELEASE)
    include(fixes/aimfix/aimfix.cmake)
endif()

# --- developer tools (separate proxy DLLs, never part of the public release) ---------------
# autorun: build/version.dll, the in-game side of the unattended replay runner
# (tools/autorun/README.md). Turn on with build.bat -DTOOL_AUTORUN=ON.
if(TOOL_AUTORUN AND NOT PUBLIC_RELEASE)
    include(tools/autorun/autorun.cmake)
endif()
