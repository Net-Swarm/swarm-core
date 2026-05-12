#include "swarm_core/swarm_core_particles.h"

#include <cstddef>

namespace swarm::core {

// Pin element widths required by the public memory contract
// (Docs/CORE_EXTRACTION_ARCHITECTURE.md §6).
static_assert(sizeof(f32) == 4, "ABI v1 requires float32 scalar buffers");
static_assert(sizeof(u32) == 4, "ABI v1 requires uint32_t for the flags buffer");

// Pin ordinal-0 placement. Field reordering would silently break the documented
// buffer order; the offset check fails the build instead.
static_assert(offsetof(ParticleStateSoA, posX) == 0,
              "posX must be ordinal 0 in ParticleStateSoA (ABI v1)");

void zero(ParticleStateSoA& state) {
    state.posX     = nullptr;
    state.posY     = nullptr;
    state.velX     = nullptr;
    state.velY     = nullptr;
    state.colorR   = nullptr;
    state.colorG   = nullptr;
    state.colorB   = nullptr;
    state.colorA   = nullptr;
    state.life     = nullptr;
    state.flags    = nullptr;
    state.count    = 0u;
    state.capacity = 0u;
}

} // namespace swarm::core
