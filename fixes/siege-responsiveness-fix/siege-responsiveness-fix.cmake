# siege-responsiveness-fix (siegefix): included by the root CMakeLists.txt when FIX_SIEGE_RESPONSIVENESS is ON.
# Dev build: src/siegefix.cpp (the fix, and the research probe with SIEGEFIX_PROBE=1)
# + the harness. No public release source yet.
set(SIEGEFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(dinput8 PRIVATE ${SIEGEFIX_DIR}/src)          # the generated stack-offset header
target_compile_definitions(dinput8 PRIVATE FIX_SIEGE_RESPONSIVENESS)      # registers SiegeFix_Install / _Remove (proxy/fixes.cpp)

if(NOT PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE ${SIEGEFIX_DIR}/src/siegefix.cpp)
    if(BUILD_TESTS)
        # Maps the real empire.retail.dll, loads the proxy and enters the hooked game code on a fake
        # pathfinder, entities and requests (tests/run_tests.py runs it in all its modes).
        add_executable(siegefix_test ${SIEGEFIX_DIR}/tests/siegefix_test.cpp)
        target_compile_definitions(siegefix_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(siegefix_test PRIVATE /W3 /O1)
        target_link_options(siegefix_test PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(siegefix_test PRIVATE user32)
    endif()
endif()
