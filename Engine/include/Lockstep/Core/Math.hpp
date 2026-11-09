#ifndef LOCKSTEP_CORE_MATH_HPP
#define LOCKSTEP_CORE_MATH_HPP

#include <Vela/Math/Math.hpp>

// Engine math. Vectors, quaternions, matrices and their operations come from Vela's math
// library (header-only, no GPU code) and are re-exported here under Engine names, so Engine and
// Game share one set of types and never convert between them. Game includes this header, never
// Vela's directly. Engine-only additions (Transform and friends) live in their own headers in
// Lockstep/Core.
namespace lockstep::math
{
    using Vec2 = vela::math::Vector2f;
    using Vec3 = vela::math::Vector3f;
    using Vec4 = vela::math::Vector4f;
    using Vec2i = vela::math::Vector2i;
    using Vec3i = vela::math::Vector3i;
    using Quat = vela::math::Quaternion;
    using Mat4 = vela::math::Mat4;

    inline constexpr float k_pi{vela::math::pi};

    using vela::math::abs;
    using vela::math::clamp;
    using vela::math::cross;
    using vela::math::degrees;
    using vela::math::dot;
    using vela::math::length;
    using vela::math::lengthSquared;
    using vela::math::lerp;
    using vela::math::max;
    using vela::math::min;
    using vela::math::normalize;
    using vela::math::radians;

    using vela::math::angleAxis;
    using vela::math::conjugate;
    using vela::math::fromEuler;
    using vela::math::fromRotationMatrix;
    using vela::math::slerp;
    using vela::math::toEuler;
    using vela::math::toMat4;

    using vela::math::compose;
    using vela::math::decompose;
    using vela::math::inverse;
    using vela::math::lookAt;
    using vela::math::ortho;
    using vela::math::perspective;
    using vela::math::rotate;
    using vela::math::scale;
    using vela::math::transformDirection;
    using vela::math::transformPoint;
    using vela::math::translate;
    using vela::math::transpose;
} //namespace lockstep::math

#endif //LOCKSTEP_CORE_MATH_HPP
