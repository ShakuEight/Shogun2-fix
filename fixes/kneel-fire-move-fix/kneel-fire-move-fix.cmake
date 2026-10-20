# kneel-fire-move-fix (kneelfix): included by the root CMakeLists.txt when FIX_KNEEL_FIRE_MOVE is ON.
# Dev build: src/kneelfix.cpp (the fix, plus the research probe with KNEELFIX_PROBE=1) + the
# harness. No public release source yet.
set(KNEELFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_compile_definitions(dinput8 PRIVATE FIX_KNEEL_FIRE_MOVE)         # registers KneelFix_Install / _Remove (proxy/fixes.cpp)
# Live-check builds: the fix starts off, I turns it on (KNEELFIX_ON=1 starts it on).
option(KNEELFIX_START_OFF "kneel-fire-move-fix starts off at launch (dev live-check build)" OFF)
if(KNEELFIX_START_OFF)
    target_compile_definitions(dinput8 PRIVATE KNEELFIX_START_OFF)
endif()

if(NOT PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE ${KNEELFIX_DIR}/src/kneelfix.cpp)
    if(BUILD_TESTS)
        # Maps the real empire.retail.dll, loads the proxy and calls the redirected can-move test
        # (the game's own FUN_1015c3f0) on fake units (tests/run_tests.py runs it in all its modes).
        add_executable(kneelfix_test ${KNEELFIX_DIR}/tests/kneelfix_test.cpp)
        target_compile_definitions(kneelfix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(kneelfix_test PRIVATE /W3 /O1)
        target_link_options(kneelfix_test PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(kneelfix_test PRIVATE user32)
    endif()
endif()
