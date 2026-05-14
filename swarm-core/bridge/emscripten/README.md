# swarm-core Emscripten Bridge

The Emscripten bridge builds the portable C++ core into a Wasm module and a
JavaScript loader:

- `swarm-core.wasm`
- `swarm-core.js`

The bridge exposes a narrow `swarm_*` C ABI for browser consumers. It is the only
translation unit in this repository that may include `<emscripten.h>` or use
`EMSCRIPTEN_KEEPALIVE`; the portable core stays free of Emscripten dependencies.

## Cross-Repo Consumers

The browser integration lives in the separate `net-swarm` repository and is
linked to this work through CORE-2 scope.

- NET-52 is complete in `net-swarm`: it boots `swarm-core.js` /
  `swarm-core.wasm`, validates ABI `1`, initializes fixed capacity, and reads
  metadata.
- NET-53 is planned/Todo in `net-swarm`: it will create typed-array views over
  exported SoA offsets and render from a browser-owned animation loop.

This repository owns the bridge artifact and ABI. `net-swarm` owns browser boot,
rendering, DOM integration, and lifecycle.

## Build

Requires the Emscripten SDK.

```powershell
emcmake cmake -S . -B build-wasm -DSWARM_CORE_BUILD_WASM=ON
cmake --build build-wasm
```

Outputs in `build-wasm/`:

- `swarm-core.js`: Emscripten loader factory named `createSwarmCoreModule`.
- `swarm-core.wasm`: Wasm module.

## Capacity Policy

| Constant | Value | Export |
|---|---:|---|
| Default proof-of-life capacity | `16384` | `swarm_get_default_particles()` |
| Hard supported cap in Wasm v1 | `131072` | `swarm_get_max_supported_particles()` |
| Initial Wasm linear memory | `67108864` bytes | `-sINITIAL_MEMORY` |

`swarm_init(maxParticles)` rejects `0` and values above `131072`. The Wasm heap
is fixed with `ALLOW_MEMORY_GROWTH=0`; consumers must not rely on heap growth or
memory-growth fallbacks.

## Exported C ABI

Source names do not include a leading underscore. Direct JavaScript calls through
the generated module use Emscripten's underscore-prefixed export names, for
example `Module._swarm_init(...)`. `ccall` and `cwrap` use the source names.

### Lifecycle

| Function | Return | Notes |
|---|---|---|
| `int swarm_init(uint32_t maxParticles)` | `1` on success, `0` on rejection | Rejects `0` and `> 131072` before delegating to the core. |
| `void swarm_shutdown(void)` | none | Releases the arena. Idempotent. |
| `int swarm_reset(void)` | `1` on success, `0` if uninitialized | Clears count and preserves buffer offset identity. |
| `int swarm_update(float deltaTime)` | `1` on success, `0` if uninitialized | Runs one deterministic Euler step. |

### Capacity and Count

| Function | Return |
|---|---|
| `uint32_t swarm_get_particle_count(void)` | Live particle count. |
| `uint32_t swarm_get_particle_capacity(void)` | Fixed capacity, or `0` before init/after shutdown. |
| `uint32_t swarm_get_default_particles(void)` | `16384`. |
| `uint32_t swarm_get_max_supported_particles(void)` | `131072`. |

### Buffer Offsets

Each function returns the corresponding core pointer. In JavaScript this appears
as an integer byte offset into `Module.HEAPU8.buffer`.

| Function | Element type | View constructor |
|---|---|---|
| `float* swarm_get_pos_x(void)` | `float` | `Float32Array` |
| `float* swarm_get_pos_y(void)` | `float` | `Float32Array` |
| `float* swarm_get_vel_x(void)` | `float` | `Float32Array` |
| `float* swarm_get_vel_y(void)` | `float` | `Float32Array` |
| `float* swarm_get_color_r(void)` | `float` | `Float32Array` |
| `float* swarm_get_color_g(void)` | `float` | `Float32Array` |
| `float* swarm_get_color_b(void)` | `float` | `Float32Array` |
| `float* swarm_get_color_a(void)` | `float` | `Float32Array` |
| `float* swarm_get_life(void)` | `float` | `Float32Array` |
| `uint32_t* swarm_get_flags(void)` | `uint32_t` | `Uint32Array` |

Spatial-grid buffers are intentionally not exported in ABI v1.

### ABI Version

| Function | Return |
|---|---|
| `uint32_t swarm_get_abi_version(void)` | Mirrors `SWARM_CORE_ABI_VERSION`; current value is `1`. |

Consumers should compare this value with their expected ABI once after module
instantiation and fail fast on mismatch.

## JavaScript Usage

```js
import createSwarmCoreModule from './swarm-core.js';

const Module = await createSwarmCoreModule();

if (Module._swarm_get_abi_version() !== 1) {
  throw new Error('Unsupported swarm-core ABI');
}

const capacity = Module._swarm_get_default_particles();
if (Module._swarm_init(capacity) !== 1) {
  throw new Error('swarm_init failed');
}

const heap = Module.HEAPU8.buffer;
const posX = new Float32Array(heap, Module._swarm_get_pos_x(), capacity);
const posY = new Float32Array(heap, Module._swarm_get_pos_y(), capacity);
const flags = new Uint32Array(heap, Module._swarm_get_flags(), capacity);

Module._swarm_update(1 / 60);
Module._swarm_shutdown();
```

Browser consumers should create non-copying typed-array views over Wasm memory.
They must not mirror particle positions, velocities, colors, life, or flags into
JavaScript-owned particle state each frame.

## Smoke Harness

```powershell
node swarm-core\bridge\emscripten\smoke\smoke.mjs build-wasm\swarm-core.js
```

The harness verifies ABI version, capacity policy, rejection paths, pointer
alignment/uniqueness, typed-array view length, and pointer stability across
`update` and `reset`.

CTest registers this harness when `SWARM_CORE_BUILD_WASM=ON`,
`SWARM_CORE_BUILD_TESTS=ON`, and `node` is on `PATH`.

## Invariants

- `ALLOW_MEMORY_GROWTH=0`.
- Initial memory is fixed at 64 MiB unless the CMake cache value is changed.
- Buffer offsets are stable from successful `swarm_init` to `swarm_shutdown`.
- Consumers read from Wasm-backed views after `swarm_update` returns.
- The bridge contains no browser DOM behavior and no renderer.
