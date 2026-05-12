// Self-contained CORE-7 contract test driver. Exit 0 means every check passed.
// Once CORE-8 lands the CMake target, this driver will become a CTest case.
// Until then, build with the command in tests/README.md.

#include "swarm_core/swarm_core.h"
#include "swarm_core/swarm_core_particles.h"
#include "swarm_core/version.h"

// arena.h and spatial_grid.h are private (src/). Tests live inside swarm-core
// and may include them to reach internal helpers (set_count, init(Config&),
// grid()). Public consumers must not.
#include "../src/arena.h"
#include "../src/spatial_grid.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using swarm::core::Config;
using swarm::core::SpatialGrid;
using swarm::core::SpatialGridConfig;
using swarm::core::u32;
namespace arena = swarm::core::arena;

int g_failures = 0;

void report_failure(const char* expr, const char* file, int line) {
    std::fprintf(stderr, "FAIL  %s:%d  %s\n", file, line, expr);
    ++g_failures;
}

#define CHECK(expr) do { if (!(expr)) report_failure(#expr, __FILE__, __LINE__); } while (0)

bool is_16_byte_aligned(const void* p) {
    return (reinterpret_cast<std::uintptr_t>(p) & 0xFu) == 0u;
}

// ----------------------------------------------------------------------------
// Default-path tests — exercise the public swarm_core_init convenience path,
// which supplies the documented 64x64 grid defaults per §12.
// ----------------------------------------------------------------------------

void test_default_init_uses_64x64() {
    CHECK(swarm_core_init(1024u));
    const SpatialGrid& g = arena::grid();
    CHECK(g.gridCols     == swarm::core::SWARM_CORE_DEFAULT_GRID_COLS);
    CHECK(g.gridRows     == swarm::core::SWARM_CORE_DEFAULT_GRID_ROWS);
    CHECK(g.gridCols     == 64u);
    CHECK(g.gridRows     == 64u);
    CHECK(g.cellCount    == 64u * 64u);
    CHECK(g.cellCount    == 4096u);
    CHECK(g.maxParticles == 1024u);
    swarm_core_shutdown();
}

void test_grid_buffers_non_null_and_aligned() {
    CHECK(swarm_core_init(777u)); // non-power-of-two capacity stresses padding math
    const SpatialGrid& g = arena::grid();
    CHECK(g.cellStartOrHead != nullptr);
    CHECK(g.cellCountOrNext != nullptr);
    CHECK(g.particleCell    != nullptr);
    CHECK(g.particleNext    != nullptr);
    CHECK(is_16_byte_aligned(g.cellStartOrHead));
    CHECK(is_16_byte_aligned(g.cellCountOrNext));
    CHECK(is_16_byte_aligned(g.particleCell));
    CHECK(is_16_byte_aligned(g.particleNext));
    swarm_core_shutdown();
}

void test_grid_pointer_stability() {
    CHECK(swarm_core_init(64u));
    SpatialGrid& g = arena::grid();

    u32* p0 = g.cellStartOrHead;
    u32* p1 = g.cellCountOrNext;
    u32* p2 = g.particleCell;
    u32* p3 = g.particleNext;

    arena::set_count(64u);
    swarm_core_update(0.016f);
    CHECK(p0 == arena::grid().cellStartOrHead);
    CHECK(p1 == arena::grid().cellCountOrNext);
    CHECK(p2 == arena::grid().particleCell);
    CHECK(p3 == arena::grid().particleNext);

    swarm_core_reset();
    CHECK(p0 == arena::grid().cellStartOrHead);
    CHECK(p1 == arena::grid().cellCountOrNext);
    CHECK(p2 == arena::grid().particleCell);
    CHECK(p3 == arena::grid().particleNext);

    swarm_core_shutdown();
}

void test_reset_zeroes_grid_buffers() {
    CHECK(swarm_core_init(128u));
    SpatialGrid& g = arena::grid();
    const u32 cellCount = g.cellCount;
    const u32 maxP      = g.maxParticles;

    // Sentinel-write distinct patterns into every grid buffer. If reset misses
    // a buffer, the post-reset check below catches it.
    for (u32 i = 0u; i < cellCount; ++i) {
        g.cellStartOrHead[i] = 0xB1000000u | i;
        g.cellCountOrNext[i] = 0xB2000000u | i;
    }
    for (u32 i = 0u; i < maxP; ++i) {
        g.particleCell[i] = 0xB3000000u | i;
        g.particleNext[i] = 0xB4000000u | i;
    }

    // Move count off zero too, so we exercise the existing particle-reset path
    // alongside the new grid-reset behavior.
    arena::set_count(maxP);
    CHECK(swarm_core_particle_count() == maxP);

    swarm_core_reset();

    CHECK(swarm_core_particle_count() == 0u);
    for (u32 i = 0u; i < cellCount; ++i) {
        CHECK(g.cellStartOrHead[i] == 0u);
        CHECK(g.cellCountOrNext[i] == 0u);
    }
    for (u32 i = 0u; i < maxP; ++i) {
        CHECK(g.particleCell[i] == 0u);
        CHECK(g.particleNext[i] == 0u);
    }

    // Dimensions and pointer identity preserved (§12).
    CHECK(g.cellCount    == cellCount);
    CHECK(g.maxParticles == maxP);
    CHECK(g.gridCols     == 64u);
    CHECK(g.gridRows     == 64u);

    swarm_core_shutdown();
}

void test_update_does_not_touch_grid() {
    const u32 N = 64u;
    CHECK(swarm_core_init(N));
    arena::set_count(N);

    SpatialGrid& g = arena::grid();
    const u32 cellCount = g.cellCount;

    // Sentinel pattern the integrator cannot produce.
    for (u32 i = 0u; i < cellCount; ++i) {
        g.cellStartOrHead[i] = 0xC1000000u | i;
        g.cellCountOrNext[i] = 0xC2000000u | i;
    }
    for (u32 i = 0u; i < N; ++i) {
        g.particleCell[i] = 0xC3000000u | i;
        g.particleNext[i] = 0xC4000000u | i;
    }

    // Seed velocities so integrator does real work.
    float* velX = swarm_core_buffer_velX();
    float* velY = swarm_core_buffer_velY();
    for (u32 i = 0u; i < N; ++i) {
        velX[i] = 1.0f;
        velY[i] = 1.0f;
    }

    swarm_core_update(0.016f);

    for (u32 i = 0u; i < cellCount; ++i) {
        CHECK(g.cellStartOrHead[i] == (0xC1000000u | i));
        CHECK(g.cellCountOrNext[i] == (0xC2000000u | i));
    }
    for (u32 i = 0u; i < N; ++i) {
        CHECK(g.particleCell[i] == (0xC3000000u | i));
        CHECK(g.particleNext[i] == (0xC4000000u | i));
    }

    swarm_core_shutdown();
}

void test_post_shutdown_grid_nulled() {
    CHECK(swarm_core_init(32u));
    swarm_core_shutdown();

    const SpatialGrid& g = arena::grid();
    CHECK(g.cellStartOrHead == nullptr);
    CHECK(g.cellCountOrNext == nullptr);
    CHECK(g.particleCell    == nullptr);
    CHECK(g.particleNext    == nullptr);
    CHECK(g.cellCount    == 0u);
    CHECK(g.gridCols     == 0u);
    CHECK(g.gridRows     == 0u);
    CHECK(g.maxParticles == 0u);
}

// ----------------------------------------------------------------------------
// Validation tests — exercise paths the public convenience init cannot reach,
// by constructing Config directly and going through the internal arena::init.
// Every failed init must leave the arena uninitialized so the next test can
// proceed.
// ----------------------------------------------------------------------------

void test_init_rejects_zero_grid_dims() {
    CHECK(arena::init(Config{1024u, SpatialGridConfig{0u, 64u}}) == false);
    CHECK(arena::is_initialized() == false);
    CHECK(arena::init(Config{1024u, SpatialGridConfig{64u, 0u}}) == false);
    CHECK(arena::is_initialized() == false);
    CHECK(arena::init(Config{1024u, SpatialGridConfig{0u, 0u}}) == false);
    CHECK(arena::is_initialized() == false);
}

void test_init_rejects_oversize_grid_dims() {
    using swarm::core::SWARM_CORE_MAX_GRID_COLS;
    using swarm::core::SWARM_CORE_MAX_GRID_ROWS;

    CHECK(arena::init(Config{1024u,
        SpatialGridConfig{SWARM_CORE_MAX_GRID_COLS + 1u, 64u}}) == false);
    CHECK(arena::is_initialized() == false);

    CHECK(arena::init(Config{1024u,
        SpatialGridConfig{64u, SWARM_CORE_MAX_GRID_ROWS + 1u}}) == false);
    CHECK(arena::is_initialized() == false);
}

void test_init_rejects_grid_dim_overflow() {
    // The product 65536 * 65536 = 2^32 overflows uint32_t. These dimensions
    // also exceed the per-axis maximum, so the validator may reject before
    // reaching the overflow check; either way init must return false.
    CHECK(arena::init(Config{1024u, SpatialGridConfig{65536u, 65536u}}) == false);
    CHECK(arena::is_initialized() == false);
}

void test_init_rejects_cell_count_exceeding_limit() {
    // 2048 * 1024 = 2,097,152 cells > SWARM_CORE_MAX_GRID_CELLS (1,048,576).
    // Both dimensions are inside the per-axis cap, so this exercises the
    // cellCount-cap check specifically.
    CHECK(arena::init(Config{1024u, SpatialGridConfig{2048u, 1024u}}) == false);
    CHECK(arena::is_initialized() == false);
}

void test_init_accepts_custom_grid_dims() {
    CHECK(arena::init(Config{256u, SpatialGridConfig{32u, 32u}}));
    CHECK(arena::grid().gridCols     == 32u);
    CHECK(arena::grid().gridRows     == 32u);
    CHECK(arena::grid().cellCount    == 1024u);
    CHECK(arena::grid().maxParticles == 256u);
    CHECK(swarm_core_max_particles() == 256u);
    swarm_core_shutdown();
}

void test_cellcount_equals_cols_times_rows() {
    struct GridCase { u32 cols; u32 rows; };
    const GridCase cases[] = {
        {1u, 1u},
        {1u, 64u},
        {64u, 1u},
        {32u, 32u},
        {17u, 23u},
        {swarm::core::SWARM_CORE_MAX_GRID_COLS, 1u},
        {1u, swarm::core::SWARM_CORE_MAX_GRID_ROWS},
    };
    for (const auto& tc : cases) {
        CHECK(arena::init(Config{256u, SpatialGridConfig{tc.cols, tc.rows}}));
        CHECK(arena::grid().gridCols  == tc.cols);
        CHECK(arena::grid().gridRows  == tc.rows);
        CHECK(arena::grid().cellCount == tc.cols * tc.rows);
        swarm_core_shutdown();
    }
}

} // namespace

int main() {
    test_default_init_uses_64x64();
    test_grid_buffers_non_null_and_aligned();
    test_grid_pointer_stability();
    test_reset_zeroes_grid_buffers();
    test_update_does_not_touch_grid();
    test_post_shutdown_grid_nulled();

    test_init_rejects_zero_grid_dims();
    test_init_rejects_oversize_grid_dims();
    test_init_rejects_grid_dim_overflow();
    test_init_rejects_cell_count_exceeding_limit();
    test_init_accepts_custom_grid_dims();
    test_cellcount_equals_cols_times_rows();

    if (g_failures != 0) {
        std::fprintf(stderr, "test_spatial_grid: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_spatial_grid: OK\n");
    return 0;
}
