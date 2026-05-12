#include "integrator.h"

#include "arena.h"

#include <cassert>
#include <cmath>

namespace swarm::core::integrator {

void step(f32 deltaTime) {
    if (!arena::is_initialized()) {
        return;
    }

    ParticleStateSoA& state = arena::state();
    assert(std::isfinite(deltaTime) && "integrator::step deltaTime must be finite");
    assert(state.count <= state.capacity &&
           "particleCount must not exceed maxParticles (CORE_EXTRACTION_ARCHITECTURE.md §5.4)");

    // Snapshot count so the optimizer can hoist the bound and so a future code
    // path that mutates count mid-call cannot retarget the loop.
    const u32 n = state.count;

    // Two-statement form (not packed) so no compiler is licensed to contract
    // the multiply-add into FMA. Bit-exact determinism across MSVC/Clang/GCC
    // requires that -ffast-math / /fp:fast stay OFF in every build target.
    for (u32 i = 0u; i < n; ++i) {
        state.posX[i] += state.velX[i] * deltaTime;
        state.posY[i] += state.velY[i] * deltaTime;
    }
}

} // namespace swarm::core::integrator
