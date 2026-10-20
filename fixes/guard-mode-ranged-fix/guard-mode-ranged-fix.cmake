# guard-mode-ranged-fix (guardfix): included by the root CMakeLists.txt when FIX_GUARD_MODE_RANGED is ON.
# Dev build: src/guardfix.cpp (the fix, plus the research probe with GUARDFIX_PROBE=1) + the
# harness. No public release source yet.
set(GUARDFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(dinput8 PRIVATE ${GUARDFIX_DIR}/src)        # the generated stack-offset header
target_compile_definitions(dinput8 PRIVATE FIX_GUARD_MODE_RANGED)       # registers GuardFix_Install / _Remove (proxy/fixes.cpp)
# Live-check builds: the fix starts off, I turns it on (GUARDFIX_ON=1 starts it on).
option(GUARDFIX_START_OFF "guard-mode-ranged-fix starts off at launch (dev live-check build)" OFF)
if(GUARDFIX_START_OFF)
    target_compile_definitions(dinput8 PRIVATE GUARDFIX_START_OFF)
endif()

if(NOT PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE ${GUARDFIX_DIR}/src/guardfix.cpp)
    if(BUILD_TESTS)
        # Maps the real empire.retail.dll, loads the proxy and calls the redirected missile task
        # tests on fake units, groups and tasks (tests/run_tests.py runs it in all its modes).
        add_executable(guardfix_test ${GUARDFIX_DIR}/tests/guardfix_test.cpp)
        target_compile_definitions(guardfix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(guardfix_test PRIVATE /W3 /O1)
        target_link_options(guardfix_test PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(guardfix_test PRIVATE user32)
    endif()
endif()
