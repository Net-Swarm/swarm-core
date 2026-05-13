// Emscripten bridge layer for swarm-core (CORE-9).
//
// Translates the JS-facing short C ABI (swarm_*) onto the portable
// swarm-core C ABI (swarm_core_*). Adds Wasm-specific capacity policy
// (init() rejects 0 and values above SWARM_CORE_WASM_MAX_PARTICLES) and
// exposes those constants so JS consumers can query them before building
// typed-array views over Wasm linear memory.
//
// This translation unit lives outside swarm-core/src/ by design: the
// portable core must stay free of <emscripten.h> and EMSCRIPTEN_KEEPALIVE
// per Docs/CORE_EXTRACTION_ARCHITECTURE.md §10.
//
// Buffer accessors return raw pointers; under Emscripten these surface in
// JS as integer byte offsets into Module.HEAPU8.buffer, which is exactly
// the contract callers (CORE-10/11) need for non-copying typed-array views.

#include "swarm_core/swarm_core.h"
#include "swarm_core/version.h"

#include <emscripten.h>
#include <stdint.h>

namespace {

constexpr uint32_t SWARM_CORE_WASM_DEFAULT_PARTICLES = 16384u;
constexpr uint32_t SWARM_CORE_WASM_MAX_PARTICLES     = 131072u;

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
int swarm_init(uint32_t maxParticles) {
    if (maxParticles == 0u || maxParticles > SWARM_CORE_WASM_MAX_PARTICLES) {
        return 0;
    }
    return swarm_core_init(maxParticles) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
void swarm_shutdown(void) {
    swarm_core_shutdown();
}

EMSCRIPTEN_KEEPALIVE
int swarm_reset(void) {
    if (swarm_core_max_particles() == 0u) {
        return 0;
    }
    swarm_core_reset();
    return 1;
}

EMSCRIPTEN_KEEPALIVE
int swarm_update(float deltaTime) {
    if (swarm_core_max_particles() == 0u) {
        return 0;
    }
    swarm_core_update(deltaTime);
    return 1;
}

EMSCRIPTEN_KEEPALIVE
uint32_t swarm_get_particle_count(void) {
    return swarm_core_particle_count();
}

EMSCRIPTEN_KEEPALIVE
uint32_t swarm_get_particle_capacity(void) {
    return swarm_core_max_particles();
}

EMSCRIPTEN_KEEPALIVE
uint32_t swarm_get_default_particles(void) {
    return SWARM_CORE_WASM_DEFAULT_PARTICLES;
}

EMSCRIPTEN_KEEPALIVE
uint32_t swarm_get_max_supported_particles(void) {
    return SWARM_CORE_WASM_MAX_PARTICLES;
}

EMSCRIPTEN_KEEPALIVE float*    swarm_get_pos_x(void)   { return swarm_core_buffer_posX();   }
EMSCRIPTEN_KEEPALIVE float*    swarm_get_pos_y(void)   { return swarm_core_buffer_posY();   }
EMSCRIPTEN_KEEPALIVE float*    swarm_get_vel_x(void)   { return swarm_core_buffer_velX();   }
EMSCRIPTEN_KEEPALIVE float*    swarm_get_vel_y(void)   { return swarm_core_buffer_velY();   }
EMSCRIPTEN_KEEPALIVE float*    swarm_get_color_r(void) { return swarm_core_buffer_colorR(); }
EMSCRIPTEN_KEEPALIVE float*    swarm_get_color_g(void) { return swarm_core_buffer_colorG(); }
EMSCRIPTEN_KEEPALIVE float*    swarm_get_color_b(void) { return swarm_core_buffer_colorB(); }
EMSCRIPTEN_KEEPALIVE float*    swarm_get_color_a(void) { return swarm_core_buffer_colorA(); }
EMSCRIPTEN_KEEPALIVE float*    swarm_get_life(void)    { return swarm_core_buffer_life();   }
EMSCRIPTEN_KEEPALIVE uint32_t* swarm_get_flags(void)   { return swarm_core_buffer_flags();  }

EMSCRIPTEN_KEEPALIVE
uint32_t swarm_get_abi_version(void) {
    return swarm_core_abi_version();
}

} // extern "C"
