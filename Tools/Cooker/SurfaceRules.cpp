#include "SurfaceRules.hpp"

#include "SceneSource.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace lockstep::cooker
{
    namespace
    {
        constexpr float k_detailRepeatMetres{0.5f};
        constexpr float k_minDetailScale{0.25f};
        constexpr float k_maxDetailScale{64.0f};
        constexpr float k_defaultParallaxDepth{0.01f};
        constexpr float k_maxHeightScale{0.2f};
        constexpr std::array<std::string_view, 2> k_imageExtensions{".png", ".jpg"};
        constexpr std::array<std::string_view, 4> k_resolutions{"2k", "1k", "4k", "8k"};

        struct DepthRule
        {
            std::string_view word;
            float metres{0.0f};
        };

        constexpr std::array<DepthRule, 13> k_depthRules{{{"brick", 0.025f}, {"pavement", 0.015f},
            {"paving", 0.015f}, {"asphalt", 0.012f}, {"road", 0.012f}, {"shutter", 0.012f}, {"concrete", 0.01f},
            {"terrazzo", 0.006f}, {"tile", 0.008f}, {"ceiling", 0.008f}, {"plaster", 0.004f}, {"paint", 0.004f},
            {"linoleum", 0.003f}}};

        constexpr std::array<std::string_view, 6> k_noDetailWords{"decal", "glass", "window", "screen", "emissive",
            "light"};

        struct DetailRule
        {
            render::SurfaceDetail layer{render::SurfaceDetail::None};
            std::array<std::string_view, 20> words{};
        };

        constexpr std::array<DetailRule, 5> k_detailRules{{
            {render::SurfaceDetail::Metal, {"metal", "steel", "iron", "shutter", "pipe", "rail", "chrome", "alumin",
                "rust", "desk", "cart", "cabinet", "locker", "vent", "hydrant", "extinguisher", "dumpster", "fence",
                "barrel"}},
            {render::SurfaceDetail::Paint, {"linoleum", "plastic", "laminate", "vinyl", "sign", "clock"}},
            {render::SurfaceDetail::Mineral, {"concrete", "cement", "brick", "plaster", "asphalt", "road", "pavement",
                "stone", "terrazzo", "tile", "ceiling", "wall", "floor", "facade", "block", "foundation"}},
            {render::SurfaceDetail::Fibrous, {"fabric", "cloth", "sofa", "curtain", "bed", "mattress", "carpet", "wood",
                "crate", "table", "cardboard", "rug", "towel", "couch", "chair"}},
            {render::SurfaceDetail::Paint, {"paint", "door", "sign", "frame", "skirting"}}}};

        std::string lowered(std::string_view text)
        {
            std::string result(text);
            std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return result;
        }

        std::string imageName(const cgltf_image* image)
        {
            if (image == nullptr)
            {
                return {};
            }

            if (image->name != nullptr && image->name[0] != '\0')
            {
                return image->name;
            }

            return image->uri != nullptr ? std::filesystem::path(image->uri).stem().string() : std::string{};
        }

        struct MaterialMeasure
        {
            double worldArea{0.0};
            double uvArea{0.0};
        };

        std::vector<MaterialMeasure> measureMaterials(const SourceScene& scene)
        {
            std::vector<MaterialMeasure> measures(scene.materials.size());
            std::vector<MaterialMeasure> meshAreas(scene.meshes.size());

            for (std::size_t mesh = 0; mesh < scene.meshes.size(); ++mesh)
            {
                const assets::PrimitiveGeometry& geometry = scene.meshes[mesh].geometry;

                for (std::size_t first = 0; first + 2 < geometry.indices.size(); first += 3)
                {
                    const render::PbrVertex& a = geometry.vertices[geometry.indices[first]];
                    const render::PbrVertex& b = geometry.vertices[geometry.indices[first + 1]];
                    const render::PbrVertex& c = geometry.vertices[geometry.indices[first + 2]];
                    const math::Vec3 edge1{b.position[0] - a.position[0], b.position[1] - a.position[1],
                        b.position[2] - a.position[2]};
                    const math::Vec3 edge2{c.position[0] - a.position[0], c.position[1] - a.position[1],
                        c.position[2] - a.position[2]};
                    const float uvCross = (b.uv[0] - a.uv[0]) * (c.uv[1] - a.uv[1]) - (c.uv[0] - a.uv[0]) * (b.uv[1] - a.uv[1]);
                    meshAreas[mesh].worldArea += 0.5 * static_cast<double>(math::length(math::cross(edge1, edge2)));
                    meshAreas[mesh].uvArea += 0.5 * std::abs(static_cast<double>(uvCross));
                }
            }

            for (const assets::SceneInstance& instance : scene.instances)
            {
                const math::Vec3& scale = instance.transform.scale;
                const double volume = std::abs(static_cast<double>(scale.x) * scale.y * scale.z);
                const double areaScale = std::cbrt(volume * volume);
                MaterialMeasure& measure = measures[scene.meshes[instance.mesh].material];
                measure.worldArea += meshAreas[instance.mesh].worldArea * areaScale;
                measure.uvArea += meshAreas[instance.mesh].uvArea;
            }

            return measures;
        }

        std::optional<std::filesystem::path> existingImage(const std::filesystem::path& folder, const std::string& stem)
        {
            for (const std::string_view extension : k_imageExtensions)
            {
                std::filesystem::path candidate = folder / (stem + std::string(extension));
                std::error_code error;

                if (std::filesystem::is_regular_file(candidate, error))
                {
                    return candidate;
                }
            }

            return std::nullopt;
        }

        std::optional<std::filesystem::path> polyHavenHeight(const SurfaceRuleOptions& options, const std::string& name)
        {
            const std::size_t marker = name.rfind("_nor");

            if (marker == std::string::npos || marker == 0)
            {
                return std::nullopt;
            }

            const std::string id = name.substr(0, marker);
            const std::size_t lastUnderscore = name.rfind('_');
            const std::string resolution = lastUnderscore > marker ? name.substr(lastUnderscore + 1) : std::string{};
            std::vector<std::string> resolutions;

            if (!resolution.empty())
            {
                resolutions.push_back(resolution);
            }

            for (const std::string_view common : k_resolutions)
            {
                resolutions.emplace_back(common);
            }

            for (const std::filesystem::path& folder : options.heightDirectories)
            {
                for (const std::string& size : resolutions)
                {
                    const std::string stem = id + "_disp_" + size;

                    if (auto found = existingImage(folder / id, stem))
                    {
                        return found;
                    }

                    if (auto found = existingImage(folder, stem))
                    {
                        return found;
                    }
                }
            }

            return std::nullopt;
        }

        std::optional<std::filesystem::path> namedHeight(const SurfaceRuleOptions& options, const std::string& name)
        {
            constexpr std::string_view k_normalSuffix{"_normal"};

            if (!name.ends_with(k_normalSuffix))
            {
                return std::nullopt;
            }

            const std::string stem = name.substr(0, name.size() - k_normalSuffix.size()) + "_height";

            for (const std::filesystem::path& folder : options.heightDirectories)
            {
                if (auto found = existingImage(folder, stem))
                {
                    return found;
                }
            }

            return std::nullopt;
        }

        bool containsAny(const std::string& text, std::span<const std::string_view> words)
        {
            return std::any_of(words.begin(), words.end(),
                [&text](std::string_view word) { return !word.empty() && text.find(word) != std::string::npos; });
        }

        render::SurfaceDetail detailLayer(const std::string& names)
        {
            if (containsAny(names, k_noDetailWords))
            {
                return render::SurfaceDetail::None;
            }

            for (const DetailRule& rule : k_detailRules)
            {
                if (containsAny(names, rule.words))
                {
                    return rule.layer;
                }
            }

            return render::SurfaceDetail::None;
        }

        float parallaxDepth(const std::string& names)
        {
            for (const DepthRule& rule : k_depthRules)
            {
                if (names.find(rule.word) != std::string::npos)
                {
                    return rule.metres;
                }
            }

            return k_defaultParallaxDepth;
        }

        uint32_t addHeightTexture(SourceScene& scene, const std::filesystem::path& file)
        {
            for (std::size_t index = 0; index < scene.textures.size(); ++index)
            {
                if (scene.textures[index].usage == TextureUsage::Height && scene.textures[index].file == file)
                {
                    return static_cast<uint32_t>(index);
                }
            }

            scene.textures.push_back(SourceTexture{.image = nullptr, .file = file, .usage = TextureUsage::Height,
                .alphaCutoff = -1.0f});
            return static_cast<uint32_t>(scene.textures.size() - 1);
        }
    } //namespace

    SurfaceRuleReport applySurfaceRules(SourceScene& scene, const SurfaceRuleOptions& options)
    {
        SurfaceRuleReport report{};
        const std::vector<MaterialMeasure> measures = measureMaterials(scene);

        for (std::size_t index = 0; index < scene.materials.size(); ++index)
        {
            assets::CookedMaterial& material = scene.materials[index];
            const MaterialMeasure& measure = measures[index];

            if (material.factors.alphaMode == render::AlphaMode::Blend || measure.uvArea <= 0.0 || measure.worldArea <= 0.0)
            {
                continue;
            }

            const float metresPerUv = static_cast<float>(std::sqrt(measure.worldArea / measure.uvArea));
            const std::string materialName = index < scene.materialNames.size() ? scene.materialNames[index] : "";
            const std::string baseName = material.baseColor != assets::k_noTexture
                ? imageName(scene.textures[material.baseColor].image) : std::string{};
            const std::string names = lowered(materialName + " " + baseName);

            if (options.detail && metresPerUv / k_detailRepeatMetres <= k_maxDetailScale)
            {
                material.factors.detail = detailLayer(names);
                material.factors.detailScale = std::max(metresPerUv / k_detailRepeatMetres, k_minDetailScale);
                report.detailLayers += material.factors.detail != render::SurfaceDetail::None ? 1u : 0u;
            }

            if (options.heightDirectories.empty() || material.normal == assets::k_noTexture
                || containsAny(names, k_noDetailWords))
            {
                continue;
            }

            const std::string normalName = imageName(scene.textures[material.normal].image);
            std::optional<std::filesystem::path> height = polyHavenHeight(options, normalName);

            if (!height)
            {
                height = namedHeight(options, normalName);
            }

            if (!height)
            {
                continue;
            }

            material.height = addHeightTexture(scene, *height);
            material.factors.heightScale = std::min(parallaxDepth(names) / metresPerUv, k_maxHeightScale);
            ++report.heightMaps;
        }

        return report;
    }
} //namespace lockstep::cooker
