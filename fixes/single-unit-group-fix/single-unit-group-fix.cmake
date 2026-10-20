# single-unit-group-fix (groupfix): included by the root CMakeLists.txt when FIX_SINGLE_UNIT_GROUP is ON.
# Dev build: src/groupfix.cpp (the fix, its dev log hook, the research probe with GROUPFIX_PROBE=1)
# + the harness. No public release source yet.
set(GROUPFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(dinput8 PRIVATE ${GROUPFIX_DIR}/src)          # the generated stack-offset headers
target_compile_definitions(dinput8 PRIVATE FIX_SINGLE_UNIT_GROUP)        # registers GroupFix_Install / _Remove (proxy/fixes.cpp)
# Live-check builds: the fix starts off, I turns it on (GROUPFIX_ON=1 starts it on).
option(GROUPFIX_START_OFF "single-unit-group-fix starts off at launch (dev live-check build)" OFF)
if(GROUPFIX_START_OFF)
    target_compile_definitions(dinput8 PRIVATE GROUPFIX_START_OFF)
endif()
# Live-check builds for a game started from Steam (no environment variables): the probe starts on,
# GROUPFIX_PROBE=0 turns it off. Never in build/ (the harness expects the probe off by default).
option(GROUPFIX_PROBE_DEFAULT "single-unit-group-fix probe on at launch (dev live-check build)" OFF)
if(GROUPFIX_PROBE_DEFAULT)
    target_compile_definitions(dinput8 PRIVATE GROUPFIX_PROBE_DEFAULT)
endif()

if(NOT PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE ${GROUPFIX_DIR}/src/groupfix.cpp)
    if(BUILD_TESTS AND EXISTS ${GROUPFIX_DIR}/tests/groupfix_test.cpp)
        # Maps the real empire.retail.dll, loads the proxy and runs the game's own group-of-the-
        # selection function on a fake selection manager (tests/run_tests.py runs it in all modes).
        add_executable(groupfix_test ${GROUPFIX_DIR}/tests/groupfix_test.cpp)
        target_compile_definitions(groupfix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(groupfix_test PRIVATE /W3 /O1)
        target_link_options(groupfix_test PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(groupfix_test PRIVATE user32)
    endif()
endif()
