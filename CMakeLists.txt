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
option(FIX_ARTILLERY_TARGETING "Artillery targeting fix: artillery aims at the middle of the target unit (fixes/artillery-targeting-fix)" ON)
option(FIX_FIRE_BY_RANK "Fire-by-rank fix: matchlock target acquisition, fire by rank, Hattori / Ikko Ikki (fixes/fire-by-rank-fix)" ON)
option(FIX_SIEGE_RESPONSIVENESS "Siege responsiveness fix: defending units answer orders during wall assaults (fixes/siege-responsiveness-fix)" ON)
option(FIX_WALL_MANNING "Wall manning fix: units ordered onto a wall go straight there (fixes/wall-manning-fix)" ON)
option(FIX_ZOMBIE_YARI_WALL "Zombie yari wall fix: routed yari wall units stop killing and run (fixes/zombie-yari-wall-fix)" ON)
option(FIX_WALL_COLLAPSE "Wall collapse fix: men deployed on a gatehouse are not killed at Start Battle when the gates are open to all (fixes/wall-collapse-fix)" ON)
option(FIX_SINGLE_UNIT_GROUP "Single unit group fix: G, Ctrl+G and Ctrl+number accept a single unit (fixes/single-unit-group-fix)" ON)
option(FIX_GUARD_MODE_RANGED "Guard mode ranged fix: missile units in guard mode do not chase targets out of range and drop shattered targets (fixes/guard-mode-ranged-fix)" ON)
option(FIX_KNEEL_FIRE_MOVE "Kneel fire move fix: units with kneel fire on take move orders (fixes/kneel-fire-move-fix)" ON)
option(TOOL_AUTORUN "Dev tool: version.dll proxy for unattended replay runs (tools/autorun); ignored with PUBLIC_RELEASE" OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# --- the proxy -----------------------------------------------------------------------------
add_library(dinput8 SHARED
    proxy/dllmain.cpp
    proxy/dinput8_proxy.cpp
    proxy/fixes.cpp
    proxy/hook.cpp
    proxy/gamemem.cpp
    proxy/dinput8.def)
target_include_directories(dinput8 PRIVATE proxy)
target_compile_definitions(dinput8 PRIVATE _CRT_SECURE_NO_WARNINGS)
if(PUBLIC_RELEASE)
    target_compile_definitions(dinput8 PRIVATE PATHFIX_NO_LOG)          # proxy/log.h: logging compiled out
else()
    target_sources(dinput8 PRIVATE proxy/log.cpp proxy/keys.cpp)   # keys + tones: dev build only
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
if(FIX_ARTILLERY_TARGETING AND NOT PUBLIC_RELEASE)
    include(fixes/artillery-targeting-fix/artillery-targeting-fix.cmake)
endif()
if(FIX_FIRE_BY_RANK AND NOT PUBLIC_RELEASE)
    include(fixes/fire-by-rank-fix/fire-by-rank-fix.cmake)
endif()
if(FIX_SIEGE_RESPONSIVENESS AND NOT PUBLIC_RELEASE)
    include(fixes/siege-responsiveness-fix/siege-responsiveness-fix.cmake)
endif()
if(FIX_WALL_MANNING AND NOT PUBLIC_RELEASE)
    include(fixes/wall-manning-fix/wall-manning-fix.cmake)
endif()
if(FIX_ZOMBIE_YARI_WALL AND NOT PUBLIC_RELEASE)
    include(fixes/zombie-yari-wall-fix/zombie-yari-wall-fix.cmake)
endif()
if(FIX_WALL_COLLAPSE AND NOT PUBLIC_RELEASE)
    include(fixes/wall-collapse-fix/wall-collapse-fix.cmake)
endif()
if(FIX_SINGLE_UNIT_GROUP AND NOT PUBLIC_RELEASE)
    include(fixes/single-unit-group-fix/single-unit-group-fix.cmake)
endif()
if(FIX_GUARD_MODE_RANGED AND NOT PUBLIC_RELEASE)
    include(fixes/guard-mode-ranged-fix/guard-mode-ranged-fix.cmake)
endif()
if(FIX_KNEEL_FIRE_MOVE AND NOT PUBLIC_RELEASE)
    include(fixes/kneel-fire-move-fix/kneel-fire-move-fix.cmake)
endif()

# --- developer tools (separate proxy DLLs, never part of the public release) ---------------
# autorun: build/version.dll, the in-game side of the unattended replay runner
# (tools/autorun/README.md). Turn on with build.bat -DTOOL_AUTORUN=ON.
if(TOOL_AUTORUN AND NOT PUBLIC_RELEASE)
    include(tools/autorun/autorun.cmake)
endif()
