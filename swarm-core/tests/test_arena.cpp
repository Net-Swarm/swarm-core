// Self-contained CORE-5 contract test driver. Exit 0 means every check passed.
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

bool is_16_byte_aligned(const void* p) {
    return (reinterpret_cast<std::uintptr_t>(p) & 0xFu) == 0u;
}

void test_lifecycle() {
    CHECK(swarm_core_init(1024u));
    CHECK(swarm_core_max_particles() == 1024u);
    CHECK(swarm_core_particle_count() == 0u);
    CHECK(swarm_core_abi_version() == SWARM_CORE_ABI_VERSION);
    swarm_core_shutdown();
}

void test_pointer_stability() {
    CHECK(swarm_core_init(4096u));

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

    swarm_core_reset();
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

void test_alignment() {
    CHECK(swarm_core_init(777u)); // non-power-of-two capacity stresses padding math
    CHECK(is_16_byte_aligned(swarm_core_buffer_posX()));
    CHECK(is_16_byte_aligned(swarm_core_buffer_posY()));
    CHECK(is_16_byte_aligned(swarm_core_buffer_velX()));
    CHECK(is_16_byte_aligned(swarm_core_buffer_velY()));
    CHECK(is_16_byte_aligned(swarm_core_buffer_colorR()));
    CHECK(is_16_byte_aligned(swarm_core_buffer_colorG()));
    CHECK(is_16_byte_aligned(swarm_core_buffer_colorB()));
    CHECK(is_16_byte_aligned(swarm_core_buffer_colorA()));
    CHECK(is_16_byte_aligned(swarm_core_buffer_life()));
    CHECK(is_16_byte_aligned(swarm_core_buffer_flags()));
    swarm_core_shutdown();
}

void test_buffer_non_overlap_and_writability() {
    const uint32_t N = 256u;
    CHECK(swarm_core_init(N));

    // Write a buffer-unique sentinel pattern at every element of every buffer.
    // If two buffers aliased, the later writes would corrupt the earlier ones.
    float* fbufs[9] = {
        swarm_core_buffer_posX(),
        swarm_core_buffer_posY(),
        swarm_core_buffer_velX(),
        swarm_core_buffer_velY(),
        swarm_core_buffer_colorR(),
        swarm_core_buffer_colorG(),
        swarm_core_buffer_colorB(),
        swarm_core_buffer_colorA(),
        swarm_core_buffer_life(),
    };
    for (int b = 0; b < 9; ++b) {
        for (uint32_t i = 0; i < N; ++i) {
            fbufs[b][i] = static_cast<float>(b * 10000 + static_cast<int>(i));
        }
    }
    uint32_t* flags = swarm_core_buffer_flags();
    for (uint32_t i = 0; i < N; ++i) {
        flags[i] = 0xABCD0000u | i;
    }

    // Read back and verify no buffer was clobbered by another.
    for (int b = 0; b < 9; ++b) {
        for (uint32_t i = 0; i < N; ++i) {
            CHECK(fbufs[b][i] == static_cast<float>(b * 10000 + static_cast<int>(i)));
        }
    }
    for (uint32_t i = 0; i < N; ++i) {
        CHECK(flags[i] == (0xABCD0000u | i));
    }

    swarm_core_shutdown();
}

void test_reset_zeroes_flags_and_count() {
    const uint32_t N = 128u;
    CHECK(swarm_core_init(N));

    // Move count off zero to verify reset clears it. Public ABI has no setter
    // in v1, so reach through the internal helper.
    swarm::core::arena::set_count(N);
    CHECK(swarm_core_particle_count() == N);

    uint32_t* flags = swarm_core_buffer_flags();
    for (uint32_t i = 0; i < N; ++i) {
        flags[i] = 0xFFFFFFFFu;
    }

    // Touch a non-flags buffer so we can confirm reset does NOT clear it (§8).
    float* life = swarm_core_buffer_life();
    life[0] = 42.5f;

    swarm_core_reset();
    CHECK(swarm_core_particle_count() == 0u);
    for (uint32_t i = 0; i < N; ++i) {
        CHECK(flags[i] == 0u);
    }
    CHECK(life[0] == 42.5f); // §8: reset does not touch non-flags buffers

    swarm_core_shutdown();
}

void test_init_failure_paths() {
    CHECK(swarm_core_init(0u) == false);

    CHECK(swarm_core_init(64u));
    CHECK(swarm_core_init(64u) == false); // double-init rejected
    swarm_core_shutdown();

    // Overflow guard: a value far above the byte-budget cap must fail cleanly.
    CHECK(swarm_core_init(UINT32_MAX) == false);
}

void test_post_shutdown_state() {
    CHECK(swarm_core_init(32u));
    swarm_core_shutdown();

    CHECK(swarm_core_buffer_posX()   == nullptr);
    CHECK(swarm_core_buffer_posY()   == nullptr);
    CHECK(swarm_core_buffer_velX()   == nullptr);
    CHECK(swarm_core_buffer_velY()   == nullptr);
    CHECK(swarm_core_buffer_colorR() == nullptr);
    CHECK(swarm_core_buffer_colorG() == nullptr);
    CHECK(swarm_core_buffer_colorB() == nullptr);
    CHECK(swarm_core_buffer_colorA() == nullptr);
    CHECK(swarm_core_buffer_life()   == nullptr);
    CHECK(swarm_core_buffer_flags()  == nullptr);
    CHECK(swarm_core_particle_count() == 0u);
    CHECK(swarm_core_max_particles() == 0u);
}

void test_set_count_invariant() {
    // The internal set_count helper enforces count <= capacity. Public ABI v1
    // exposes no setter, so this exercises the invariant for future tickets
    // (spawn API, integrator). Over-capacity calls trip assert() in debug
    // builds — not exercised here to keep the suite return-code-based.
    CHECK(swarm_core_init(16u));
    swarm::core::arena::set_count(0u);
    CHECK(swarm_core_particle_count() == 0u);
    swarm::core::arena::set_count(16u);
    CHECK(swarm_core_particle_count() == 16u);
    swarm_core_shutdown();
}

} // namespace

int main() {
    test_lifecycle();
    test_pointer_stability();
    test_alignment();
    test_buffer_non_overlap_and_writability();
    test_reset_zeroes_flags_and_count();
    test_init_failure_paths();
    test_post_shutdown_state();
    test_set_count_invariant();

    if (g_failures != 0) {
        std::fprintf(stderr, "test_arena: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_arena: OK\n");
    return 0;
}
