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

// CORE-7: Spatial grid scaffolding configuration. See §12 of the architecture
// document. cellCount is *derived* as gridCols * gridRows; it is never inferred
// from a runtime heuristic.
constexpr u32 SWARM_CORE_DEFAULT_GRID_COLS = 64u;
constexpr u32 SWARM_CORE_DEFAULT_GRID_ROWS = 64u;

// Validation limits. Chosen to keep cellCount safely inside u32 and to bound
// the worst-case arena byte budget at compile time. Realistic Plexus grids are
// far smaller than these caps.
constexpr u32 SWARM_CORE_MAX_GRID_COLS  = 4096u;
constexpr u32 SWARM_CORE_MAX_GRID_ROWS  = 4096u;
constexpr u32 SWARM_CORE_MAX_GRID_CELLS = 1u << 20;  // 1,048,576 cells

struct SpatialGridConfig {
    u32 gridCols;
    u32 gridRows;
};

struct Config {
    u32 maxParticles;
    SpatialGridConfig spatialGrid;
};

void zero(ParticleStateSoA& state);

} // namespace swarm::core

#endif // SWARM_CORE_PARTICLES_H
