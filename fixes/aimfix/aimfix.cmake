# aimfix: included by the root CMakeLists.txt when FIX_AIMFIX is ON.
# Dev build: src/aimfix.cpp (fix on at launch, AIMFIX_OFF=1 starts it off; AIMFIX_PROBE=1 research
# capture) + the harness. No public release source yet.
set(AIMFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(dinput8 PRIVATE ${AIMFIX_DIR}/src)    # the generated stack-offset header
target_compile_definitions(dinput8 PRIVATE FIX_AIMFIX)             # registers Aimfix_Install / _Remove (proxy/fixes.cpp)

if(NOT PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE ${AIMFIX_DIR}/src/aimfix.cpp)
    if(BUILD_TESTS)
        # Maps the real empire.retail.dll, loads the proxy and enters FUN_1047fd90 on fake fire
        # actions (tests/run_tests.py runs it in all its modes).
        add_executable(aimfix_test ${AIMFIX_DIR}/tests/aimtest.cpp)
        target_compile_definitions(aimfix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(aimfix_test PRIVATE /W3 /O1)
        target_link_options(aimfix_test PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(aimfix_test PRIVATE user32)
    endif()
endif()
