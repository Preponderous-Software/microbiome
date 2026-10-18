# The opt-in Strata build of Microbiome: the micro-environment simulated with
# Strata's simulation module (strata::sim) and drawn with the Strata engine.
# It is separate from the Makefile on purpose: `make`, mb_app, mb_webapp,
# the test suites, the Docker image and the browser build do not use it.
#
#   cmake -S strata -B build-strata -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-strata -j4
#   ctest --test-dir build-strata --output-on-failure
#
# Strata is fetched at the commit in strata.ref, or taken from a local
# checkout with -DFETCHCONTENT_SOURCE_DIR_STRATA=<path>. See docs/STRATA.md.
cmake_minimum_required(VERSION 3.24)
project(microbiome_strata VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MB_STRATA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(MB_STRATA_BUILD_TESTS "Build the Catch2 tests" ON)
option(MB_STRATA_RENDER_TESTS "Also build the headless render smoke test (needs EGL + a GL 4.5 driver at run time)" ON)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/strata.ref" MB_STRATA_DEFAULT_REF)
string(STRIP "${MB_STRATA_DEFAULT_REF}" MB_STRATA_DEFAULT_REF)
set(MB_STRATA_REF "${MB_STRATA_DEFAULT_REF}" CACHE STRING "Strata git ref (a full commit SHA; strata.ref pins it)")

include(FetchContent)
set(STRATA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STRATA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(STRATA_WITH_SIM ON CACHE BOOL "" FORCE)
set(STRATA_WITH_USD OFF CACHE BOOL "" FORCE)
FetchContent_Declare(strata
    GIT_REPOSITORY https://github.com/Preponderous-Software/strata.git
    GIT_TAG ${MB_STRATA_REF})
FetchContent_MakeAvailable(strata)

function(mb_strata_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(MB_STRATA_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
        # Determinism: never fuse multiply-adds in the simulation.
        target_compile_options(${target} PRIVATE -ffp-contract=off)
        if(MB_STRATA_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# The simulation: no GL, links only strata::sim.
add_library(mb_strata_sim STATIC src/micro_environment.cpp)
target_include_directories(mb_strata_sim PUBLIC src)
target_link_libraries(mb_strata_sim PUBLIC strata::sim)
mb_strata_warnings(mb_strata_sim)

# Turning the simulation into Strata draw data, and the on-screen HUD.
add_library(mb_strata_scene STATIC src/scene.cpp src/hud.cpp)
target_link_libraries(mb_strata_scene PUBLIC mb_strata_sim strata::static)
mb_strata_warnings(mb_strata_scene)

add_executable(mb_strata app/main.cpp)
target_link_libraries(mb_strata PRIVATE mb_strata_scene)
mb_strata_warnings(mb_strata)

if(MB_STRATA_BUILD_TESTS)
    enable_testing()
    set(CATCH2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../test/lib")

    add_executable(mb_strata_tests test/main.cpp test/test_micro_environment.cpp)
    target_include_directories(mb_strata_tests SYSTEM PRIVATE "${CATCH2_DIR}")
    target_link_libraries(mb_strata_tests PRIVATE mb_strata_sim)
    mb_strata_warnings(mb_strata_tests)
    add_test(NAME mb_strata_tests COMMAND mb_strata_tests)

    if(MB_STRATA_RENDER_TESTS)
        add_executable(mb_strata_render_tests test/main.cpp test/test_render_smoke.cpp)
        target_include_directories(mb_strata_render_tests SYSTEM PRIVATE "${CATCH2_DIR}")
        target_link_libraries(mb_strata_render_tests PRIVATE mb_strata_scene)
        mb_strata_warnings(mb_strata_render_tests)
        add_test(NAME mb_strata_render_tests COMMAND mb_strata_render_tests)
    endif()
endif()
