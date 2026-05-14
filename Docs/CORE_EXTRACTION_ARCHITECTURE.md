# Core Extraction Architecture and Public Memory Contract

> Status: Living v1 contract implemented in `swarm-core` through CORE-9.
> Tracking: CORE-2 epic. CORE-3 through CORE-9 are complete in this repository.
> Cross-repo consumers: NET-52 is complete in the separate `net-swarm` repo;
> NET-53 is planned/Todo in `net-swarm`.
> ABI baseline: `SWARM_CORE_ABI_VERSION = 1`.

## 1. Purpose

This document is the source of truth for the `swarm-core` v1 public memory
contract. It defines the native C ABI, SoA buffer layout, ownership, alignment,
lifecycle, fixed-capacity arena rules, Wasm bridge expectations, and the boundary
between this repository and shell repositories.

`swarm-core` owns the contract and the native/Wasm artifacts. Shell repositories
consume those artifacts:

- Native shell code links the static library and includes public headers only.
- The separate `net-swarm` repo consumes `swarm-core.js` and `swarm-core.wasm`.
- NET-52 and NET-53 are cross-repo consumer work items linked by CORE-2 scope;
  they are not implementation work inside this repository.

## 2. Module Boundary

The implemented on-disk boundary is:

```text
swarm-core/
  include/swarm_core/      public headers, part of the ABI contract
  src/                     private implementation
  tests/                   internal contract tests
  bridge/emscripten/       Wasm bridge and smoke harness
Docs/
  CORE_EXTRACTION_ARCHITECTURE.md
CMakeLists.txt
```

Rules:

- Anything declared in `swarm-core/include/swarm_core/` is public contract.
- Breaking public header changes require an ABI version decision.
- Anything in `swarm-core/src/` is private and must not be included by shell
  consumers.
- The Emscripten bridge is intentionally outside `src/` so Emscripten headers and
  export macros do not enter the portable core.

## 3. Ownership Boundary

| Concern | `swarm-core` owns | Shell repos own |
|---|---:|---:|
| Particle math and scalar types | Yes | No |
| SoA particle buffers | Yes | No |
| Fixed-capacity arena | Yes | No |
| Euler update step | Yes | No |
| Internal spatial-grid storage | Yes | No |
| Native static library target | Yes | No |
| Emscripten bridge artifact | Yes | No |
| Desktop windowing/rendering/UI | No | Yes |
| Browser boot/render/DOM lifecycle | No | Yes |
| Typed-array views over Wasm memory | No | Yes |
| Per-frame browser render cadence | No | Yes |

The core computes and owns memory. Shells display, schedule, and integrate.

## 4. Native Public C ABI

The native ABI is declared in `swarm-core/include/swarm_core/swarm_core.h`.
All functions use `extern "C"` linkage.

### Lifecycle

| Symbol | Purpose |
|---|---|
| `bool swarm_core_init(uint32_t maxParticles)` | Allocate and bind the fixed-capacity arena. Returns `false` for invalid capacity, double-init, allocation failure, or grid validation failure. |
| `void swarm_core_update(float deltaTime)` | Advance active particles by one deterministic Euler step. No-op before init or after shutdown. |
| `void swarm_core_reset(void)` | Set `particleCount` to 0, clear flags, and clear internal spatial-grid buffers. Preserves arena and pointer identity. |
| `void swarm_core_shutdown(void)` | Release the arena. Public buffer accessors return null afterward. |

### Capacity and Count

| Symbol | Purpose |
|---|---|
| `uint32_t swarm_core_max_particles(void)` | Returns fixed capacity, or `0` before init/after shutdown. |
| `uint32_t swarm_core_particle_count(void)` | Returns live particle count, always `<= maxParticles`. |

### Buffer Accessors

Each accessor returns a pointer to the first element of one SoA buffer. Pointers
are stable from successful init until shutdown.

| Symbol | Returns |
|---|---|
| `float* swarm_core_buffer_posX(void)` | `posX[0]` |
| `float* swarm_core_buffer_posY(void)` | `posY[0]` |
| `float* swarm_core_buffer_velX(void)` | `velX[0]` |
| `float* swarm_core_buffer_velY(void)` | `velY[0]` |
| `float* swarm_core_buffer_colorR(void)` | `colorR[0]` |
| `float* swarm_core_buffer_colorG(void)` | `colorG[0]` |
| `float* swarm_core_buffer_colorB(void)` | `colorB[0]` |
| `float* swarm_core_buffer_colorA(void)` | `colorA[0]` |
| `float* swarm_core_buffer_life(void)` | `life[0]` |
| `uint32_t* swarm_core_buffer_flags(void)` | `flags[0]` |

### ABI Version

| Symbol | Purpose |
|---|---|
| `uint32_t swarm_core_abi_version(void)` | Returns `SWARM_CORE_ABI_VERSION`. Current value: `1`. |

## 5. Particle SoA Layout

ABI v1 exposes ten particle buffers in Structure-of-Arrays layout.

| Ordinal | Field | Element type | Element count | Stride bytes |
|---:|---|---|---:|---:|
| 0 | `posX` | `float` | `maxParticles` | 4 |
| 1 | `posY` | `float` | `maxParticles` | 4 |
| 2 | `velX` | `float` | `maxParticles` | 4 |
| 3 | `velY` | `float` | `maxParticles` | 4 |
| 4 | `colorR` | `float` | `maxParticles` | 4 |
| 5 | `colorG` | `float` | `maxParticles` | 4 |
| 6 | `colorB` | `float` | `maxParticles` | 4 |
| 7 | `colorA` | `float` | `maxParticles` | 4 |
| 8 | `life` | `float` | `maxParticles` | 4 |
| 9 | `flags` | `uint32_t` | `maxParticles` | 4 |

Rules:

- Shells access buffers by named accessor, not by computing offsets from a base
  pointer.
- Each buffer is flat and contiguous.
- There is no public parent struct that shells may depend on for layout.
- `flags` bit definitions remain reserved in ABI v1.
- The internal `ParticleStateSoA` type lives in public headers for core/module
  composition, but public C consumers should treat the named accessors as the
  stable ABI.

## 6. Fixed-Capacity Memory Model

The memory model is fixed-capacity and fixed-address for the lifetime of an
initialized instance.

Rules:

- `maxParticles` is fixed by `swarm_core_init`.
- There is no resize API in ABI v1.
- The core uses one arena allocation at init to bind particle SoA buffers and
  internal spatial-grid buffers.
- No runtime heap allocation may occur in `swarm_core_update`,
  `swarm_core_reset`, or buffer accessors.
- `particleCount <= maxParticles` is enforced as a core invariant.
- `std::malloc` and `std::free` are permitted only for the single arena
  allocation/release path at init/shutdown. This is why `<cstdlib>` appears in
  `swarm-core/src/arena.cpp`.

This rule is required for Wasm consumers. If Emscripten memory grows, existing
JavaScript typed-array views can detach. Therefore the bridge is built with
`ALLOW_MEMORY_GROWTH=0`.

## 7. Alignment

Every exported particle buffer pointer is 16-byte aligned.

The guarantee applies to the base pointer returned by each buffer accessor. It
supports 128-bit SIMD-friendly loads in native code and Wasm `v128` use. ABI v1
does not guarantee 32-byte or 64-byte alignment.

## 8. Lifecycle and Read Rules

Pointer lifecycle:

- Pointers are valid from successful `swarm_core_init` until
  `swarm_core_shutdown`.
- `swarm_core_update` preserves pointer identity.
- `swarm_core_reset` preserves pointer identity.
- After shutdown, old pointers are invalid and accessors return null.

Shell read rules:

- Shells may read buffers after init and after each update returns.
- Shells must not read concurrently with `swarm_core_update`.
- Shells must not free, reallocate, or retain pointers across shutdown.
- ABI v1 treats shell-facing buffer access as a view contract; explicit write
  APIs can be added later if needed.

Reset behavior:

- `particleCount` becomes 0.
- `flags[0..maxParticles)` is cleared to 0.
- Internal spatial-grid buffers are cleared to 0.
- Other particle buffers are unspecified after reset until written again.

## 9. Deterministic Update

`swarm_core_update(float deltaTime)` delegates to the internal integrator and
updates only the active range `[0, particleCount)`.

Current v1 behavior:

```text
posX[i] += velX[i] * deltaTime
posY[i] += velY[i] * deltaTime
```

The update does not touch `life`, `flags`, color buffers, capacity, pointer
identity, or spatial-grid buffers. The build must not use `-ffast-math` or
`/fp:fast`, because the deterministic test contract depends on avoiding
multiply-add contraction.

## 10. Internal Spatial Grid

CORE-7 reserves internal spatial-grid storage in the arena for future broad-phase
work. ABI v1 does not export grid pointers.

Defaults used by `swarm_core_init(uint32_t maxParticles)`:

```c
SWARM_CORE_DEFAULT_GRID_COLS = 64
SWARM_CORE_DEFAULT_GRID_ROWS = 64
```

Validation rejects:

- `gridCols == 0`
- `gridRows == 0`
- `gridCols > SWARM_CORE_MAX_GRID_COLS` (`4096`)
- `gridRows > SWARM_CORE_MAX_GRID_ROWS` (`4096`)
- `gridCols * gridRows` overflow
- `cellCount > SWARM_CORE_MAX_GRID_CELLS` (`1048576`)

Internal buffers:

| Buffer | Element count | Purpose |
|---|---:|---|
| `cellStartOrHead` | `cellCount` | First particle index per cell, or list head |
| `cellCountOrNext` | `cellCount` | Occupancy per cell, or list-tail link |
| `particleCell` | `maxParticles` | Owning cell per particle |
| `particleNext` | `maxParticles` | Next particle in cell |

These buffers are 16-byte aligned, stable during the arena lifetime, and cleared
on init/reset. They are intentionally internal-only in ABI v1.

## 11. Wasm Bridge Contract

The Emscripten bridge lives in `swarm-core/bridge/emscripten/wasm_bridge.cpp`.
It links the portable core and exposes short `swarm_*` functions for JavaScript.

Build settings:

- `ALLOW_MEMORY_GROWTH=0`
- `INITIAL_MEMORY=67108864` (64 MiB)
- `MODULARIZE=1`
- `EXPORT_NAME=createSwarmCoreModule`
- Environment: `web,node`

Capacity policy:

- `swarm_get_default_particles()` returns `16384`.
- `swarm_get_max_supported_particles()` returns `131072`.
- `swarm_init(maxParticles)` rejects `0` and values above `131072`.

Buffer accessors return byte offsets into `Module.HEAPU8.buffer` when called from
JavaScript. Consumers should create `Float32Array` or `Uint32Array` views over
those offsets instead of copying whole buffers into JavaScript-owned arrays.

## 12. Versioning

The public ABI version is:

```c
#define SWARM_CORE_ABI_VERSION 1u
```

It is queryable at runtime through:

- Native: `swarm_core_abi_version()`
- Wasm bridge: `swarm_get_abi_version()`

Bump the ABI version for:

- Adding, removing, or changing public buffers.
- Changing public buffer element types or alignment guarantees.
- Changing lifecycle or ownership semantics.
- Changing public function signatures.
- Changing existing public function behavior in a non-additive way.

Do not bump for:

- Private `src/` refactors.
- Performance changes with no contract change.
- Documentation-only edits.
- Internal spatial-grid implementation changes that remain unexported.

Shells must compare the runtime ABI against their expected ABI and fail fast on
mismatch.

## 13. Forbidden Dependencies

The portable core (`swarm-core/include` and `swarm-core/src`) must not depend on:

- Rendering APIs.
- GUI frameworks.
- OS/windowing APIs.
- Browser/DOM APIs.
- Emscripten headers or export macros.
- JavaScript runtime assumptions.
- Heavy third-party C++ libraries.

Allowed standard-library use remains intentionally small. Current core code uses
headers such as `<cstdint>`, `<cstddef>`, `<cstring>`, `<cmath>`, `<cassert>`,
and `<cstdlib>` for the arena allocation path.

## 14. Future Work

Future tickets may add:

- Particle spawning/emission APIs.
- Public configuration beyond the convenience `swarm_core_init` path.
- Spatial-grid query or broad-phase output APIs.
- Telemetry counters.
- Explicit shell write APIs if needed.
- Cross-repo render consumption in `net-swarm` through NET-53.

Future public contract changes must update this document and evaluate whether
`SWARM_CORE_ABI_VERSION` must change before dependent shell work lands.
