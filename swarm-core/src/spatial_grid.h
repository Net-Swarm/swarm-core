#ifndef SWARM_CORE_SRC_SPATIAL_GRID_H
#define SWARM_CORE_SRC_SPATIAL_GRID_H

// Internal spatial-grid scaffolding (CORE-7). Not part of the public ABI —
// must not be included from outside swarm-core/src. The grid storage lives
// inside the arena (CORE-5); this header defines the SoA shape and the
// init-time validation helper. The actual broad-phase algorithm lands in a
// later ticket. See Docs/CORE_EXTRACTION_ARCHITECTURE.md §12.

#include "swarm_core/swarm_core_particles.h"
#include "swarm_core/swarm_core_types.h"

namespace swarm::core {

// Pointer-backed SoA spatial-grid scaffolding. Pointer fields are bound by the
// arena. cellCount = gridCols * gridRows (validated at init); all four scalars
// are immutable after init.
struct SpatialGrid {
    u32* cellStartOrHead;  // length: cellCount      (cell -> first-particle index / list head)
    u32* cellCountOrNext;  // length: cellCount      (cell -> occupancy / next-link tail)
    u32* particleCell;     // length: maxParticles   (particle -> owning cell)
    u32* particleNext;     // length: maxParticles   (particle -> next-particle-in-cell)

    u32 cellCount;
    u32 gridCols;
    u32 gridRows;
    u32 maxParticles;
};

// Nulls the four pointer fields and zeroes the four scalar fields. Mirrors the
// shape of swarm::core::zero(ParticleStateSoA&) in swarm_core_particles.cpp.
void zero(SpatialGrid& g);

// Validates the grid config per §12 of the architecture document. Returns
// false on any of:
//   - gridCols == 0 or gridRows == 0
//   - gridCols  > SWARM_CORE_MAX_GRID_COLS
//   - gridRows  > SWARM_CORE_MAX_GRID_ROWS
//   - gridCols * gridRows overflows uint32_t
//   - cellCount > SWARM_CORE_MAX_GRID_CELLS
// On success, writes the validated cellCount through outCellCount (if non-null)
// and returns true.
bool validate(const SpatialGridConfig& cfg, u32* outCellCount);

} // namespace swarm::core

#endif // SWARM_CORE_SRC_SPATIAL_GRID_H
