#ifndef LOCKSTEP_RENDER_SKINNING_HPP
#define LOCKSTEP_RENDER_SKINNING_HPP

#include "Lockstep/Core/Result.hpp"

#include <Vela/Graphics/VertexLayout.hpp>

#include <cstdint>

namespace lockstep::render
{
    struct RendererState;

    // Set-1 binding of the joint palette storage buffer in every skinned shader (skinning.glsl).
    inline constexpr uint32_t k_jointPaletteBinding{6};

    // Returns: the full SkinnedPbrVertex layout the skinned PBR vertex shader reads (locations 0-3
    //          as PbrVertex, 4 joints, 5 weights).
    [[nodiscard]] vela::graphics::VertexLayout skinnedPbrLayout();

    // Returns: the SkinnedPbrVertex layout of the skinned shadow shader: position, joints, weights.
    [[nodiscard]] vela::graphics::VertexLayout skinnedShadowLayout();

    // Returns: the SkinnedPbrVertex layout of the skinned masked-shadow shader: position, uv,
    //          joints, weights.
    [[nodiscard]] vela::graphics::VertexLayout skinnedMaskedShadowLayout();

    // Uploads the joint palettes queued this frame (RendererState::jointData) into the joint
    // buffer the frame's skinned draws read. Does nothing when no palette was queued.
    // state:   the renderer state.
    // Returns: success, or an error if the buffer could not grow.
    [[nodiscard]] Status uploadJointPalettes(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SKINNING_HPP
