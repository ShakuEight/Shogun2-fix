# artillery-targeting-fix (artfix): included by the root CMakeLists.txt when FIX_ARTILLERY_TARGETING is ON.
# Dev build: src/artfix.cpp (fix on at launch, ARTFIX_OFF=1 starts it off; ARTFIX_PROBE=1 research
# capture; U toggles it) + the harness. No public release source yet.
# Named aimfix until 2026-10-18.
set(ARTFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(dinput8 PRIVATE ${ARTFIX_DIR}/src)    # the generated stack-offset header
target_compile_definitions(dinput8 PRIVATE FIX_ARTILLERY_TARGETING)   # registers ArtFix_Install / _Remove (proxy/fixes.cpp)

if(NOT PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE ${ARTFIX_DIR}/src/artfix.cpp)
    if(BUILD_TESTS)
        # Maps the real empire.retail.dll, loads the proxy and enters FUN_1047fd90 on fake fire
        # actions (tests/run_tests.py runs it in all its modes).
        add_executable(artfix_test ${ARTFIX_DIR}/tests/artfix_test.cpp)
        target_compile_definitions(artfix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(artfix_test PRIVATE /W3 /O1)
        target_link_options(artfix_test PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(artfix_test PRIVATE user32)
    endif()
endif()
