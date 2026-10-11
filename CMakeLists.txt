cmake_minimum_required(VERSION 3.21)

project(sys-stuff LANGUAGES CXX VERSION 1.0.0)

if(NOT WIN32)
    message(FATAL_ERROR "system-stuff is a Win32 SDK application and only builds on Windows.")
endif()
if(NOT MSVC)
    message(FATAL_ERROR "system-stuff requires MSVC; no other toolchain is supported.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "system-stuff is 64-bit only; configure with an x64 toolchain.")
endif()

enable_language(RC)

add_executable(sys-stuff WIN32
    src/main.cpp
    src/sys-stuff.rc
)

target_sources(sys-stuff PRIVATE
    src/chart.h
    src/custom_controls.h
    src/dark_theme.h
    src/helpers.h
    src/layout.h
    src/resource.h
)

target_compile_features(sys-stuff PRIVATE cxx_std_17)
target_include_directories(sys-stuff PRIVATE src)
target_compile_definitions(sys-stuff PRIVATE WIN32 _WINDOWS UNICODE _UNICODE)
# /sdl, as in the Visual Studio project this build replaced: the SDL security
# checks, with their warnings as errors.
target_compile_options(sys-stuff PRIVATE /W3 /MP /permissive- /sdl)

# As in the Visual Studio project this build replaced: Release links the CRT
# statically, so the single exe runs without the VC++ redistributable; Debug
# keeps the debug DLL CRT.
set_property(TARGET sys-stuff PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")

# Also from that project: Release is optimised as a whole program, /GL on the
# compile and /LTCG on the link.
set_property(TARGET sys-stuff PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

set_target_properties(sys-stuff PROPERTIES
    OUTPUT_NAME sys-stuff-64
    DEBUG_POSTFIX d
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/exe"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/exe"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/exe"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(sys_stuff_tests tests/smoke.cpp)
    target_compile_features(sys_stuff_tests PRIVATE cxx_std_17)
    add_test(NAME sys_stuff_unit COMMAND sys_stuff_tests)
    set_tests_properties(sys_stuff_unit PROPERTIES TIMEOUT 30 LABELS system-stuff)

    # The list sort order, compiled from the app's own header.
    add_executable(sys_stuff_sort_tests tests/sort.cpp)
    target_compile_features(sys_stuff_sort_tests PRIVATE cxx_std_17)
    target_include_directories(sys_stuff_sort_tests PRIVATE src)
    target_compile_definitions(sys_stuff_sort_tests PRIVATE WIN32 _WINDOWS UNICODE _UNICODE)
    add_test(NAME sys_stuff_sort COMMAND sys_stuff_sort_tests)
    set_tests_properties(sys_stuff_sort PROPERTIES TIMEOUT 30 LABELS system-stuff)
endif()
