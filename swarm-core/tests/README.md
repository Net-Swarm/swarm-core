# swarm-core tests

Self-compiling contract checks. CORE-8 will wire these into CMake/CTest; until
then, build from the repository root:

## MSVC (Developer Command Prompt)

```
cl /std:c++17 /W4 /EHsc /Iswarm-core\include ^
   swarm-core\src\swarm_core_math.cpp ^
   swarm-core\src\swarm_core_particles.cpp ^
   swarm-core\src\arena.cpp ^
   swarm-core\src\swarm_core.cpp ^
   swarm-core\tests\test_arena.cpp ^
   /Fe:test_arena.exe
test_arena.exe
```

## Clang / GCC

```
clang++ -std=c++17 -Wall -Wextra -Iswarm-core/include \
   swarm-core/src/swarm_core_math.cpp \
   swarm-core/src/swarm_core_particles.cpp \
   swarm-core/src/arena.cpp \
   swarm-core/src/swarm_core.cpp \
   swarm-core/tests/test_arena.cpp \
   -o test_arena
./test_arena
```

Exit code 0 means every check passed. The driver prints `test_arena: OK` on
success and `FAIL <file>:<line> <expr>` for any failed assertion.
