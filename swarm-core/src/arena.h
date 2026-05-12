#ifndef SWARM_CORE_SRC_ARENA_H
#define SWARM_CORE_SRC_ARENA_H

// Internal arena interface. Not part of the public ABI — must not be included
// from outside swarm-core/src. The singleton model matches the public C ABI in
// Docs/CORE_EXTRACTION_ARCHITECTURE.md §4, which takes no handle parameter.

#include "spatial_grid.h"
#include "swarm_core/swarm_core_particles.h"
#include "swarm_core/swarm_core_types.h"

namespace swarm::core::arena {

// Allocates the backing block, binds the particle SoA and spatial-grid pointer
// fields, and seeds state.count = 0, state.capacity = config.maxParticles.
// Returns false on:
//   - config.maxParticles == 0
//   - config.spatialGrid violates §12 (validate() in spatial_grid.h)
//   - size overflow (maxParticles too large to multiply safely)
//   - std::malloc failure
//   - already initialized (double-init is rejected without touching state)
bool init(const Config& config);

// Releases the backing block and resets the SoA + grid structs to their zero
// state. Subsequent buffer accessors return nullptr until init() is called
// again.
void shutdown();

// Zeros state.count, clears the flags buffer, and clears all four spatial-grid
// buffers. Pointer identity and grid dimensions are preserved (§8, §12).
// Other particle buffers are left untouched per the contract.
void reset();

bool is_initialized();

// Internal C++ accessor for CORE-6 (integrator).
// Not exposed through the public ABI — those consumers live inside swarm-core.
ParticleStateSoA& state();

// Internal C++ accessor for CORE-7 (spatial grid). Future broad-phase code
// reads and writes through this reference. Not exposed through the public ABI.
SpatialGrid& grid();

// Internal helper for code paths that advance the live particle count
// (future spawn API, integrator). Asserts the contract invariant
// count <= capacity (§5.4) in debug builds.
void set_count(u32 newCount);

} // namespace swarm::core::arena

#endif // SWARM_CORE_SRC_ARENA_H
