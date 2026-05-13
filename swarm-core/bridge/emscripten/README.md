# swarm-core Emscripten Bridge

The Emscripten bridge produces a single Wasm module (`swarm-core.wasm` + the
loader `swarm-core.js`) that exposes a narrow short-name C ABI for the
Net-Swarm web shell. The portable C++ core (`swarm::core`) is built with the
Emscripten toolchain and statically linked into the bridge translation unit.

The bridge is the **only** translation unit allowed to include
`<emscripten.h>` or apply `EMSCRIPTEN_KEEPALIVE` — see
`Docs/CORE_EXTRACTION_ARCHITECTURE.md` §10.

## Build

Requires the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
(activate the latest stable: `emsdk activate latest`).

```
emcmake cmake -S . -B build-wasm -DSWARM_CORE_BUILD_WASM=ON
cmake --build build-wasm
```

Outputs (in `build-wasm/`):
- `swarm-core.js`   — Emscripten loader (factory function `createSwarmCoreModule`)
- `swarm-core.wasm` — the module itself

## Capacity policy

| Constant | Value | Source-level name |
|---|---|---|
| Default proof-of-life capacity | `16384` | `swarm_get_default_particles()` |
| Hard supported cap (v1) | `131072` | `swarm_get_max_supported_particles()` |
| Initial Wasm linear memory | `67108864` bytes (64 MiB) | `-sINITIAL_MEMORY` |

`swarm_init(maxParticles)` rejects `0` and any value above
`SWARM_CORE_WASM_MAX_PARTICLES` (returns `0`). The Wasm heap is fixed
(`ALLOW_MEMORY_GROWTH=0`); JS must not assume the heap can grow.

## Exported C ABI

All names appear in source without a leading underscore. Emscripten emits
them into the Wasm export table with an `_` prefix; that is the spelling JS
uses for direct `Module._fn(...)` calls. `Module.ccall('fn', …)` and
`Module.cwrap('fn', …)` use the no-underscore spelling.

### Lifecycle

| Function | Return | Notes |
|---|---|---|
| `int swarm_init(uint32_t maxParticles)` | `1` on success, `0` on rejection | Rejects `0` and `> 131072` before delegating to the core. |
| `void swarm_shutdown(void)` | — | Releases the arena. Idempotent. |
| `int swarm_reset(void)` | `1` on success, `0` if uninitialised | Zeroes count; preserves buffer pointer identity. |
| `int swarm_update(float deltaTime)` | `1` on success, `0` if uninitialised | One canonical Euler step (`p += v * dt`). |

### Capacity / count

| Function | Return |
|---|---|
| `uint32_t swarm_get_particle_count(void)` | Live particle count. |
| `uint32_t swarm_get_particle_capacity(void)` | `maxParticles` from `swarm_init`. `0` before init / after shutdown. |
| `uint32_t swarm_get_default_particles(void)` | `16384`. Demo/proof-of-life default. |
| `uint32_t swarm_get_max_supported_particles(void)` | `131072`. Wasm v1 hard cap. |

### Buffer offsets

Each returns the SoA buffer pointer verbatim from the core. In JS the
pointer surfaces as an **integer byte offset** into `Module.HEAPU8.buffer`.
Pointer identity is stable from `swarm_init` until `swarm_shutdown`.

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

Spatial-grid buffers (CORE-7) are intentionally **not** exported in
CORE-9. Exposing them is a separate ABI-bump ticket.

### ABI version

| Function | Return |
|---|---|
| `uint32_t swarm_get_abi_version(void)` | Mirrors `SWARM_CORE_ABI_VERSION` (currently `1`). |

JS consumers should `assert(Module._swarm_get_abi_version() === 1)` once
after instantiation. Any mismatch indicates the bridge and the consumer
have drifted.

## JS usage

```js
import createSwarmCoreModule from './swarm-core.js';

const Module = await createSwarmCoreModule();

const cap = Module._swarm_get_default_particles();
if (Module._swarm_init(cap) !== 1) {
    throw new Error('swarm_init failed');
}

// Non-copying typed-array views over Wasm linear memory.
const heap = Module.HEAPU8.buffer;
const posX = new Float32Array(heap, Module._swarm_get_pos_x(), cap);
const posY = new Float32Array(heap, Module._swarm_get_pos_y(), cap);
const flags = new Uint32Array(heap, Module._swarm_get_flags(), cap);

// Step the simulation.
Module._swarm_update(1 / 60);

// Tear down.
Module._swarm_shutdown();
```

`Module.ccall` / `Module.cwrap` are also exposed (no-underscore names) for
typed convenience wrappers.

## Smoke harness

```
node swarm-core/bridge/emscripten/smoke/smoke.mjs build-wasm/swarm-core.js
```

The harness verifies the ABI version, capacity policy, init rejection
paths, pointer alignment/uniqueness, typed-array view length, and pointer
stability across `update` and `reset`. CTest registers this automatically
when `SWARM_CORE_BUILD_WASM=ON`, `SWARM_CORE_BUILD_TESTS=ON`, and `node` is
on `PATH`.

## Invariants

- `ALLOW_MEMORY_GROWTH=0` — fixed Wasm heap; growing it would detach every
  typed-array view in the render loop.
- No `-ffast-math` / `/fp:fast` — the integrator depends on the compiler
  not fusing `p += v * dt` into an FMA (determinism contract).
- Buffer pointer identity is stable from `swarm_init` to `swarm_shutdown`;
  JS may cache views across frames.
