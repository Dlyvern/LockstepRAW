#ifndef LOCKSTEP_RENDER_VELA_CONVERSIONS_HPP
#define LOCKSTEP_RENDER_VELA_CONVERSIONS_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/Camera.hpp"
#include "Lockstep/Render/PresentMode.hpp"
#include "Lockstep/Render/Resources.hpp"

#include "Core/VelaError.hpp"

#include <Vela/Core/ContextPreferences.hpp>
#include <Vela/Graphics/RenderTypes.hpp>
#include <Vela/Graphics/Sampler.hpp>

#include <span>

namespace lockstep::render
{
    // Vela error translation lives in Core; re-exported so Render and Debug code can call
    // render::toError().
    using lockstep::toError;

    // Builds a right-handed view matrix looking from the camera position towards its target.
    // camera:  the camera.
    // Returns: the view matrix.
    [[nodiscard]] math::Mat4 toViewMatrix(const Camera& camera);

    // Builds a perspective projection for Vulkan clip space, including the Y flip.
    // camera:      the camera lens settings.
    // aspectRatio: viewport width divided by height.
    // Returns:     the projection matrix.
    [[nodiscard]] math::Mat4 toProjectionMatrix(const Camera& camera, float aspectRatio);

    // format:  an Engine texture format.
    // Returns: the matching Vela texture format.
    [[nodiscard]] vela::graphics::TextureFormat toVelaFormat(TextureFormat format);

    // filter:  an Engine texture filter.
    // Returns: a Vela sampler description using that filter for both minification and magnification;
    //          Linear is also 16x anisotropic (clamped by Vela to what the device supports).
    [[nodiscard]] vela::graphics::SamplerDescription toVelaSampler(TextureFilter filter);

    // mode:      an Engine present mode.
    // supported: the VSync modes the surface offers (Context::getSupportedVSync()); may be empty
    //            before the context exists.
    // Returns:   the Vela mode to ask for: VSync -> On, LowLatency -> LatestReady if supported
    //            else Fast, Uncapped -> Off (Vela falls back Off -> Fast -> On on its own).
    [[nodiscard]] vela::core::VSync toVelaVSync(PresentMode mode, std::span<const vela::core::VSync> supported);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_VELA_CONVERSIONS_HPP
