# wall-manning-fix (wallfix): included by the root CMakeLists.txt when FIX_WALL_MANNING is ON.
# Dev build: src/wallfix.cpp (the research probe with WALLFIX_PROBE=1; the fix follows) + the
# harness. No public release source yet.
set(WALLFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(dinput8 PRIVATE ${WALLFIX_DIR}/src)          # the generated stack-offset header
target_compile_definitions(dinput8 PRIVATE FIX_WALL_MANNING)            # registers WallFix_Install / _Remove (proxy/fixes.cpp)

if(NOT PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE ${WALLFIX_DIR}/src/wallfix.cpp)
    if(BUILD_TESTS)
        # Maps the real empire.retail.dll, loads the proxy and enters the hooked game code on fake
        # units, actions and requests (tests/run_tests.py runs it in all its modes).
        add_executable(wallfix_test ${WALLFIX_DIR}/tests/wallfix_test.cpp)
        target_compile_definitions(wallfix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(wallfix_test PRIVATE /W3 /O1)
        target_link_options(wallfix_test PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(wallfix_test PRIVATE user32)
    endif()
endif()
