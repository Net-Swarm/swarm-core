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

option(SWARM_CORE_BUILD_TESTS
    "Build swarm-core native contract test drivers and register with CTest"
    ${PROJECT_IS_TOP_LEVEL})

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
