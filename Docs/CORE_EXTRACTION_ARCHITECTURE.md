# CORE EXTRACTION ARCHITECTURE & PUBLIC MEMORY CONTRACT

> **Status:** Phase 1 — Data Model & The Arena
> **Scope:** Contract definition only. No implementation.
> **Tracking:** [CORE-2 epic] / CORE-3
> **ABI baseline:** `SWARM_CORE_ABI_VERSION = 1`

---

## 1. Purpose & Scope

This document defines the **public contract** between `swarm-core` and every shell that consumes it (the DX12 native shell `swarm-engine`, the Wasm/Next.js web shell `net-swarm`, and any future consumer). It is the source of truth for buffer layout, ownership, alignment, lifecycle, and dependency boundaries.

This document is **not**:
- An implementation guide. No allocator, integrator, or bridge code is specified here — those land in CORE-4 through CORE-9.
- A build manual. CMake and Emscripten targets are defined in CORE-8 / CORE-9.
- A replacement for [`ARCHITECTURE.md`](../ARCHITECTURE.md). That document describes the conceptual Core/Shell doctrine; this one defines the technical contract that doctrine implies.

Every subsequent Phase 1 ticket (CORE-4, CORE-5, CORE-6, CORE-7) and every shell-bridge ticket (CORE-8, CORE-9, CORE-10, CORE-11) implements *against* this contract. If implementation diverges from this document, the document is wrong and must be revised before code lands — not the other way around.

---

## 2. Module Boundary

`swarm-core` is a self-contained C++ module. Its intended on-disk layout:

```
swarm-core/
├── include/
│   └── swarm_core/        # PUBLIC headers — part of the ABI contract
│       ├── swarm_core.h   # Top-level entry point (init, update, reset, shutdown, buffer accessors)
│       └── version.h      # SWARM_CORE_ABI_VERSION constant
├── src/                   # PRIVATE implementation — not part of the ABI contract
│   └── (arena, math, integrator, spatial grid, etc.)
└── tests/                 # Internal tests; not consumed by shells
```

**Rule of contract:**
- Anything declared in `include/swarm_core/` is part of the public contract. Breaking changes here require a `SWARM_CORE_ABI_VERSION` bump (see §9).
- Anything in `src/` is private. Layouts, classes, helper functions, and internal types may change at any time without notice. Shells **must not** include `src/` headers.

Shells link against the produced static library (native) or the Wasm module (web) and consume only `include/swarm_core/`.

---

## 3. Portable Core vs. Shell-Specific Integration

| Concern | Belongs in `swarm-core` (portable C++) | Belongs in a shell |
|---|---|---|
| Particle math (vec2, scalar ops) | ✅ Core | ❌ |
| SoA particle state buffers | ✅ Core | ❌ |
| Euler integration / `Update(dt)` | ✅ Core | ❌ |
| Fixed-capacity arena allocator | ✅ Core | ❌ |
| Spatial partitioning (cell grid) | ✅ Core | ❌ |
| Plexus distance checks | ✅ Core | ❌ |
| DirectX 12 device / swapchain | ❌ | ✅ Native shell |
| ImGui debug UI | ❌ | ✅ Native shell |
| Win32 / OS windowing | ❌ | ✅ Native shell |
| Emscripten `EMSCRIPTEN_KEEPALIVE` exports | ❌ | ✅ Web shell bridge layer |
| React / Next.js UI controls | ❌ | ✅ Web shell |
| `requestAnimationFrame` loop | ❌ | ✅ Web shell |
| WebGL / Canvas rendering | ❌ | ✅ Web shell |
| `Float32Array` / `Uint32Array` views | ❌ | ✅ Web shell (over exported Wasm memory) |
| Asset loading, file I/O | ❌ | ✅ Shell |

The core is rendering-agnostic, OS-agnostic, and UI-agnostic. It computes; shells display.

---

## 4. Public C/C++ API Surface (Initial Sketch)

This section defines the **shape** of the public API. Function bodies are not part of this ticket; signatures and intent are. All public functions use `extern "C"` linkage to allow stable consumption by both native C++ callers and Emscripten exports.

### Lifecycle (mandatory for v1)

| Symbol | Purpose |
|---|---|
| `bool swarm_core_init(uint32_t maxParticles)` | Allocate the arena. Fixes capacity for the lifetime of the instance. Returns `false` on allocation failure. Must be called before any other function. |
| `void swarm_core_update(float deltaTime)` | Advance the simulation by `deltaTime` seconds. Mutates the SoA buffers in place. |
| `void swarm_core_reset(void)` | Set `particleCount` to 0. Preserves arena, capacity, and exported pointer identity. |
| `void swarm_core_shutdown(void)` | Release the arena. After this call, all previously exported pointers are invalid. |

### Capacity & Count Accessors (mandatory for v1)

| Symbol | Purpose |
|---|---|
| `uint32_t swarm_core_max_particles(void)` | Returns the `maxParticles` value passed to `swarm_core_init`. |
| `uint32_t swarm_core_particle_count(void)` | Returns the current live particle count. Always `≤ swarm_core_max_particles()`. |

### Buffer Accessors (mandatory for v1)

Each accessor returns a stable pointer to the head of one SoA buffer. See §6 for layout, §7 for alignment, §8 for ownership.

| Symbol | Returns |
|---|---|
| `float*    swarm_core_buffer_posX(void)` | Pointer to `posX[0]`. |
| `float*    swarm_core_buffer_posY(void)` | Pointer to `posY[0]`. |
| `float*    swarm_core_buffer_velX(void)` | Pointer to `velX[0]`. |
| `float*    swarm_core_buffer_velY(void)` | Pointer to `velY[0]`. |
| `float*    swarm_core_buffer_colorR(void)` | Pointer to `colorR[0]`. |
| `float*    swarm_core_buffer_colorG(void)` | Pointer to `colorG[0]`. |
| `float*    swarm_core_buffer_colorB(void)` | Pointer to `colorB[0]`. |
| `float*    swarm_core_buffer_colorA(void)` | Pointer to `colorA[0]`. |
| `float*    swarm_core_buffer_life(void)` | Pointer to `life[0]`. |
| `uint32_t* swarm_core_buffer_flags(void)` | Pointer to `flags[0]`. |

### Versioning (mandatory for v1)

| Symbol | Purpose |
|---|---|
| `uint32_t swarm_core_abi_version(void)` | Returns `SWARM_CORE_ABI_VERSION`. See §9. |

### Reserved for Later Phases

These are *not* part of the v1 contract and shells must not depend on them yet:
- Per-particle emission/spawning (Phase 2+)
- Spatial-grid query accessors (CORE-7 scaffolds the storage; queries land later)
- Telemetry counters
- Configurable integration parameters beyond `deltaTime`

---

## 5. Fixed-Capacity Memory Model

The core enforces a **Zero-GC, Zero-Growth** doctrine. The rules:

1. **Capacity is fixed at `swarm_core_init`.** `maxParticles` is the immutable upper bound for the life of the instance. There is no `swarm_core_resize` and there never will be in v1.
2. **One arena, one allocation.** All SoA buffers, metadata, and spatial-grid storage live inside a single contiguous arena allocated once at init.
3. **No runtime heap traffic.** No `new`, `malloc`, `realloc`, `std::vector::push_back`, or any other allocating call may execute inside `swarm_core_update`, `swarm_core_reset`, or any buffer accessor.
4. **`particleCount ≤ maxParticles`** is an inviolable invariant. The core enforces it; shells must not write past `particleCount`.
5. **Capacity vs. count distinction:**
   - `maxParticles` (capacity) is set at `Init` and never changes.
   - `particleCount` (count) is a runtime value, always `≤ maxParticles`, and is the live-particle high-water mark within the buffers.

This contract exists because **Wasm memory cannot grow safely under a JS bridge.** Emscripten's `ALLOW_MEMORY_GROWTH=1` detaches every typed-array view in JS the moment the heap expands, crashing the render loop. Fixed memory at boot is the only way to guarantee a stable bridge. The same fixed-budget discipline also benefits the native shell — predictable working sets, no GC stutter, cache-friendly access.

---

## 6. Public Memory Contract — `ParticleStateSoA`

The particle state is exposed as ten independent buffers in **Structure of Arrays** layout. SoA is chosen over Array of Structs because the Plexus distance checks and integration step read tight subsets of the fields (position-only, position+velocity) — dense linear scans through homogeneous-type arrays are dramatically faster than strided scans through mixed-type structs.

### Canonical buffer order (ABI v1)

| Ordinal | Field | Element type | Element count | Stride (bytes) |
|---|---|---|---|---|
| 0 | `posX`   | `float` (float32)  | `maxParticles` | 4 |
| 1 | `posY`   | `float` (float32)  | `maxParticles` | 4 |
| 2 | `velX`   | `float` (float32)  | `maxParticles` | 4 |
| 3 | `velY`   | `float` (float32)  | `maxParticles` | 4 |
| 4 | `colorR` | `float` (float32)  | `maxParticles` | 4 |
| 5 | `colorG` | `float` (float32)  | `maxParticles` | 4 |
| 6 | `colorB` | `float` (float32)  | `maxParticles` | 4 |
| 7 | `colorA` | `float` (float32)  | `maxParticles` | 4 |
| 8 | `life`   | `float` (float32)  | `maxParticles` | 4 |
| 9 | `flags`  | `uint32_t`         | `maxParticles` | 4 |

### Rules

- **Field-by-name access.** Shells reference buffers via the named accessors in §4, never by ordinal index or by computing offsets from a base pointer. The ordinal column above is informational and may be reordered in future ABI versions.
- **No interior padding.** Element `i` of any buffer is at byte offset `i * stride` from the buffer base pointer.
- **No struct-of-arrays-of-structs.** Each buffer is a flat, contiguous array of its element type. There is no parent `ParticleStateSoA` struct exposed publicly; the SoA is a *pattern*, not a public type.
- **Scalar element types.** The default public element type for all scalar buffers is **`float32`** (IEEE-754 single precision), except `flags`, which is `uint32_t`. Future ticket-level changes to element type require an ABI bump.
- **`flags` semantics.** Bit definitions for `flags` are reserved and will be specified in a later ticket. Bits are zero-initialized on `swarm_core_init` and on `swarm_core_reset`.

---

## 7. Alignment Requirements

Every exported buffer pointer is **16-byte aligned**.

- **Why 16:** Matches the SSE/NEON 128-bit SIMD width on every supported native target. Matches the Wasm `v128` SIMD type. Does not force AVX-only (32-byte) padding that would inflate memory on Wasm without benefit.
- **What this guarantees:** Auto-vectorizers and explicit SIMD intrinsics in core code can issue aligned 128-bit loads/stores against any exported buffer's first element. Shells that read buffers as `Float32Array` / `Uint32Array` (web) or `__m128` / `float32x4_t` (native) can do so without alignment trapping.
- **What this does *not* guarantee:** 32-byte (AVX) or 64-byte (cache-line) alignment. Implementations may exceed 16-byte alignment incidentally, but shells must not depend on more than 16.
- **Interior elements:** Element `i` of a buffer is naturally aligned to its element size (4 bytes for `float`/`uint32_t`). No interior padding (see §6).

The arena allocator (CORE-5) is responsible for placing each buffer's base pointer on a 16-byte boundary.

---

## 8. Ownership & Lifecycle Rules

### Who owns what

- **`swarm-core` owns all simulation memory.** The arena and every SoA buffer inside it are allocated, written, and freed exclusively by the core.
- **Shells consume views.** A shell receives a `(pointer, element type, element count)` triple via the accessors. That triple is a *view* — read access only — and grants no ownership.

### Pointer stability

- Buffer pointers returned by the accessors in §4 are **stable for the entire lifetime of the core instance** from the moment `swarm_core_init` returns until `swarm_core_shutdown` is called.
- `swarm_core_update` does **not** move buffers.
- `swarm_core_reset` does **not** move buffers; it only zeroes `particleCount` (and may zero buffer contents — see below).
- This stability is what makes the Wasm bridge safe: a JS `Float32Array` constructed once over `swarm_core_buffer_posX()` remains valid for the life of the module.

### What shells must not do

- **Must not** call `free`, `delete`, `realloc`, or any deallocator on an exported pointer.
- **Must not** retain a pointer across `swarm_core_shutdown`. After shutdown all exported pointers are invalid; using one is undefined behavior.
- **Must not** write through an exported pointer. v1 exports are read-only contracts. (A future ticket may add explicit write-back APIs; until then, do not write.)
- **Must not** depend on any private `src/` type, even if it appears in a public header indirectly.

### Read timing

- Shells **may** read exported buffers at any time between `swarm_core_init` returning and `swarm_core_shutdown` being called, **except** during a call to `swarm_core_update`.
- The intended pattern: shell calls `swarm_core_update(dt)`, waits for it to return, then reads buffers to render the frame.
- Concurrent reads during `swarm_core_update` are **undefined** in v1. The core does not promise mid-update consistency. A future ticket may add a double-buffered read API; until then, read only after update returns.

### Reset semantics

- `swarm_core_reset` sets `particleCount` to 0 and clears `flags[0..maxParticles)` to 0. Other buffer contents are unspecified after reset and should be treated as garbage until written.
- Arena, capacity, and pointer identity are preserved across reset.

---

## 9. Versioning — `SWARM_CORE_ABI_VERSION`

The public contract is versioned with a single monotonically-increasing unsigned integer:

```c
// include/swarm_core/version.h
#define SWARM_CORE_ABI_VERSION 1u
```

queryable at runtime via `swarm_core_abi_version()`.

### When to bump

`SWARM_CORE_ABI_VERSION` is bumped on **any** of the following:

- A new buffer is added, removed, or reordered.
- An existing buffer's element type changes (e.g., `float` → `double`).
- Alignment requirements increase (e.g., 16 → 32).
- A lifecycle ownership rule changes (e.g., shells gain write permission).
- A public function signature changes.
- The semantics of an existing public function changes in a non-additive way.

It is **not** bumped for:
- Internal `src/` refactors that do not affect any `include/` header.
- Performance improvements with no behavioral change.
- Documentation-only edits.

### Shell responsibility

Shells should record the `SWARM_CORE_ABI_VERSION` they were compiled against and compare it to `swarm_core_abi_version()` at runtime. A mismatch is a fatal error — the shell should refuse to run rather than silently consume buffers laid out differently than it expects.

### Initial version

`SWARM_CORE_ABI_VERSION = 1`. This document defines that version.

---

## 10. Forbidden Dependencies in `swarm-core`

The core remains platform-agnostic. The following are **forbidden** inside `swarm-core/include/` and `swarm-core/src/` in Phase 1:

- **Rendering:** DirectX (`<d3d12.h>`, `<dxgi.h>`, etc.), OpenGL, Vulkan, Metal, WebGL, Canvas, any GPU driver header.
- **UI:** ImGui, Dear ImGui derivatives, any GUI framework.
- **OS / windowing:** Win32 (`<windows.h>`), Cocoa, X11, GLFW, SDL.
- **Web platform:** Emscripten headers (`<emscripten.h>`, `<emscripten/bind.h>`), any `EMSCRIPTEN_KEEPALIVE` macro usage inside the core (it lives in the bridge layer, CORE-9), DOM/WebIDL APIs, browser-only intrinsics.
- **Web framework:** React, Next.js, any JS runtime.
- **Heavyweight C++ deps:** Boost, Qt, Eigen (we ship our own minimal math — CORE-4), any networking or filesystem library.

### Allowed in Phase 1

- C++ standard library headers limited to: `<cstdint>`, `<cstddef>`, `<cstring>` (for `memset`/`memcpy` at init), `<cmath>`, `<cassert>`. Additions require justification.
- Custom math types and helpers introduced by CORE-4.
- No third-party runtime libraries.

The Emscripten bridge (`wasm_bridge.cpp`, CORE-9) lives **outside** `swarm-core` — it is a shell-side translation layer that links against `swarm-core` and adds the `EMSCRIPTEN_KEEPALIVE` exports. The core itself never sees an Emscripten header.

---

## 11. Out of Scope for This Document

This ticket (CORE-3) defines the contract. It does **not** include:

- The arena allocator implementation — **CORE-5**.
- The SoA data model extraction and math port — **CORE-4**.
- The `Update(deltaTime)` Euler integrator — **CORE-6**.
- Spatial grid metadata storage — **CORE-7**.
- CMake static-library target — **CORE-8**.
- Emscripten bridge and exported C ABI for Wasm — **CORE-9**.
- React Wasm-loader hook — **CORE-10**.
- `requestAnimationFrame` loop and JS-side `Float32Array` views — **CORE-11**.

If a later ticket needs to deviate from this contract, the deviation is resolved by editing this document first (and bumping `SWARM_CORE_ABI_VERSION` if the change is breaking) before code lands.
