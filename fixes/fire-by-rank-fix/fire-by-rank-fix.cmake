# fire-by-rank-fix (fbrfix): included by the root CMakeLists.txt when FIX_FIRE_BY_RANK is ON.
# Dev build: src/fbrfix.cpp (research probe with FBRFIX_PROBE=1; the fix itself is not written yet)
# + the harness. No public release source yet.
set(FBRFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(dinput8 PRIVATE ${FBRFIX_DIR}/src)    # the generated stack-offset header
target_compile_definitions(dinput8 PRIVATE FIX_FIRE_BY_RANK)      # registers FbrFix_Install / _Remove (proxy/fixes.cpp)

if(NOT PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE ${FBRFIX_DIR}/src/fbrfix.cpp)
    if(BUILD_TESTS)
        # Maps the real empire.retail.dll, loads the proxy and enters the hooked game code on fake
        # machines, states, units and soldiers (tests/run_tests.py runs it in all its modes).
        add_executable(fbrfix_test ${FBRFIX_DIR}/tests/fbrfix_test.cpp)
        target_compile_definitions(fbrfix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(fbrfix_test PRIVATE /W3 /O1)
        target_link_options(fbrfix_test PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(fbrfix_test PRIVATE user32)
    endif()
endif()
