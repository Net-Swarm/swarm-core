# swarm-core tests

The native contract tests are registered through CMake when
`SWARM_CORE_BUILD_TESTS=ON`. When this repository is configured as the top-level
CMake project, tests are enabled by default.

## CMake / CTest

From the repository root:

```powershell
cmake -S . -B build -DSWARM_CORE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -C Debug
```

On single-config generators, omit `-C Debug`:

```powershell
ctest --test-dir build
```

Registered native tests:

- `test_arena`: CORE-5 fixed arena, alignment, lifecycle, pointer stability,
  reset behavior, and capacity checks.
- `test_update`: CORE-6 deterministic Euler update behavior.
- `test_spatial_grid`: CORE-7 internal spatial-grid storage and validation.

Exit code 0 means every check passed. Each driver prints `<name>: OK` on
success and `FAIL <file>:<line> <expr>` for any failed assertion.

## Direct Driver Execution

If prebuilt local test drivers are present, they can be run directly from the
repository root:

```powershell
.\test_arena.exe
.\test_update.exe
.\test_spatial_grid.exe
```

## Manual Compile Fallback

Use these commands only when CMake is unavailable and a quick one-off compile is
needed.

### MSVC

```bat
cl /std:c++17 /W4 /EHsc /Iswarm-core\include ^
   swarm-core\src\swarm_core_math.cpp ^
   swarm-core\src\swarm_core_particles.cpp ^
   swarm-core\src\spatial_grid.cpp ^
   swarm-core\src\arena.cpp ^
   swarm-core\src\integrator.cpp ^
   swarm-core\src\swarm_core.cpp ^
   swarm-core\tests\test_arena.cpp ^
   /Fe:test_arena.exe
test_arena.exe

cl /std:c++17 /W4 /EHsc /Iswarm-core\include ^
   swarm-core\src\swarm_core_math.cpp ^
   swarm-core\src\swarm_core_particles.cpp ^
   swarm-core\src\spatial_grid.cpp ^
   swarm-core\src\arena.cpp ^
   swarm-core\src\integrator.cpp ^
   swarm-core\src\swarm_core.cpp ^
   swarm-core\tests\test_update.cpp ^
   /Fe:test_update.exe
test_update.exe

cl /std:c++17 /W4 /EHsc /Iswarm-core\include ^
   swarm-core\src\swarm_core_math.cpp ^
   swarm-core\src\swarm_core_particles.cpp ^
   swarm-core\src\spatial_grid.cpp ^
   swarm-core\src\arena.cpp ^
   swarm-core\src\integrator.cpp ^
   swarm-core\src\swarm_core.cpp ^
   swarm-core\tests\test_spatial_grid.cpp ^
   /Fe:test_spatial_grid.exe
test_spatial_grid.exe
```

### Clang / GCC

```sh
clang++ -std=c++17 -Wall -Wextra -Iswarm-core/include \
   swarm-core/src/swarm_core_math.cpp \
   swarm-core/src/swarm_core_particles.cpp \
   swarm-core/src/spatial_grid.cpp \
   swarm-core/src/arena.cpp \
   swarm-core/src/integrator.cpp \
   swarm-core/src/swarm_core.cpp \
   swarm-core/tests/test_arena.cpp \
   -o test_arena
./test_arena

clang++ -std=c++17 -Wall -Wextra -Iswarm-core/include \
   swarm-core/src/swarm_core_math.cpp \
   swarm-core/src/swarm_core_particles.cpp \
   swarm-core/src/spatial_grid.cpp \
   swarm-core/src/arena.cpp \
   swarm-core/src/integrator.cpp \
   swarm-core/src/swarm_core.cpp \
   swarm-core/tests/test_update.cpp \
   -o test_update
./test_update

clang++ -std=c++17 -Wall -Wextra -Iswarm-core/include \
   swarm-core/src/swarm_core_math.cpp \
   swarm-core/src/swarm_core_particles.cpp \
   swarm-core/src/spatial_grid.cpp \
   swarm-core/src/arena.cpp \
   swarm-core/src/integrator.cpp \
   swarm-core/src/swarm_core.cpp \
   swarm-core/tests/test_spatial_grid.cpp \
   -o test_spatial_grid
./test_spatial_grid
```

## Determinism Note

Do not add `-ffast-math` or `/fp:fast` to test or library builds. The Euler step
in `swarm-core/src/integrator.cpp` depends on the compiler not contracting
`v * dt + p` into a fused multiply-add.
