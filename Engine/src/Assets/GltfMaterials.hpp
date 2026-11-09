#ifndef LOCKSTEP_ASSETS_GLTF_MATERIALS_HPP
#define LOCKSTEP_ASSETS_GLTF_MATERIALS_HPP

#include "Lockstep/Render/PbrMaterial.hpp"
#include "Lockstep/Render/Resources.hpp"

#include "ImportWarnings.hpp"

#include <cgltf.h>

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace lockstep::assets
{
    // One GPU texture to create: a glTF image read in one colour space. The same image used as
    // colour and as data becomes two textures.
    // image: the image (the KHR_texture_basisu source when there is one).
    // srgb:  base colour and emissive are sRGB; normal, metallic-roughness and occlusion linear.
    struct TextureKey
    {
        const cgltf_image* image{nullptr};
        bool srgb{false};
    };

    // The textures a document's materials use, deduplicated, and their handles once uploaded.
    class TextureTable
    {
    public:
        // Collects every texture the document's materials sample.
        // document: the glTF document.
        // warnings: receives texture features that are ignored (texture transforms, UV sets).
        // Returns:  the table with every handle still invalid.
        [[nodiscard]] static TextureTable collect(const cgltf_data& document, ImportWarnings& warnings);

        // Returns: the textures to create, in a fixed order.
        [[nodiscard]] const std::vector<TextureKey>& keys() const noexcept;

        // Records the handle created for keys()[slot].
        // slot:   index into keys().
        // handle: the texture, or an invalid handle if it could not be created.
        void setHandle(std::size_t slot, render::TextureHandle handle);

        // view:    a material's texture slot.
        // srgb:    the colour space the slot needs.
        // Returns: the uploaded texture for it, or an invalid handle if the slot is empty or the
        //          texture failed.
        [[nodiscard]] render::TextureHandle find(const cgltf_texture_view& view, bool srgb) const;

    private:
        void add(const cgltf_texture_view& view, bool srgb, ImportWarnings& warnings);

        std::vector<TextureKey> m_keys;
        std::vector<render::TextureHandle> m_handles;
        std::map<std::pair<const cgltf_image*, bool>, std::size_t> m_slots;
    };

    // Converts a glTF material to the Renderer's PBR description.
    // material: the material, or nullptr for glTF's default material.
    // textures: the uploaded textures.
    // Returns:  the description; factors are copied as authored, emissive strength is folded in.
    [[nodiscard]] render::PbrMaterialDescription toPbrDescription(const cgltf_material* material,
        const TextureTable& textures);

    // mode:    a glTF alpha mode.
    // Returns: the matching render::AlphaMode.
    [[nodiscard]] render::AlphaMode toAlphaMode(cgltf_alpha_mode mode);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_GLTF_MATERIALS_HPP
