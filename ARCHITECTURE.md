# 🏗️ ARCHITECTURE: CORE/SHELL PHILOSOPHY

### // THE DOCTRINE
In the Net-Swarm architecture, we enforce a strict separation between **Intelligence** (Core) and **Interface** (Shell).

#### 1. The Core (`swarm-core`)
The Core is a "black box" written in C++. It contains the heavy lifting:
- Particle simulation loops.
- Distance-checking (Plexus) logic.
- Telemetry data structures.

**Constraint:** Must remain Platform Agnostic. Standard C++ only.

#### 2. The Native Shell (`swarm-engine`)
Wraps the Core for desktop use. Handles OS Windowing, DirectX 12 rendering, and IMGUI debugging.

#### 3. The Web Shell (`net-swarm`)
"Beams" the Core into the browser via Emscripten. Uses Next.js for the UI shell and pipes parameters into the Wasm module via `EMSCRIPTEN_KEEPALIVE`.

### // DATA FLOW
`[Web UI Sliders] -> [JS Bridge] -> [Wasm Core] -> [Canvas Render Target]`
