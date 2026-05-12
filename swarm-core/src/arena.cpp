#include "arena.h"

#include "spatial_grid.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
// <cstdlib> is added beyond the §10 baseline for std::malloc/std::free.
// Justification: CORE-5 owns the single arena allocation at init and the
// matching release at shutdown. No other path in the core may allocate.
#include <cstdlib>

namespace swarm::core::arena {
namespace {

constexpr std::size_t kAlignment    = 16u;
constexpr std::size_t kSoABuffers   = 10u; // CORE-5 particle SoA buffers (ordinals 0..9)
constexpr std::size_t kScalarStride = 4u;  // float and uint32_t share width

// CORE-7 grid buffer counts. Particle-indexed grid buffers reuse the
// per-particle slab size; cell-indexed grid buffers use their own per-cell
// slab size derived from cellCount.
constexpr std::size_t kGridParticleU32Buffers = 2u; // particleCell, particleNext
constexpr std::size_t kGridCellU32Buffers     = 2u; // cellStartOrHead, cellCountOrNext

constexpr std::size_t kTotalParticleBuffers = kSoABuffers + kGridParticleU32Buffers; // 12

constexpr std::size_t align_up(std::size_t n, std::size_t a) {
    return (n + (a - 1u)) & ~(a - 1u);
}

// Catch arithmetic regression in the alignment helper.
static_assert(align_up(0u, kAlignment) == 0u);
static_assert(align_up(1u, kAlignment) == 16u);
static_assert(align_up(16u, kAlignment) == 16u);
static_assert(align_up(17u, kAlignment) == 32u);
static_assert(align_up(1024u * 4u, kAlignment) == 4096u);

static_assert(sizeof(f32) == kScalarStride, "ABI v1 requires float32 scalar buffers");
static_assert(sizeof(u32) == kScalarStride, "ABI v1 requires uint32_t for the flags buffer");

struct Arena {
    void*             raw{nullptr};          // std::malloc result, retained for std::free
    unsigned char*    aligned_base{nullptr}; // 16-aligned head inside raw
    std::size_t       particle_slab_bytes{0}; // bytes per particle-indexed buffer (post align_up)
    std::size_t       cell_slab_bytes{0};     // bytes per cell-indexed buffer    (post align_up)
    ParticleStateSoA  state{};
    SpatialGrid       grid{};
    bool              initialized{false};
};

Arena g_arena{};

// Maximum cell contribution to the arena, bounded at compile time by
// SWARM_CORE_MAX_GRID_CELLS. Used to derive the safe upper bound for
// maxParticles given the byte budget below.
constexpr std::size_t kMaxCellSlabBytes =
    align_up(static_cast<std::size_t>(SWARM_CORE_MAX_GRID_CELLS) * kScalarStride, kAlignment);
constexpr std::size_t kMaxCellTotalBytes = kGridCellU32Buffers * kMaxCellSlabBytes;

// Conservative upper bound on maxParticles: leaves headroom for the worst-case
// cell contribution and the kAlignment-1 alignment slack so the total stays
// inside size_t. Capacity is bounded by both u32 and size_t arithmetic limits.
constexpr std::size_t kMaxParticlesByByteBudget =
    (SIZE_MAX - kMaxCellTotalBytes - (kAlignment - 1u))
    / (kTotalParticleBuffers * kScalarStride);

} // namespace

bool init(const Config& config) {
    if (g_arena.initialized) {
        return false;
    }
    if (config.maxParticles == 0u) {
        return false;
    }

    // CORE-7: validate grid config before any allocation so a rejected init
    // leaves no partial state. cellCount is derived as gridCols * gridRows per
    // Docs/CORE_EXTRACTION_ARCHITECTURE.md §12.
    u32 cellCount = 0u;
    if (!validate(config.spatialGrid, &cellCount)) {
        return false;
    }

    if (static_cast<std::size_t>(config.maxParticles) > kMaxParticlesByByteBudget) {
        return false;
    }

    const std::size_t per_particle = align_up(
        static_cast<std::size_t>(config.maxParticles) * kScalarStride,
        kAlignment);
    const std::size_t per_cell = align_up(
        static_cast<std::size_t>(cellCount) * kScalarStride,
        kAlignment);
    const std::size_t total_bytes =
        per_particle * kTotalParticleBuffers
      + per_cell     * kGridCellU32Buffers
      + (kAlignment - 1u);

    void* raw = std::malloc(total_bytes);
    if (raw == nullptr) {
        return false;
    }

    auto raw_addr     = reinterpret_cast<std::uintptr_t>(raw);
    auto aligned_addr = (raw_addr + (kAlignment - 1u)) & ~(static_cast<std::uintptr_t>(kAlignment) - 1u);
    auto* aligned     = reinterpret_cast<unsigned char*>(aligned_addr);
    assert((reinterpret_cast<std::uintptr_t>(aligned) & (kAlignment - 1u)) == 0u);

    // Deterministic init: zero every buffer. §6 mandates zeroed flags; CORE-7
    // requires zeroed grid buffers; the remaining particle buffers are zeroed
    // so the rest of the core never reads uninitialized memory.
    const std::size_t buffers_bytes =
        per_particle * kTotalParticleBuffers + per_cell * kGridCellU32Buffers;
    std::memset(aligned, 0, buffers_bytes);

    g_arena.raw                 = raw;
    g_arena.aligned_base        = aligned;
    g_arena.particle_slab_bytes = per_particle;
    g_arena.cell_slab_bytes     = per_cell;

    // Bind SoA pointer fields in canonical ordinal order (§6).
    unsigned char* p = aligned;
    g_arena.state.posX   = reinterpret_cast<f32*>(p); p += per_particle;
    g_arena.state.posY   = reinterpret_cast<f32*>(p); p += per_particle;
    g_arena.state.velX   = reinterpret_cast<f32*>(p); p += per_particle;
    g_arena.state.velY   = reinterpret_cast<f32*>(p); p += per_particle;
    g_arena.state.colorR = reinterpret_cast<f32*>(p); p += per_particle;
    g_arena.state.colorG = reinterpret_cast<f32*>(p); p += per_particle;
    g_arena.state.colorB = reinterpret_cast<f32*>(p); p += per_particle;
    g_arena.state.colorA = reinterpret_cast<f32*>(p); p += per_particle;
    g_arena.state.life   = reinterpret_cast<f32*>(p); p += per_particle;
    g_arena.state.flags  = reinterpret_cast<u32*>(p); p += per_particle;

    // CORE-7 grid bindings. Cell-indexed buffers first (per_cell stride), then
    // particle-indexed buffers (per_particle stride). Each prior slab was
    // align_up-padded, so every binding remains 16-byte aligned.
    g_arena.grid.cellStartOrHead = reinterpret_cast<u32*>(p); p += per_cell;
    g_arena.grid.cellCountOrNext = reinterpret_cast<u32*>(p); p += per_cell;
    g_arena.grid.particleCell    = reinterpret_cast<u32*>(p); p += per_particle;
    g_arena.grid.particleNext    = reinterpret_cast<u32*>(p);

    g_arena.state.count    = 0u;
    g_arena.state.capacity = config.maxParticles;

    g_arena.grid.cellCount    = cellCount;
    g_arena.grid.gridCols     = config.spatialGrid.gridCols;
    g_arena.grid.gridRows     = config.spatialGrid.gridRows;
    g_arena.grid.maxParticles = config.maxParticles;

    g_arena.initialized = true;
    return true;
}

void shutdown() {
    if (!g_arena.initialized) {
        return;
    }
    std::free(g_arena.raw);
    g_arena.raw                 = nullptr;
    g_arena.aligned_base        = nullptr;
    g_arena.particle_slab_bytes = 0u;
    g_arena.cell_slab_bytes     = 0u;
    zero(g_arena.state); // nulls all pointer fields and zeros count/capacity
    zero(g_arena.grid);  // CORE-7: nulls grid pointers and zeros dimensions
    g_arena.initialized = false;
}

void reset() {
    if (!g_arena.initialized) {
        return;
    }
    g_arena.state.count = 0u;
    std::memset(g_arena.state.flags, 0,
                static_cast<std::size_t>(g_arena.state.capacity) * sizeof(u32));

    // CORE-7 §12: spatial-grid buffers cleared to zero deterministically.
    // Dimensions and pointer identity are preserved.
    const SpatialGrid& g = g_arena.grid;
    std::memset(g.cellStartOrHead, 0,
                static_cast<std::size_t>(g.cellCount)    * sizeof(u32));
    std::memset(g.cellCountOrNext, 0,
                static_cast<std::size_t>(g.cellCount)    * sizeof(u32));
    std::memset(g.particleCell,    0,
                static_cast<std::size_t>(g.maxParticles) * sizeof(u32));
    std::memset(g.particleNext,    0,
                static_cast<std::size_t>(g.maxParticles) * sizeof(u32));
}

bool is_initialized() {
    return g_arena.initialized;
}

ParticleStateSoA& state() {
    return g_arena.state;
}

SpatialGrid& grid() {
    return g_arena.grid;
}

void set_count(u32 newCount) {
    assert(g_arena.initialized && "arena::set_count before arena::init");
    assert(newCount <= g_arena.state.capacity &&
           "particleCount must not exceed maxParticles (CORE_EXTRACTION_ARCHITECTURE.md §5.4)");
    g_arena.state.count = newCount;
}

} // namespace swarm::core::arena
