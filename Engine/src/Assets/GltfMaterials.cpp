#include "GltfMaterials.hpp"

namespace lockstep::assets
{
    namespace
    {
        constexpr float k_transmissionOpacity{0.9f};

        const cgltf_image* sourceImage(const cgltf_texture_view& view)
        {
            if (view.texture == nullptr)
            {
                return nullptr;
            }

            if (view.texture->has_basisu && view.texture->basisu_image != nullptr)
            {
                return view.texture->basisu_image;
            }

            return view.texture->image;
        }
    } //namespace

    TextureTable TextureTable::collect(const cgltf_data& document, ImportWarnings& warnings)
    {
        TextureTable table{};

        for (cgltf_size index = 0; index < document.materials_count; ++index)
        {
            const cgltf_material& material = document.materials[index];

            if (material.has_pbr_specular_glossiness && !material.has_pbr_metallic_roughness)
            {
                warnings.add("KHR_materials_pbrSpecularGlossiness materials use default metallic-roughness");
            }

            table.add(material.pbr_metallic_roughness.base_color_texture, true, warnings);
            table.add(material.emissive_texture, true, warnings);
            table.add(material.normal_texture, false, warnings);
            table.add(material.pbr_metallic_roughness.metallic_roughness_texture, false, warnings);
            table.add(material.occlusion_texture, false, warnings);
        }

        return table;
    }

    const std::vector<TextureKey>& TextureTable::keys() const noexcept
    {
        return m_keys;
    }

    void TextureTable::setHandle(std::size_t slot, render::TextureHandle handle)
    {
        m_handles[slot] = handle;
    }

    const std::vector<render::TextureHandle>& TextureTable::handles() const
    {
        return m_handles;
    }

    render::TextureHandle TextureTable::find(const cgltf_texture_view& view, bool srgb) const
    {
        const auto found = m_slots.find({sourceImage(view), srgb});

        return found != m_slots.end() ? m_handles[found->second] : render::TextureHandle{};
    }

    void TextureTable::add(const cgltf_texture_view& view, bool srgb, ImportWarnings& warnings)
    {
        const cgltf_image* image = sourceImage(view);

        if (image == nullptr)
        {
            return;
        }

        if (view.has_transform)
        {
            warnings.add("KHR_texture_transform is ignored");
        }

        if (view.texcoord != 0)
        {
            warnings.add("textures sampled with a UV set other than TEXCOORD_0 use TEXCOORD_0");
        }

        if (m_slots.emplace(std::pair{image, srgb}, m_keys.size()).second)
        {
            m_keys.push_back(TextureKey{.image = image, .srgb = srgb});
            m_handles.emplace_back();
        }
    }

    render::PbrMaterialDescription toPbrDescription(const cgltf_material* material, const TextureTable& textures)
    {
        render::PbrMaterialDescription description{};

        if (material == nullptr)
        {
            return description;
        }

        const cgltf_pbr_metallic_roughness& pbr = material->pbr_metallic_roughness;

        if (material->has_pbr_metallic_roughness)
        {
            description.baseColor = textures.find(pbr.base_color_texture, true);
            description.baseColorFactor = math::Vec4{pbr.base_color_factor[0], pbr.base_color_factor[1],
                pbr.base_color_factor[2], pbr.base_color_factor[3]};
            description.metallicRoughness = textures.find(pbr.metallic_roughness_texture, false);
            description.metallicFactor = pbr.metallic_factor;
            description.roughnessFactor = pbr.roughness_factor;
        }

        description.normal = textures.find(material->normal_texture, false);
        description.normalScale = material->normal_texture.texture != nullptr ? material->normal_texture.scale : 1.0f;
        description.occlusion = textures.find(material->occlusion_texture, false);
        description.occlusionStrength = material->occlusion_texture.texture != nullptr
            ? material->occlusion_texture.scale
            : 1.0f;
        description.emissive = textures.find(material->emissive_texture, true);

        const float emissiveStrength = material->has_emissive_strength
            ? material->emissive_strength.emissive_strength
            : 1.0f;
        description.emissiveFactor = math::Vec3{material->emissive_factor[0], material->emissive_factor[1],
            material->emissive_factor[2]} * emissiveStrength;

        description.alphaMode = toAlphaMode(material->alpha_mode);
        description.alphaCutoff = material->alpha_cutoff;
        description.doubleSided = material->double_sided != 0;

        if (material->has_transmission && material->transmission.transmission_factor > 0.0f)
        {
            description.alphaMode = render::AlphaMode::Blend;
            description.baseColorFactor.w *= 1.0f - material->transmission.transmission_factor * k_transmissionOpacity;
        }

        return description;
    }

    render::AlphaMode toAlphaMode(cgltf_alpha_mode mode)
    {
        switch (mode)
        {
            case cgltf_alpha_mode_mask:
            {
                return render::AlphaMode::Mask;
            }
            case cgltf_alpha_mode_blend:
            {
                return render::AlphaMode::Blend;
            }
            default:
            {
                return render::AlphaMode::Opaque;
            }
        }
    }
} //namespace lockstep::assets
