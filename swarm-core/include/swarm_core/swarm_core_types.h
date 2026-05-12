#ifndef SWARM_CORE_TYPES_H
#define SWARM_CORE_TYPES_H

#include <cstdint>

namespace swarm::core {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

static_assert(sizeof(f32) == 4, "f32 must be IEEE-754 single precision (4 bytes)");
static_assert(sizeof(f64) == 8, "f64 must be IEEE-754 double precision (8 bytes)");

} // namespace swarm::core

#endif // SWARM_CORE_TYPES_H
