#include "swarm_core/swarm_core.h"

#include "arena.h"
#include "integrator.h"

using swarm::core::Config;
using swarm::core::ParticleStateSoA;
using swarm::core::SpatialGridConfig;
namespace arena = swarm::core::arena;
namespace integrator = swarm::core::integrator;

extern "C" {

bool swarm_core_init(uint32_t maxParticles) {
    // CORE-7: the public convenience init supplies the documented defaults
    // (Docs/CORE_EXTRACTION_ARCHITECTURE.md §12). Future shells that need to
    // tune grid dimensions will land via a separate init path; for now the
    // grid scaffolding is internal-only.
    return arena::init(Config{
        maxParticles,
        SpatialGridConfig{
            swarm::core::SWARM_CORE_DEFAULT_GRID_COLS,
            swarm::core::SWARM_CORE_DEFAULT_GRID_ROWS,
        },
    });
}

void swarm_core_update(float deltaTime) {
    integrator::step(deltaTime);
}

void swarm_core_reset(void) {
    arena::reset();
}

void swarm_core_shutdown(void) {
    arena::shutdown();
}

uint32_t swarm_core_max_particles(void) {
    return arena::state().capacity;
}

uint32_t swarm_core_particle_count(void) {
    return arena::state().count;
}

float* swarm_core_buffer_posX(void)   { return arena::state().posX; }
float* swarm_core_buffer_posY(void)   { return arena::state().posY; }
float* swarm_core_buffer_velX(void)   { return arena::state().velX; }
float* swarm_core_buffer_velY(void)   { return arena::state().velY; }
float* swarm_core_buffer_colorR(void) { return arena::state().colorR; }
float* swarm_core_buffer_colorG(void) { return arena::state().colorG; }
float* swarm_core_buffer_colorB(void) { return arena::state().colorB; }
float* swarm_core_buffer_colorA(void) { return arena::state().colorA; }
float* swarm_core_buffer_life(void)   { return arena::state().life; }
uint32_t* swarm_core_buffer_flags(void) { return arena::state().flags; }

uint32_t swarm_core_abi_version(void) {
    return SWARM_CORE_ABI_VERSION;
}

} // extern "C"
