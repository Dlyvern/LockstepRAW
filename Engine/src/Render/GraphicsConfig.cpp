#include "Lockstep/Render/GraphicsConfig.hpp"

#include "Lockstep/Core/Log.hpp"
#include "Lockstep/Core/SettingsFile.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_graphicsFlag{"--graphics"};
        constexpr std::string_view k_settingsFlag{"--settings"};
        constexpr std::string_view k_fileName{"settings.ini"};
        constexpr std::string_view k_folderName{"LockstepRAW"};
        constexpr std::string_view k_presetKey{"preset"};
        constexpr std::string_view k_antiAliasingKey{"anti_aliasing"};
        constexpr std::string_view k_windowWidthKey{"window_width"};
        constexpr std::string_view k_windowHeightKey{"window_height"};
        constexpr std::string_view k_graphicsSection{"graphics"};
        constexpr uint32_t k_minWindowSide{320};
        constexpr uint32_t k_maxWindowSide{7680};
        constexpr uint32_t k_lowWindowWidth{960};
        constexpr uint32_t k_lowWindowHeight{540};
        constexpr uint32_t k_windowWidth{1280};
        constexpr uint32_t k_windowHeight{720};

        using QualityField = std::variant<float GraphicsQuality::*, uint32_t GraphicsQuality::*,
            bool GraphicsQuality::*>;

        struct FieldKey
        {
            std::string_view key;
            QualityField field;
        };

        const std::array<FieldKey, 25> k_fields{{
            {"render_scale", &GraphicsQuality::renderScale},
            {"texture_mip_drop", &GraphicsQuality::textureMipDrop},
            {"shadow_atlas_size", &GraphicsQuality::shadowAtlasSize},
            {"shadow_cascades", &GraphicsQuality::shadowCascades},
            {"shadowed_local_lights", &GraphicsQuality::shadowedLocalLights},
            {"volumetric_fog", &GraphicsQuality::volumetricFog},
            {"volumetric_fog_resolution", &GraphicsQuality::volumetricFogResolution},
            {"volumetric_fog_lights", &GraphicsQuality::volumetricFogLights},
            {"character_shadow_cascades", &GraphicsQuality::characterShadowCascades},
            {"parallax_local_shadows", &GraphicsQuality::parallaxLocalShadows},
            {"screen_space_reflections", &GraphicsQuality::screenSpaceReflections},
            {"ambient_occlusion", &GraphicsQuality::ambientOcclusion},
            {"contact_shadows", &GraphicsQuality::contactShadows},
            {"effect_steps", &GraphicsQuality::effectSteps},
            {"parallax", &GraphicsQuality::parallax},
            {"surface_detail", &GraphicsQuality::surfaceDetail},
            {"motion_blur", &GraphicsQuality::motionBlur},
            {"depth_of_field", &GraphicsQuality::depthOfField},
            {"lens_effects", &GraphicsQuality::lensEffects},
            {"bloom", &GraphicsQuality::bloom},
            {"planar_reflections", &GraphicsQuality::planarReflections},
            {"reflection_resolution", &GraphicsQuality::reflectionResolution},
            {"scope_resolution", &GraphicsQuality::scopeResolution},
            {"particle_budget", &GraphicsQuality::particleBudget},
            {"fps_limit", &GraphicsQuality::frameRateLimit}}};

        constexpr std::array<std::pair<std::string_view, AntiAliasingMode>, 5> k_antiAliasingNames{{
            {"off", AntiAliasingMode::Off}, {"fxaa", AntiAliasingMode::Fxaa}, {"msaa2x", AntiAliasingMode::Msaa2x},
            {"msaa4x", AntiAliasingMode::Msaa4x}, {"taa", AntiAliasingMode::Taa}}};

        struct Entry
        {
            std::string key;
            std::string value;
            uint32_t line{0};
        };

        std::string_view trimmed(std::string_view text)
        {
            const std::size_t first = text.find_first_not_of(" \t\r");

            if (first == std::string_view::npos)
            {
                return {};
            }

            const std::size_t last = text.find_last_not_of(" \t\r");

            return text.substr(first, last - first + 1);
        }

        std::optional<Entry> parseLine(std::string_view line, uint32_t number)
        {
            const std::string_view content = trimmed(line.substr(0, line.find('#')));

            if (content.empty() || content.front() == '[')
            {
                return std::nullopt;
            }

            const std::size_t equals = content.find('=');

            if (equals == std::string_view::npos)
            {
                return Entry{.key = std::string(content), .value = {}, .line = number};
            }

            return Entry{.key = std::string(trimmed(content.substr(0, equals))),
                .value = std::string(trimmed(content.substr(equals + 1))), .line = number};
        }

        std::optional<std::string_view> sectionHeader(std::string_view line)
        {
            const std::string_view content = trimmed(line.substr(0, line.find('#')));

            if (content.size() < 2 || content.front() != '[' || content.back() != ']')
            {
                return std::nullopt;
            }

            return trimmed(content.substr(1, content.size() - 2));
        }

        std::vector<Entry> readEntries(const std::filesystem::path& file)
        {
            std::vector<Entry> entries;
            std::ifstream stream(file);
            std::string line;
            uint32_t number = 0;
            bool isGraphicsSection = true;

            while (std::getline(stream, line))
            {
                ++number;

                if (const std::optional<std::string_view> section = sectionHeader(line))
                {
                    isGraphicsSection = *section == k_graphicsSection;
                    continue;
                }

                if (!isGraphicsSection)
                {
                    continue;
                }

                if (auto entry = parseLine(line, number))
                {
                    entries.push_back(std::move(*entry));
                }
            }

            return entries;
        }

        void setGraphicsValue(SettingsFile& settings, std::string_view key, std::string_view value)
        {
            settings.set(settings.contains("", key) ? std::string_view() : k_graphicsSection, key, value);
        }

        std::optional<uint32_t> parseUnsigned(std::string_view text)
        {
            uint32_t value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

            if (error != std::errc{} || end != text.data() + text.size())
            {
                return std::nullopt;
            }

            return value;
        }

        std::optional<float> parseFloat(std::string_view text)
        {
            float value = 0.0f;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

            if (error != std::errc{} || end != text.data() + text.size())
            {
                return std::nullopt;
            }

            return value;
        }

        std::optional<bool> parseBool(std::string_view text)
        {
            if (text == "1" || text == "true" || text == "on" || text == "yes")
            {
                return true;
            }

            if (text == "0" || text == "false" || text == "off" || text == "no")
            {
                return false;
            }

            return std::nullopt;
        }

        bool assignField(GraphicsQuality& quality, const QualityField& field, std::string_view text)
        {
            if (const auto* member = std::get_if<float GraphicsQuality::*>(&field))
            {
                const std::optional<float> value = parseFloat(text);

                if (value)
                {
                    quality.*(*member) = *value;
                }

                return value.has_value();
            }

            if (const auto* member = std::get_if<uint32_t GraphicsQuality::*>(&field))
            {
                const std::optional<uint32_t> value = parseUnsigned(text);

                if (value)
                {
                    quality.*(*member) = *value;
                }

                return value.has_value();
            }

            const std::optional<bool> value = parseBool(text);

            if (value)
            {
                quality.*(std::get<bool GraphicsQuality::*>(field)) = *value;
            }

            return value.has_value();
        }

        std::string formatField(const GraphicsQuality& quality, const QualityField& field)
        {
            if (const auto* member = std::get_if<float GraphicsQuality::*>(&field))
            {
                std::ostringstream stream;
                stream << quality.*(*member);
                return stream.str();
            }

            if (const auto* member = std::get_if<uint32_t GraphicsQuality::*>(&field))
            {
                return std::to_string(quality.*(*member));
            }

            return quality.*(std::get<bool GraphicsQuality::*>(field)) ? "true" : "false";
        }

        const char* antiAliasingName(AntiAliasingMode mode)
        {
            for (const auto& [name, candidate] : k_antiAliasingNames)
            {
                if (candidate == mode)
                {
                    return name.data();
                }
            }

            return "taa";
        }

        void warn(const std::filesystem::path& file, const Entry& entry, std::string_view problem)
        {
            log(LogLevel::Warning, "Render", file.string() + ":" + std::to_string(entry.line) + ": "
                + std::string(problem) + " '" + entry.key + " = " + entry.value + "', skipped");
        }

        bool applyOverride(GraphicsConfig& config, const Entry& entry)
        {
            if (entry.key == k_windowWidthKey || entry.key == k_windowHeightKey)
            {
                const std::optional<uint32_t> side = parseUnsigned(entry.value);

                if (!side || *side < k_minWindowSide || *side > k_maxWindowSide)
                {
                    return false;
                }

                (entry.key == k_windowWidthKey ? config.windowWidth : config.windowHeight) = *side;
                return true;
            }

            if (entry.key == k_antiAliasingKey)
            {
                for (const auto& [name, mode] : k_antiAliasingNames)
                {
                    if (entry.value == name)
                    {
                        config.quality.antiAliasing = mode;
                        return true;
                    }
                }

                return false;
            }

            for (const FieldKey& field : k_fields)
            {
                if (entry.key == field.key)
                {
                    return assignField(config.quality, field.field, entry.value);
                }
            }

            return false;
        }

        void startFromPreset(GraphicsConfig& config, GraphicsPreset preset)
        {
            config.quality = graphicsQualityPreset(preset);
            config.quality.preset = preset;
            const bool isLow = preset == GraphicsPreset::Low;
            config.windowWidth = isLow ? k_lowWindowWidth : k_windowWidth;
            config.windowHeight = isLow ? k_lowWindowHeight : k_windowHeight;
        }

        bool sameQuality(const GraphicsQuality& left, const GraphicsQuality& right)
        {
            if (left.antiAliasing != right.antiAliasing)
            {
                return false;
            }

            for (const FieldKey& field : k_fields)
            {
                if (formatField(left, field.field) != formatField(right, field.field))
                {
                    return false;
                }
            }

            return true;
        }

        void applyFile(GraphicsConfig& config, const std::vector<Entry>& entries)
        {
            for (const Entry& entry : entries)
            {
                if (entry.key == k_presetKey)
                {
                    const std::optional<GraphicsPreset> preset = parseGraphicsPreset(entry.value);

                    if (!preset)
                    {
                        warn(config.file, entry, "unknown preset");
                        continue;
                    }

                    startFromPreset(config, *preset);
                }
            }

            const GraphicsQuality preset = config.quality;

            for (const Entry& entry : entries)
            {
                if (entry.key != k_presetKey && !applyOverride(config, entry))
                {
                    warn(config.file, entry, "unknown key or bad value");
                }
            }

            config.quality = clampedQuality(config.quality);

            if (!sameQuality(config.quality, preset))
            {
                config.quality.preset = GraphicsPreset::Custom;
            }
        }

        std::optional<std::string_view> flagValue(std::span<const char* const> arguments, std::string_view flag,
            bool& missing)
        {
            std::optional<std::string_view> value;

            for (std::size_t index = 0; index < arguments.size(); ++index)
            {
                if (arguments[index] == nullptr || std::string_view(arguments[index]) != flag)
                {
                    continue;
                }

                if (index + 1 >= arguments.size() || arguments[index + 1] == nullptr)
                {
                    missing = true;
                    return std::nullopt;
                }

                value = arguments[++index];
            }

            return value;
        }

        std::filesystem::path userConfigFolder()
        {
#ifdef _WIN32
            if (const char* appData = std::getenv("APPDATA"); appData != nullptr && *appData != '\0')
            {
                return std::filesystem::path(appData) / k_folderName;
            }
#else
            if (const char* config = std::getenv("XDG_CONFIG_HOME"); config != nullptr && *config != '\0')
            {
                return std::filesystem::path(config) / k_folderName;
            }

            if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
            {
                return std::filesystem::path(home) / ".config" / k_folderName;
            }
#endif
            return {};
        }
    } //namespace

    std::filesystem::path graphicsConfigPath(std::span<const char* const> arguments)
    {
        bool missing = false;

        if (const std::optional<std::string_view> file = flagValue(arguments, k_settingsFlag, missing))
        {
            return std::filesystem::path(*file);
        }

        return userConfigFolder() / k_fileName;
    }

    Result<GraphicsConfig> loadGraphicsConfig(std::span<const char* const> arguments)
    {
        GraphicsConfig config{};
        config.file = graphicsConfigPath(arguments);
        startFromPreset(config, GraphicsPreset::High);
        config.origin = "default";

        bool missing = false;
        const std::optional<std::string_view> flag = flagValue(arguments, k_graphicsFlag, missing);

        if (missing)
        {
            return Error{ErrorCode::InvalidArgument, "--graphics needs a value (low, medium, high or ultra)"};
        }

        if (flag)
        {
            const std::optional<GraphicsPreset> preset = parseGraphicsPreset(*flag);

            if (!preset || *preset == GraphicsPreset::Custom)
            {
                return Error{ErrorCode::InvalidArgument,
                    "'" + std::string(*flag) + "' is not a graphics preset (low, medium, high or ultra)"};
            }

            startFromPreset(config, *preset);
            config.origin = "--graphics";
            return config;
        }

        std::error_code error;

        if (std::filesystem::is_regular_file(config.file, error))
        {
            applyFile(config, readEntries(config.file));
            config.origin = config.file.string();
        }

        return config;
    }

    Status saveGraphicsQuality(const std::filesystem::path& file, const GraphicsQuality& quality)
    {
        SettingsFile settings = SettingsFile::load(file);
        setGraphicsValue(settings, k_presetKey, toString(quality.preset));
        setGraphicsValue(settings, k_antiAliasingKey, antiAliasingName(quality.antiAliasing));

        for (const FieldKey& field : k_fields)
        {
            setGraphicsValue(settings, field.key, formatField(quality, field.field));
        }

        return settings.save();
    }

    Status saveWindowSize(const std::filesystem::path& file, uint32_t width, uint32_t height)
    {
        SettingsFile settings = SettingsFile::load(file);
        setGraphicsValue(settings, k_windowWidthKey, std::to_string(std::clamp(width, k_minWindowSide, k_maxWindowSide)));
        setGraphicsValue(settings, k_windowHeightKey,
            std::to_string(std::clamp(height, k_minWindowSide, k_maxWindowSide)));

        return settings.save();
    }
} //namespace lockstep::render
