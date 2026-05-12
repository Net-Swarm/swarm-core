#ifndef SWARM_CORE_PARTICLES_H
#define SWARM_CORE_PARTICLES_H

#include "swarm_core/swarm_core_types.h"

namespace swarm::core {

// Pointer-backed Structure-of-Arrays particle state. Field order is part of the
// public ABI v1 contract — see Docs/CORE_EXTRACTION_ARCHITECTURE.md §6. Pointer
// fields are bound by the arena (CORE-5); this ticket only defines the shape.
struct ParticleStateSoA {
    f32* posX;    // ordinal 0
    f32* posY;    // ordinal 1
    f32* velX;    // ordinal 2
    f32* velY;    // ordinal 3
    f32* colorR;  // ordinal 4
    f32* colorG;  // ordinal 5
    f32* colorB;  // ordinal 6
    f32* colorA;  // ordinal 7
    f32* life;    // ordinal 8
    u32* flags;   // ordinal 9

    u32 count;     // live particle count; invariant: count <= capacity
    u32 capacity;  // immutable after init; equals Config::maxParticles
};

struct Config {
    u32 maxParticles;
};

void zero(ParticleStateSoA& state);

} // namespace swarm::core

#endif // SWARM_CORE_PARTICLES_H
