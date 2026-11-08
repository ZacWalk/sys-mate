cmake_minimum_required(VERSION 3.21)

project(sys-mate LANGUAGES CXX VERSION 2.0.0)

if(NOT WIN32)
    message(FATAL_ERROR "sys-mate is a Win32 monitoring application and only builds on Windows.")
endif()
if(NOT MSVC)
    message(FATAL_ERROR "sys-mate requires MSVC; no other toolchain is supported.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "sys-mate is 64-bit only; configure with an x64 toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the application and libdesktop on the same CRT.  This must be set
# before FetchContent_MakeAvailable so the platform layer is built alike.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")

# Develop against a sibling checkout when one exists; CI and a fresh clone use
# the published, reproducible revision below.
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_LIBDESKTOP
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../libdesktop/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_LIBDESKTOP "${CMAKE_CURRENT_SOURCE_DIR}/../libdesktop"
        CACHE PATH "Local libdesktop checkout to build against")
endif()

include(FetchContent)
FetchContent_Declare(libdesktop
    GIT_REPOSITORY https://github.com/ZacWalk/libdesktop.git
    GIT_TAG        04109ee82a500db38d439fa281bbc3f43124adfe
)
FetchContent_MakeAvailable(libdesktop)

libdesktop_add_app(sys-mate
    SOURCES
        src/main.cpp
        src/native_monitoring.cpp
    ICON        src/res/sys-mate.ico
    DESCRIPTION "SysMate system information utility"
    OUTPUT_NAME sys-mate-64
    EMBED       src/res/sys-mate.ico
)

target_include_directories(sys-mate PRIVATE src)
target_compile_features(sys-mate PRIVATE cxx_std_20)
target_compile_definitions(sys-mate PRIVATE WIN32 _WINDOWS UNICODE _UNICODE)
target_compile_options(sys-mate PRIVATE /W3 /MP /permissive- /sdl /EHsc)
target_link_libraries(sys-mate PRIVATE
    libdesktop::ui
    pdh
    iphlpapi
    psapi
    ws2_32
    shell32
    advapi32
)

target_link_options(sys-mate PRIVATE
    /DEBUG
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>
)

set_property(TARGET sys-mate PROPERTY
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

set_target_properties(sys-mate PROPERTIES
    DEBUG_POSTFIX d
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/exe"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/exe"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/exe"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(sys_mate_tests tests/smoke.cpp)
    target_compile_features(sys_mate_tests PRIVATE cxx_std_20)
    add_test(NAME sys_mate_unit COMMAND sys_mate_tests)
    set_tests_properties(sys_mate_unit PROPERTIES TIMEOUT 30 LABELS sys-mate)

    add_executable(sys_mate_sort_tests tests/sort.cpp)
    target_compile_features(sys_mate_sort_tests PRIVATE cxx_std_20)
    target_include_directories(sys_mate_sort_tests PRIVATE src)
    add_test(NAME sys_mate_sort COMMAND sys_mate_sort_tests)
    set_tests_properties(sys_mate_sort PROPERTIES TIMEOUT 30 LABELS sys-mate)

    add_executable(sys_mate_ui_model_tests tests/ui_model.cpp)
    target_compile_features(sys_mate_ui_model_tests PRIVATE cxx_std_20)
    target_include_directories(sys_mate_ui_model_tests PRIVATE src)
    add_test(NAME sys_mate_ui_model COMMAND sys_mate_ui_model_tests)
    set_tests_properties(sys_mate_ui_model PROPERTIES TIMEOUT 30 LABELS sys-mate)

    add_executable(sys_mate_monitoring_tests tests/monitoring.cpp src/native_monitoring.cpp)
    target_compile_features(sys_mate_monitoring_tests PRIVATE cxx_std_20)
    target_include_directories(sys_mate_monitoring_tests PRIVATE src)
    target_link_libraries(sys_mate_monitoring_tests PRIVATE
        pdh iphlpapi psapi ws2_32 shell32 advapi32)
    add_test(NAME sys_mate_monitoring COMMAND sys_mate_monitoring_tests)
    set_tests_properties(sys_mate_monitoring PROPERTIES TIMEOUT 30 LABELS sys-mate)

    add_executable(sys_mate_filter_input_tests tests/filter_input.cpp)
    target_compile_features(sys_mate_filter_input_tests PRIVATE cxx_std_20)
    target_link_libraries(sys_mate_filter_input_tests PRIVATE libdesktop::ui)
    add_test(NAME sys_mate_filter_input COMMAND sys_mate_filter_input_tests)
    set_tests_properties(sys_mate_filter_input PROPERTIES TIMEOUT 30 LABELS sys-mate)

    add_test(NAME sys_mate_offscreen COMMAND sys-mate --test-offscreen)
    set_tests_properties(sys_mate_offscreen PROPERTIES
        TIMEOUT 30 LABELS sys-mate WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        PASS_REGULAR_EXPRESSION "offscreen: lifecycle passed"
        FAIL_REGULAR_EXPRESSION "offscreen: lifecycle FAILED")

    add_test(NAME sys_mate_ui COMMAND sys-mate --test-ui)
    set_tests_properties(sys_mate_ui PROPERTIES
        TIMEOUT 30 LABELS sys-mate WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        PASS_REGULAR_EXPRESSION "ui-test: lifecycle passed"
        FAIL_REGULAR_EXPRESSION "ui-test: .*FAILED")
endif()
