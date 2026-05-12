#ifndef SWARM_CORE_MATH_H
#define SWARM_CORE_MATH_H

#include "swarm_core/swarm_core_types.h"

namespace swarm::core {

struct Vec2 {
    f32 x;
    f32 y;
};

inline Vec2 add(Vec2 a, Vec2 b)        { return Vec2{ a.x + b.x, a.y + b.y }; }
inline Vec2 sub(Vec2 a, Vec2 b)        { return Vec2{ a.x - b.x, a.y - b.y }; }
inline Vec2 scale(Vec2 v, f32 s)       { return Vec2{ v.x * s, v.y * s }; }
inline f32  dot(Vec2 a, Vec2 b)        { return a.x * b.x + a.y * b.y; }
inline f32  length_squared(Vec2 v)     { return v.x * v.x + v.y * v.y; }
inline f32  distance_squared(Vec2 a, Vec2 b) {
    const f32 dx = a.x - b.x;
    const f32 dy = a.y - b.y;
    return dx * dx + dy * dy;
}

f32  length(Vec2 v);
f32  distance(Vec2 a, Vec2 b);
Vec2 normalize(Vec2 v);

} // namespace swarm::core

#endif // SWARM_CORE_MATH_H
