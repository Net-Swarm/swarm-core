# Architecture: Core/Shell Split

The Swarm projects use a strict split between simulation ownership and shell
presentation. `swarm-core` owns deterministic particle state and exposes a small
public contract. Shell repositories consume that contract and decide how to
display or drive it.

## swarm-core

`swarm-core` is a portable C++17 library. It owns:

- Fixed-capacity arena allocation.
- Structure-of-Arrays particle buffers.
- Deterministic Euler update over the active particle range.
- Internal spatial-grid scaffolding for future broad-phase work.
- Public native C ABI in `swarm-core/include/swarm_core/swarm_core.h`.
- Emscripten bridge artifact for web shells.

The core must stay independent of rendering, UI frameworks, operating-system
windowing APIs, browser APIs, and shell-owned application state.

## Native Shell

The native shell links against the `swarm-core` static library target
(`swarm::core`). It owns desktop concerns such as windowing, graphics devices,
debug UI, input, and presentation. It must consume public headers only and must
not include private files from `swarm-core/src`.

## Web Shell

`net-swarm` is a separate vanilla TypeScript + Vite + DOM repository. It consumes
the Emscripten bridge outputs from this repository:

- `swarm-core.js`
- `swarm-core.wasm`

NET-52, in the separate `net-swarm` repo, proves boot and metadata access. NET-53,
also in `net-swarm`, is the planned render bridge that will create typed-array
views over Wasm memory and render from a browser-owned animation loop.

`swarm-core` owns the memory and ABI contract. `net-swarm` owns browser boot,
rendering, DOM integration, and lifecycle.

## Data Flow

```text
shell controls -> public C ABI / Wasm bridge -> swarm-core arena
swarm-core update -> stable SoA buffers -> shell renderer
```

Shells may read exported buffers after an update returns. They must not own,
mirror, resize, or free core particle memory.
