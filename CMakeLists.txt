cmake_minimum_required(VERSION 3.20)
project(swarm-core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(swarm-core STATIC
    swarm-core/src/swarm_core_math.cpp
    swarm-core/src/swarm_core_particles.cpp
    swarm-core/src/arena.cpp
    swarm-core/src/integrator.cpp
    swarm-core/src/spatial_grid.cpp
    swarm-core/src/swarm_core.cpp
)
add_library(swarm::core ALIAS swarm-core)

target_include_directories(swarm-core
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/swarm-core/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/swarm-core/src
)

# Determinism: do NOT add /fp:fast or -ffast-math here or in any consumer.
# The Euler step in swarm-core/src/integrator.cpp relies on the compiler not
# contracting `v * dt + p` into an FMA; bit-exact cross-compiler determinism
# is part of the contract (see swarm-core/tests/README.md).
if (MSVC)
    target_compile_options(swarm-core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(swarm-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 3.20-compatible top-level detection (PROJECT_IS_TOP_LEVEL is 3.21+).
set(SWARM_CORE_IS_TOP_LEVEL OFF)
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(SWARM_CORE_IS_TOP_LEVEL ON)
endif()

option(SWARM_CORE_BUILD_TESTS
    "Build swarm-core native contract test drivers and register with CTest"
    ${SWARM_CORE_IS_TOP_LEVEL})

if (SWARM_CORE_BUILD_TESTS)
    enable_testing()

    foreach(name IN ITEMS test_arena test_update test_spatial_grid)
        add_executable(${name} swarm-core/tests/${name}.cpp)
        target_link_libraries(${name} PRIVATE swarm::core)
        target_include_directories(${name}
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/swarm-core/src)
        add_test(NAME ${name} COMMAND ${name})
    endforeach()
endif()

# Emscripten bridge target (CORE-9). Produces swarm-core.js + swarm-core.wasm
# with a fixed linear-memory budget. Off by default; opt in via:
#   emcmake cmake -S . -B build-wasm -DSWARM_CORE_BUILD_WASM=ON
# The bridge translation unit lives in swarm-core/bridge/emscripten/ to keep
# <emscripten.h> out of swarm-core/src/ per
# Docs/CORE_EXTRACTION_ARCHITECTURE.md §10.
option(SWARM_CORE_BUILD_WASM
    "Build the Emscripten bridge target (swarm-core-wasm)"
    OFF)

if (SWARM_CORE_BUILD_WASM)
    if (NOT EMSCRIPTEN)
        message(FATAL_ERROR
            "SWARM_CORE_BUILD_WASM=ON requires the Emscripten toolchain. "
            "Configure with `emcmake cmake -S . -B build-wasm -DSWARM_CORE_BUILD_WASM=ON`.")
    endif()

    # Fixed Wasm heap. ALLOW_MEMORY_GROWTH=0 — growing the heap would detach
    # every typed-array view in the JS render loop. Sized for the 131072-particle
    # hard cap with comfortable headroom for stack and statics.
    set(SWARM_CORE_WASM_INITIAL_MEMORY 67108864 CACHE STRING
        "Initial Wasm linear-memory size in bytes (fixed; ALLOW_MEMORY_GROWTH=0).")

    add_executable(swarm-core-wasm
        swarm-core/bridge/emscripten/wasm_bridge.cpp)
    target_link_libraries(swarm-core-wasm PRIVATE swarm::core)
    set_target_properties(swarm-core-wasm PROPERTIES
        OUTPUT_NAME "swarm-core"
        SUFFIX      ".js")

    target_link_options(swarm-core-wasm PRIVATE
        "-sALLOW_MEMORY_GROWTH=0"
        "-sINITIAL_MEMORY=${SWARM_CORE_WASM_INITIAL_MEMORY}"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createSwarmCoreModule"
        "-sENVIRONMENT=web,node"
        "-sSTRICT=1"
        "-sASSERTIONS=1"
        "-sEXPORTED_FUNCTIONS=[_swarm_init,_swarm_shutdown,_swarm_reset,_swarm_update,_swarm_get_particle_count,_swarm_get_particle_capacity,_swarm_get_default_particles,_swarm_get_max_supported_particles,_swarm_get_pos_x,_swarm_get_pos_y,_swarm_get_vel_x,_swarm_get_vel_y,_swarm_get_color_r,_swarm_get_color_g,_swarm_get_color_b,_swarm_get_color_a,_swarm_get_life,_swarm_get_flags,_swarm_get_abi_version]"
        "-sEXPORTED_RUNTIME_METHODS=[ccall,cwrap,HEAPU8,HEAPU32,HEAPF32]")

    if (SWARM_CORE_BUILD_TESTS)
        find_program(NODE_EXECUTABLE node)
        if (NODE_EXECUTABLE)
            add_test(NAME swarm-core-wasm-smoke
                COMMAND ${NODE_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/swarm-core/bridge/emscripten/smoke/smoke.mjs
                    $<TARGET_FILE:swarm-core-wasm>)
        else()
            message(STATUS
                "swarm-core-wasm-smoke skipped: `node` not on PATH. "
                "Run the harness manually after build.")
        endif()
    endif()
endif()
