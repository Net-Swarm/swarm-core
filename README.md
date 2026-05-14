# swarm-core

`swarm-core` is the portable C++ simulation core for the Swarm projects. It owns
the particle memory contract, deterministic update step, native static library
target, and Emscripten bridge artifact consumed by shell projects.

## Current Status

- CORE-2 tracks the extraction epic and cross-repo integration scope.
- CORE-3 through CORE-9 are implemented in this repository.
- ABI baseline: `SWARM_CORE_ABI_VERSION = 1`.
- `net-swarm` is a separate repository. Its NET-52 bootloader work is complete,
  and its NET-53 render bridge is planned/Todo. Both consume the CORE-2 contract
  but are not implemented in this repository.

## Contract Summary

The public native ABI is declared in `swarm-core/include/swarm_core/swarm_core.h`.
Consumers initialize a fixed-capacity core, call the deterministic update step,
and read stable Structure-of-Arrays buffers through named accessors.

Key invariants:

- Capacity is fixed by `swarm_core_init(uint32_t maxParticles)`.
- Particle state is stored as ten SoA buffers: `posX`, `posY`, `velX`, `velY`,
  `colorR`, `colorG`, `colorB`, `colorA`, `life`, and `flags`.
- Exported buffer pointers are 16-byte aligned and stable until shutdown.
- `swarm_core_update(float deltaTime)` mutates positions in place and does not
  allocate memory.
- Spatial-grid storage is arena-backed and internal-only in ABI v1.
- `swarm_core_abi_version()` returns the runtime ABI version for shell checks.

The full memory and ABI contract lives in
`Docs/CORE_EXTRACTION_ARCHITECTURE.md`.

## Native Build

Requires CMake 3.20+ and a C++17 compiler.

```powershell
cmake -S . -B build -DSWARM_CORE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -C Debug
```

The top-level target is `swarm-core`, with alias `swarm::core`. It builds a
static library (`swarm-core.lib` on MSVC, or the platform-equivalent archive).

## Wasm Build

Requires the Emscripten SDK.

```powershell
emcmake cmake -S . -B build-wasm -DSWARM_CORE_BUILD_WASM=ON
cmake --build build-wasm
node swarm-core\bridge\emscripten\smoke\smoke.mjs build-wasm\swarm-core.js
```

The Wasm target produces:

- `build-wasm/swarm-core.js`
- `build-wasm/swarm-core.wasm`

The bridge uses a fixed 64 MiB initial linear memory and
`ALLOW_MEMORY_GROWTH=0`, so browser consumers can create stable typed-array
views over exported buffer offsets.

## Documentation

- `ARCHITECTURE.md`: core/shell ownership model.
- `Docs/CORE_EXTRACTION_ARCHITECTURE.md`: v1 public memory contract and ABI.
- `swarm-core/tests/README.md`: native contract test entry points.
- `swarm-core/bridge/emscripten/README.md`: Wasm bridge build and exported ABI.
