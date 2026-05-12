#ifndef SWARM_CORE_H
#define SWARM_CORE_H

// Top-level public ABI for swarm-core. Every consumer (native shell, Wasm
// bridge) consumes simulation memory through this header alone. Layout,
// alignment, and ownership rules are defined in
// Docs/CORE_EXTRACTION_ARCHITECTURE.md (sections 4, 6, 7, 8).
//
// All public functions are declared with extern "C" linkage so the same
// header works for native C++ callers and for an Emscripten C bridge.

#include <stdint.h>
#include <stdbool.h>

#include "swarm_core/version.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle.
bool swarm_core_init(uint32_t maxParticles);
void swarm_core_update(float deltaTime);
void swarm_core_reset(void);
void swarm_core_shutdown(void);

// Capacity & count.
uint32_t swarm_core_max_particles(void);
uint32_t swarm_core_particle_count(void);

// SoA buffer accessors. Pointer identity is stable from init to shutdown.
float*    swarm_core_buffer_posX(void);
float*    swarm_core_buffer_posY(void);
float*    swarm_core_buffer_velX(void);
float*    swarm_core_buffer_velY(void);
float*    swarm_core_buffer_colorR(void);
float*    swarm_core_buffer_colorG(void);
float*    swarm_core_buffer_colorB(void);
float*    swarm_core_buffer_colorA(void);
float*    swarm_core_buffer_life(void);
uint32_t* swarm_core_buffer_flags(void);

// ABI version. Shells compare against SWARM_CORE_ABI_VERSION at runtime.
uint32_t  swarm_core_abi_version(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SWARM_CORE_H
