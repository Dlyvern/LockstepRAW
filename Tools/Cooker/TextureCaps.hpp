#ifndef LOCKSTEP_COOKER_TEXTURE_CAPS_HPP
#define LOCKSTEP_COOKER_TEXTURE_CAPS_HPP

#include "SceneSource.hpp"

#include <cstdint>
#include <vector>

namespace lockstep::cooker
{
    // How large cooked textures may be.
    // maxSize:        global cap on the top mip's larger side, in texels.
    // texelsPerMetre: texel density the automatic cap aims for on the object using the texture.
    // automatic:      derive a per-texture cap from the size of the objects that use it.
    struct TextureCapOptions
    {
        uint32_t maxSize{2048};
        float texelsPerMetre{1024.0f};
        bool automatic{true};
    };

    // Smallest automatic cap; textures are never capped below this by object size.
    inline constexpr uint32_t k_minAutomaticTextureCap{256};

    // UV extent above which a mesh counts as tiling (walls, floors) and is not capped by its size.
    inline constexpr float k_tilingUvExtent{1.5f};

    // Picks the top-mip size limit of every texture of a scene. Without `automatic` every texture
    // gets maxSize. Otherwise each mesh instance asks for texelsPerMetre times its world bounding
    // box diagonal, rounded up to a power of two and clamped to [256, maxSize]; an instance whose
    // mesh's UVs span more than 1.5 asks for maxSize. A texture gets the largest request of the
    // instances whose material uses it, or maxSize if no instance uses it. Deterministic.
    // scene:   the scene read by readSourceScene().
    // options: the caps to apply.
    // Returns: one limit per scene texture, indexed like SourceScene::textures.
    [[nodiscard]] std::vector<uint32_t> textureCaps(const SourceScene& scene, const TextureCapOptions& options);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_TEXTURE_CAPS_HPP
