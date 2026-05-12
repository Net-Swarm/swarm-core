#include "arena.h"

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
constexpr std::size_t kBufferCount  = 10u;
constexpr std::size_t kScalarStride = 4u; // float and uint32_t share width

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
    void*             raw{nullptr};         // std::malloc result, retained for std::free
    unsigned char*    aligned_base{nullptr}; // 16-aligned head inside raw
    std::size_t       block_bytes{0};       // bytes per buffer including alignment slack
    ParticleStateSoA  state{};
    bool              initialized{false};
};

Arena g_arena{};

// Maximum maxParticles that keeps (kBufferCount * aligned_block + kAlignment - 1)
// inside size_t. Capacity is bounded by both u32 and size_t arithmetic limits.
constexpr std::size_t kMaxParticlesByByteBudget =
    (SIZE_MAX - (kAlignment - 1u)) / (kBufferCount * kScalarStride);

} // namespace

bool init(u32 maxParticles) {
    if (g_arena.initialized) {
        return false;
    }
    if (maxParticles == 0u) {
        return false;
    }
    if (static_cast<std::size_t>(maxParticles) > kMaxParticlesByByteBudget) {
        return false;
    }

    const std::size_t per_buffer = align_up(
        static_cast<std::size_t>(maxParticles) * kScalarStride,
        kAlignment);
    const std::size_t total_bytes = per_buffer * kBufferCount + (kAlignment - 1u);

    void* raw = std::malloc(total_bytes);
    if (raw == nullptr) {
        return false;
    }

    auto raw_addr     = reinterpret_cast<std::uintptr_t>(raw);
    auto aligned_addr = (raw_addr + (kAlignment - 1u)) & ~(static_cast<std::uintptr_t>(kAlignment) - 1u);
    auto* aligned     = reinterpret_cast<unsigned char*>(aligned_addr);
    assert((reinterpret_cast<std::uintptr_t>(aligned) & (kAlignment - 1u)) == 0u);

    // Deterministic init: zero every buffer. §6 mandates zeroed flags; the
    // remaining buffers are zeroed so the rest of the core never reads
    // uninitialized memory.
    std::memset(aligned, 0, per_buffer * kBufferCount);

    g_arena.raw          = raw;
    g_arena.aligned_base = aligned;
    g_arena.block_bytes  = per_buffer;

    // Bind SoA pointer fields in canonical ordinal order (§6).
    unsigned char* p = aligned;
    g_arena.state.posX   = reinterpret_cast<f32*>(p); p += per_buffer;
    g_arena.state.posY   = reinterpret_cast<f32*>(p); p += per_buffer;
    g_arena.state.velX   = reinterpret_cast<f32*>(p); p += per_buffer;
    g_arena.state.velY   = reinterpret_cast<f32*>(p); p += per_buffer;
    g_arena.state.colorR = reinterpret_cast<f32*>(p); p += per_buffer;
    g_arena.state.colorG = reinterpret_cast<f32*>(p); p += per_buffer;
    g_arena.state.colorB = reinterpret_cast<f32*>(p); p += per_buffer;
    g_arena.state.colorA = reinterpret_cast<f32*>(p); p += per_buffer;
    g_arena.state.life   = reinterpret_cast<f32*>(p); p += per_buffer;
    g_arena.state.flags  = reinterpret_cast<u32*>(p);

    g_arena.state.count    = 0u;
    g_arena.state.capacity = maxParticles;
    g_arena.initialized    = true;
    return true;
}

void shutdown() {
    if (!g_arena.initialized) {
        return;
    }
    std::free(g_arena.raw);
    g_arena.raw          = nullptr;
    g_arena.aligned_base = nullptr;
    g_arena.block_bytes  = 0u;
    zero(g_arena.state); // nulls all pointer fields and zeros count/capacity
    g_arena.initialized  = false;
}

void reset() {
    if (!g_arena.initialized) {
        return;
    }
    g_arena.state.count = 0u;
    std::memset(g_arena.state.flags, 0,
                static_cast<std::size_t>(g_arena.state.capacity) * sizeof(u32));
}

bool is_initialized() {
    return g_arena.initialized;
}

ParticleStateSoA& state() {
    return g_arena.state;
}

void set_count(u32 newCount) {
    assert(g_arena.initialized && "arena::set_count before arena::init");
    assert(newCount <= g_arena.state.capacity &&
           "particleCount must not exceed maxParticles (CORE_EXTRACTION_ARCHITECTURE.md §5.4)");
    g_arena.state.count = newCount;
}

} // namespace swarm::core::arena
