#include "swarm_core/swarm_core_math.h"

#include <cmath>

namespace swarm::core {

f32 length(Vec2 v) {
    return std::sqrt(length_squared(v));
}

f32 distance(Vec2 a, Vec2 b) {
    return std::sqrt(distance_squared(a, b));
}

Vec2 normalize(Vec2 v) {
    const f32 len_sq = length_squared(v);
    if (len_sq == 0.0f) {
        return Vec2{ 0.0f, 0.0f };
    }
    const f32 inv_len = 1.0f / std::sqrt(len_sq);
    return Vec2{ v.x * inv_len, v.y * inv_len };
}

} // namespace swarm::core
