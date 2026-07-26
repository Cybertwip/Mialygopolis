#pragma once

#include "linalg.h"

#include <algorithm>
#include <cmath>

namespace m3d {

using Vec2 = linalg::vec<float, 2>;
using Vec3 = linalg::vec<float, 3>;
using Vec4 = linalg::vec<float, 4>;
using Mat4 = linalg::mat<float, 4, 4>;

constexpr float Pi = 3.14159265358979323846f;

inline Mat4 rotationY(float radians)
{
    return linalg::rotation_matrix(linalg::rotation_quat(Vec3{0.0f, 1.0f, 0.0f}, radians));
}

inline Mat4 modelMatrix(const Vec3& position, float yaw, float uniformScale, const Vec3& localOffset)
{
    return linalg::mul(
        linalg::translation_matrix(position),
        rotationY(yaw),
        linalg::scaling_matrix(Vec3{uniformScale, uniformScale, uniformScale}),
        linalg::translation_matrix(localOffset));
}

inline float clamp(float value, float lower, float upper)
{
    return std::max(lower, std::min(value, upper));
}

inline float smoothstep(float edge0, float edge1, float value)
{
    const float t = clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace m3d
