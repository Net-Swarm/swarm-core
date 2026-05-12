# swarm-core tests

Self-compiling contract checks. CORE-8 will wire these into CMake/CTest; until
then, build each driver from the repository root.

## test_arena — CORE-5 arena contract

### MSVC (Developer Command Prompt)

```
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
```

### Clang / GCC

```
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
```

## test_update — CORE-6 integrator contract

### MSVC (Developer Command Prompt)

```
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
```

### Clang / GCC

```
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
```

## test_spatial_grid — CORE-7 spatial grid scaffolding contract

### MSVC (Developer Command Prompt)

```
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

```
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

Exit code 0 means every check passed. Each driver prints `<name>: OK` on success
and `FAIL <file>:<line> <expr>` for any failed assertion.

Determinism note: do **not** add `-ffast-math` or `/fp:fast` to these commands
or to the future CMake target. The Euler step in `swarm-core/src/integrator.cpp`
relies on the compiler not contracting `v * dt + p` into a single FMA.
