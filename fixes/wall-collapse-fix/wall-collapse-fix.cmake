# wall-collapse-fix (collapsefix): included by the root CMakeLists.txt when FIX_WALL_COLLAPSE is ON.
# Dev build: src/collapsefix.cpp (the research probe with COLLAPSEFIX_PROBE=1, and the fix) + the
# harness. No public release source yet.
set(COLLAPSEFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(dinput8 PRIVATE ${COLLAPSEFIX_DIR}/src)       # the generated stack-offset header
target_compile_definitions(dinput8 PRIVATE FIX_WALL_COLLAPSE)            # registers CollapseFix_Install / _Remove (proxy/fixes.cpp)
# Live-check builds: the fix starts off, I turns it on (COLLAPSEFIX_ON=1 starts it on).
option(COLLAPSEFIX_START_OFF "wall-collapse-fix starts off at launch (dev live-check build)" OFF)
if(COLLAPSEFIX_START_OFF)
    target_compile_definitions(dinput8 PRIVATE COLLAPSEFIX_START_OFF)
endif()

if(NOT PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE ${COLLAPSEFIX_DIR}/src/collapsefix.cpp)
    if(BUILD_TESTS AND EXISTS ${COLLAPSEFIX_DIR}/tests/collapsefix_test.cpp)
        # Maps the real empire.retail.dll, loads the proxy and enters the hooked game code on fake
        # buildings, pieces and soldiers (tests/run_tests.py runs it in all its modes).
        add_executable(collapsefix_test ${COLLAPSEFIX_DIR}/tests/collapsefix_test.cpp)
        target_compile_definitions(collapsefix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(collapsefix_test PRIVATE /W3 /O1)
        target_link_options(collapsefix_test PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(collapsefix_test PRIVATE user32)
    endif()
endif()
