# autorun: included by the root CMakeLists.txt when TOOL_AUTORUN is ON (dev tool, never in the
# public release). Builds build/version.dll, a version.dll proxy that reports the battle state of
# the running game over a named pipe and keeps the window minimized (tools/autorun/README.md),
# plus the offline forwarding test.
set(AUTORUN_DIR ${CMAKE_CURRENT_LIST_DIR})

add_library(autorun_version SHARED
    ${AUTORUN_DIR}/src/dllmain.cpp
    ${AUTORUN_DIR}/src/version_proxy.cpp
    ${AUTORUN_DIR}/src/autorun.cpp
    ${AUTORUN_DIR}/src/version.def
    ${CMAKE_SOURCE_DIR}/proxy/log.cpp)                     # shared logger, own file name below
target_include_directories(autorun_version PRIVATE ${CMAKE_SOURCE_DIR}/proxy ${AUTORUN_DIR}/src)
target_compile_definitions(autorun_version PRIVATE
    _CRT_SECURE_NO_WARNINGS
    LOG_FILE_NAME="autorun_log.txt"
    LOG_BANNER="Autorun version.dll proxy v1"
    LOG_CAPTURE_ENV="AUTORUN_CAPTURE")                     # no run folders: PATHFIX_RUN_FOLDERS unset
target_compile_options(autorun_version PRIVATE /W3 /GR- /O2)
target_link_options(autorun_version PRIVATE /MACHINE:X86 /SAFESEH:NO)
target_link_libraries(autorun_version PRIVATE user32 kernel32)
set_target_properties(autorun_version PROPERTIES PREFIX "" OUTPUT_NAME "version")

if(BUILD_TESTS)
    add_executable(autorun_forward_test ${AUTORUN_DIR}/tests/forward_test.cpp)
    target_compile_definitions(autorun_forward_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(autorun_forward_test PRIVATE /W3)
    target_link_libraries(autorun_forward_test PRIVATE user32)
endif()
