# pathfix: included by the root CMakeLists.txt when FIX_PATHFIX is ON.
# Dev build: src/pathfix.cpp (v41, I toggles the fix, capture with PATHFIX_CAPTURE=1) + the harness.
# Public DLL: release/pathfix_release.cpp (variant G always on, nothing else) + the version resource.
set(PATHFIX_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(dinput8 PRIVATE ${PATHFIX_DIR}/src)   # the generated stack-offset headers
target_compile_definitions(dinput8 PRIVATE FIX_PATHFIX)            # registers Pathfix_Install / _Remove (proxy/fixes.cpp)

if(PUBLIC_RELEASE)
    target_sources(dinput8 PRIVATE
        ${PATHFIX_DIR}/release/pathfix_release.cpp
        ${PATHFIX_DIR}/release/version.rc)
else()
    target_sources(dinput8 PRIVATE ${PATHFIX_DIR}/src/pathfix.cpp)
    if(BUILD_TESTS)
        # Maps the real empire.retail.dll, loads the proxy and drives the hooks on fake game
        # objects (tests/run_tests.py runs it in all its modes).
        add_executable(pathfix_hooktest ${PATHFIX_DIR}/tests/hooktest.cpp)
        target_compile_definitions(pathfix_hooktest PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(pathfix_hooktest PRIVATE /W3 /O1)
        target_link_options(pathfix_hooktest PRIVATE /SAFESEH:NO /LARGEADDRESSAWARE)
        target_link_libraries(pathfix_hooktest PRIVATE user32)
    endif()
endif()
