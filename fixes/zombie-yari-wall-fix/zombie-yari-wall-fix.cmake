# zombie-yari-wall-fix (zombiefix): included by the root CMakeLists.txt when FIX_ZOMBIE_YARI_WALL is ON.
# Dev build: src/zombiefix.cpp (the research probe with ZOMBIEFIX_PROBE=1; the fix follows) + the
# harness. No public release source yet.
set(ZOMBIEFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(dinput8 PRIVATE ${ZOMBIEFIX_DIR}/src)        # the generated stack-offset header
target_compile_definitions(dinput8 PRIVATE FIX_ZOMBIE_YARI_WALL)        # registers ZombieFix_Install / _Remove (proxy/fixes.cpp)
# Live-check builds: the fix starts off, I turns it on (ZOMBIEFIX_ON=1 starts it on).
option(ZOMBIEFIX_START_OFF "zombie-yari-wall-fix starts off at launch (dev live-check build)" OFF)
if(ZOMBIEFIX_START_OFF)
    target_compile_definitions(dinput8 PRIVATE ZOMBIEFIX_START_OFF)
endif()

if(NOT PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE ${ZOMBIEFIX_DIR}/src/zombiefix.cpp)
    if(BUILD_TESTS)
        # Maps the real empire.retail.dll, loads the proxy and enters the hooked game code on fake
        # units, soldiers and state machines (tests/run_tests.py runs it in all its modes).
        add_executable(zombiefix_test ${ZOMBIEFIX_DIR}/tests/zombiefix_test.cpp)
        target_compile_definitions(zombiefix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(zombiefix_test PRIVATE /W3 /O1)
        target_link_options(zombiefix_test PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(zombiefix_test PRIVATE user32)
    endif()
endif()
