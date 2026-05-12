// Self-contained CORE-6 contract test driver. Exit 0 means every check passed.
// Once CORE-8 lands the CMake target, this driver will become a CTest case.
// Until then, build with the command in tests/README.md.

#include "swarm_core/swarm_core.h"
#include "swarm_core/version.h"

// arena.h is private (src/). Tests live inside swarm-core and may include it
// to exercise the internal invariant helper. Public consumers must not.
#include "../src/arena.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

int g_failures = 0;

void report_failure(const char* expr, const char* file, int line) {
    std::fprintf(stderr, "FAIL  %s:%d  %s\n", file, line, expr);
    ++g_failures;
}

#define CHECK(expr) do { if (!(expr)) report_failure(#expr, __FILE__, __LINE__); } while (0)

// ----------------------------------------------------------------------------
// Pre-init / post-shutdown safety. Must run FIRST so the global arena state is
// genuinely uninitialized; subsequent tests leave it shut down on exit.
// ----------------------------------------------------------------------------

void test_called_before_init_safe() {
    // No prior swarm_core_init. The is_initialized() short-circuit in
    // integrator::step must return before any pointer dereference.
    swarm_core_update(0.016f);
    CHECK(swarm_core_particle_count() == 0u);
    CHECK(swarm_core_max_particles() == 0u);
}

void test_called_after_shutdown_safe() {
    CHECK(swarm_core_init(32u));
    swarm_core_shutdown();
    swarm_core_update(0.016f); // must not crash
    CHECK(swarm_core_buffer_posX() == nullptr);
}

// ----------------------------------------------------------------------------
// Canonical Euler step. The ticket's named acceptance check.
// ----------------------------------------------------------------------------

void test_canonical_euler_step() {
    CHECK(swarm_core_init(16u));

    float* posX = swarm_core_buffer_posX();
    float* posY = swarm_core_buffer_posY();
    float* velX = swarm_core_buffer_velX();
    float* velY = swarm_core_buffer_velY();

    // Particle 0 — the literal numbers from the ticket.
    posX[0] = 10.0f; posY[0] = 20.0f;
    velX[0] = 2.0f;  velY[0] = -4.0f;

    // Particle 1 — distinct values to catch a misindexed loop body.
    posX[1] = 100.0f; posY[1] = 200.0f;
    velX[1] = 8.0f;   velY[1] = 16.0f;

    swarm::core::arena::set_count(2u);
    swarm_core_update(0.5f);

    CHECK(posX[0] == 11.0f);
    CHECK(posY[0] == 18.0f);
    CHECK(posX[1] == 104.0f);
    CHECK(posY[1] == 208.0f);

    swarm_core_shutdown();
}

// ----------------------------------------------------------------------------
// Pointer stability through update. test_arena proves this for the stub; the
// real loop must preserve it.
// ----------------------------------------------------------------------------

void test_pointer_stability_through_update() {
    CHECK(swarm_core_init(64u));

    float*    p0 = swarm_core_buffer_posX();
    float*    p1 = swarm_core_buffer_posY();
    float*    p2 = swarm_core_buffer_velX();
    float*    p3 = swarm_core_buffer_velY();
    float*    p4 = swarm_core_buffer_colorR();
    float*    p5 = swarm_core_buffer_colorG();
    float*    p6 = swarm_core_buffer_colorB();
    float*    p7 = swarm_core_buffer_colorA();
    float*    p8 = swarm_core_buffer_life();
    uint32_t* p9 = swarm_core_buffer_flags();

    swarm::core::arena::set_count(64u);
    p2[0] = 1.0f; p3[0] = 1.0f; // exercise a real write

    swarm_core_update(0.016f);

    CHECK(p0 == swarm_core_buffer_posX());
    CHECK(p1 == swarm_core_buffer_posY());
    CHECK(p2 == swarm_core_buffer_velX());
    CHECK(p3 == swarm_core_buffer_velY());
    CHECK(p4 == swarm_core_buffer_colorR());
    CHECK(p5 == swarm_core_buffer_colorG());
    CHECK(p6 == swarm_core_buffer_colorB());
    CHECK(p7 == swarm_core_buffer_colorA());
    CHECK(p8 == swarm_core_buffer_life());
    CHECK(p9 == swarm_core_buffer_flags());

    swarm_core_shutdown();
}

// ----------------------------------------------------------------------------
// Count and capacity unchanged.
// ----------------------------------------------------------------------------

void test_count_and_capacity_unchanged() {
    const uint32_t N = 32u;
    CHECK(swarm_core_init(128u));
    swarm::core::arena::set_count(N);

    swarm_core_update(0.016f);

    CHECK(swarm_core_particle_count() == N);
    CHECK(swarm_core_max_particles() == 128u);

    swarm_core_shutdown();
}

// ----------------------------------------------------------------------------
// Only the active range [0, count) is touched. Sentinels in [count, capacity)
// must survive bit-exact.
// ----------------------------------------------------------------------------

void test_only_active_range_touched() {
    const uint32_t CAP = 256u;
    const uint32_t N   = 8u;
    CHECK(swarm_core_init(CAP));
    swarm::core::arena::set_count(N);

    float* posX = swarm_core_buffer_posX();
    float* posY = swarm_core_buffer_posY();
    float* velX = swarm_core_buffer_velX();
    float* velY = swarm_core_buffer_velY();

    // Seed velocities across the full capacity. The integrator must not read
    // (or, more importantly, not write) past index N regardless.
    for (uint32_t i = 0u; i < CAP; ++i) {
        velX[i] = 1.0f;
        velY[i] = 2.0f;
    }

    // Write distinct sentinels into the inactive tail of the position buffers.
    for (uint32_t i = N; i < CAP; ++i) {
        posX[i] = static_cast<float>(0xC0DE0000u | i);
        posY[i] = static_cast<float>(0xCAFE0000u | i);
    }

    swarm_core_update(0.25f);

    for (uint32_t i = N; i < CAP; ++i) {
        CHECK(posX[i] == static_cast<float>(0xC0DE0000u | i));
        CHECK(posY[i] == static_cast<float>(0xCAFE0000u | i));
    }

    // Sanity: active range did update.
    for (uint32_t i = 0u; i < N; ++i) {
        CHECK(posX[i] == 0.25f);
        CHECK(posY[i] == 0.5f);
    }

    swarm_core_shutdown();
}

// ----------------------------------------------------------------------------
// life and flags must not be touched by the integrator.
// ----------------------------------------------------------------------------

void test_life_and_flags_untouched() {
    const uint32_t N = 16u;
    CHECK(swarm_core_init(N));
    swarm::core::arena::set_count(N);

    float*    life  = swarm_core_buffer_life();
    uint32_t* flags = swarm_core_buffer_flags();
    float*    velX  = swarm_core_buffer_velX();
    float*    velY  = swarm_core_buffer_velY();

    for (uint32_t i = 0u; i < N; ++i) {
        life[i]  = 0.125f * static_cast<float>(i + 1u);
        flags[i] = 0xDEAD0000u | i;
        velX[i]  = 1.0f;
        velY[i]  = 1.0f;
    }

    swarm_core_update(0.016f);

    for (uint32_t i = 0u; i < N; ++i) {
        CHECK(life[i]  == 0.125f * static_cast<float>(i + 1u));
        CHECK(flags[i] == (0xDEAD0000u | i));
    }

    swarm_core_shutdown();
}

// ----------------------------------------------------------------------------
// Color buffers must not be touched by the integrator.
// ----------------------------------------------------------------------------

void test_color_buffers_untouched() {
    const uint32_t N = 16u;
    CHECK(swarm_core_init(N));
    swarm::core::arena::set_count(N);

    float* colorR = swarm_core_buffer_colorR();
    float* colorG = swarm_core_buffer_colorG();
    float* colorB = swarm_core_buffer_colorB();
    float* colorA = swarm_core_buffer_colorA();
    float* velX   = swarm_core_buffer_velX();
    float* velY   = swarm_core_buffer_velY();

    for (uint32_t i = 0u; i < N; ++i) {
        colorR[i] = 0.1f * static_cast<float>(i);
        colorG[i] = 0.2f * static_cast<float>(i);
        colorB[i] = 0.3f * static_cast<float>(i);
        colorA[i] = 0.4f * static_cast<float>(i);
        velX[i]   = 1.0f;
        velY[i]   = 1.0f;
    }

    swarm_core_update(0.016f);

    for (uint32_t i = 0u; i < N; ++i) {
        CHECK(colorR[i] == 0.1f * static_cast<float>(i));
        CHECK(colorG[i] == 0.2f * static_cast<float>(i));
        CHECK(colorB[i] == 0.3f * static_cast<float>(i));
        CHECK(colorA[i] == 0.4f * static_cast<float>(i));
    }

    swarm_core_shutdown();
}

// ----------------------------------------------------------------------------
// count == 0 is a clean no-op; no buffer byte may change.
// ----------------------------------------------------------------------------

void test_zero_count_no_op() {
    const uint32_t CAP = 64u;
    CHECK(swarm_core_init(CAP));
    // No set_count call — count stays at the post-init value of 0.
    CHECK(swarm_core_particle_count() == 0u);

    float* posX = swarm_core_buffer_posX();
    float* posY = swarm_core_buffer_posY();
    float* velX = swarm_core_buffer_velX();
    float* velY = swarm_core_buffer_velY();

    for (uint32_t i = 0u; i < CAP; ++i) {
        posX[i] = 7.0f  + static_cast<float>(i);
        posY[i] = 11.0f + static_cast<float>(i);
        velX[i] = 1000.0f;
        velY[i] = -1000.0f;
    }

    swarm_core_update(0.5f);

    for (uint32_t i = 0u; i < CAP; ++i) {
        CHECK(posX[i] == 7.0f  + static_cast<float>(i));
        CHECK(posY[i] == 11.0f + static_cast<float>(i));
    }

    swarm_core_shutdown();
}

// ----------------------------------------------------------------------------
// deltaTime == 0 is a no-op for finite velocities. Documents the contract.
// ----------------------------------------------------------------------------

void test_zero_dt_no_op() {
    const uint32_t N = 8u;
    CHECK(swarm_core_init(N));
    swarm::core::arena::set_count(N);

    float* posX = swarm_core_buffer_posX();
    float* posY = swarm_core_buffer_posY();
    float* velX = swarm_core_buffer_velX();
    float* velY = swarm_core_buffer_velY();

    for (uint32_t i = 0u; i < N; ++i) {
        posX[i] = 3.5f + static_cast<float>(i);
        posY[i] = 6.5f + static_cast<float>(i);
        velX[i] = 1.25f;
        velY[i] = -2.5f;
    }

    swarm_core_update(0.0f);

    for (uint32_t i = 0u; i < N; ++i) {
        CHECK(posX[i] == 3.5f + static_cast<float>(i));
        CHECK(posY[i] == 6.5f + static_cast<float>(i));
    }

    swarm_core_shutdown();
}

} // namespace

int main() {
    // Order matters: the "before init" check must precede any init.
    test_called_before_init_safe();
    test_called_after_shutdown_safe();
    test_canonical_euler_step();
    test_pointer_stability_through_update();
    test_count_and_capacity_unchanged();
    test_only_active_range_touched();
    test_life_and_flags_untouched();
    test_color_buffers_untouched();
    test_zero_count_no_op();
    test_zero_dt_no_op();

    if (g_failures != 0) {
        std::fprintf(stderr, "test_update: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_update: OK\n");
    return 0;
}
