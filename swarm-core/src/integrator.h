#ifndef SWARM_CORE_SRC_INTEGRATOR_H
#define SWARM_CORE_SRC_INTEGRATOR_H

// Internal integrator interface. Not part of the public ABI — must not be
// included from outside swarm-core/src. The Euler step advances the position
// buffers from the velocity buffers over the active particle range. See
// Docs/CORE_EXTRACTION_ARCHITECTURE.md §4 for the public entry point this
// implements and CORE-6 for the contract.

#include "swarm_core/swarm_core_types.h"

namespace swarm::core::integrator {

// Advances posX[i]/posY[i] by velX[i]/velY[i] * deltaTime for every index in
// [0, state.count). No-op if the arena is not initialized — matches the
// safe-on-uninit posture of arena::reset and arena::shutdown.
void step(f32 deltaTime);

} // namespace swarm::core::integrator

#endif // SWARM_CORE_SRC_INTEGRATOR_H
