#include "CookerArguments.hpp"

#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace lockstep::cooker
{
    namespace
    {
        Error invalid(std::string message)
        {
            return Error{ErrorCode::InvalidArgument, std::move(message)};
        }

        template<typename T>
        Result<T> parseNumber(std::string_view flag, std::string_view text)
        {
            T value{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

            if (error != std::errc{} || end != text.data() + text.size())
            {
                return invalid(std::string(flag) + ": '" + std::string(text) + "' is not a number");
            }

            return T{value};
        }

        constexpr std::string_view k_noAutoCap{"--no-auto-cap"};
        constexpr std::string_view k_limbs{"--limbs"};
        constexpr std::string_view k_noSurfaceDetail{"--no-surface-detail"};
        constexpr std::string_view k_singleSided{"--single-sided"};

        struct ParsedFlags
        {
            std::vector<std::string_view> positional;
            std::vector<std::pair<std::string_view, std::string_view>> flags;
        };

        Result<ParsedFlags> splitArguments(std::span<const char* const> arguments)
        {
            ParsedFlags parsed{};

            for (std::size_t index = 0; index < arguments.size(); ++index)
            {
                const std::string_view argument = arguments[index] != nullptr ? arguments[index] : "";

                if (!argument.starts_with("--"))
                {
                    parsed.positional.push_back(argument);
                    continue;
                }

                if (argument == k_noAutoCap || argument == k_limbs || argument == k_noSurfaceDetail
                    || argument == k_singleSided)
                {
                    parsed.flags.emplace_back(argument, std::string_view{});
                    continue;
                }

                if (index + 1 >= arguments.size() || arguments[index + 1] == nullptr)
                {
                    return invalid(std::string(argument) + " needs a value");
                }

                parsed.flags.emplace_back(argument, arguments[++index]);
            }

            return parsed;
        }

        Result<CookerCommand> parsePack(const ParsedFlags& parsed)
        {
            if (parsed.positional.size() != 3)
            {
                return invalid("pack needs <cooked dir> and <out.pak>");
            }

            PackOptions options{};
            options.directory = parsed.positional[1];
            options.pak = parsed.positional[2];

            for (const auto& [flag, value] : parsed.flags)
            {
                if (flag == "--zstd-level")
                {
                    auto level = parseNumber<int>(flag, value);

                    if (!level || level.value() < 1 || level.value() > 22)
                    {
                        return invalid("--zstd-level must be 1 to 22");
                    }

                    options.zstdLevel = level.value();
                }
                else if (flag == "--threads")
                {
                    auto threads = parseNumber<uint32_t>(flag, value);

                    if (!threads)
                    {
                        return threads.error();
                    }

                    options.threads = threads.value();
                }
                else
                {
                    return invalid("pack: unknown option " + std::string(flag));
                }
            }

            return CookerCommand{options};
        }

        Result<CookerCommand> parseCook(const ParsedFlags& parsed)
        {
            if (parsed.positional.size() != 1)
            {
                return invalid("cook needs exactly one source file");
            }

            CookOptions options{};
            options.source = parsed.positional[0];

            for (const auto& [flag, value] : parsed.flags)
            {
                if (flag == "--out")
                {
                    options.out = value;
                }
                else if (flag == "--name")
                {
                    options.name = value;
                }
                else if (flag == "--scale")
                {
                    auto scale = parseNumber<float>(flag, value);

                    if (!scale || !(scale.value() > 0.0f) || !std::isfinite(scale.value()))
                    {
                        return invalid("--scale must be a positive number");
                    }

                    options.scale = scale.value();
                }
                else if (flag == "--threads")
                {
                    auto threads = parseNumber<uint32_t>(flag, value);

                    if (!threads)
                    {
                        return threads.error();
                    }

                    options.threads = threads.value();
                }
                else if (flag == "--max-texture-size")
                {
                    auto size = parseNumber<uint32_t>(flag, value);

                    if (!size || size.value() < 4)
                    {
                        return invalid("--max-texture-size must be at least 4");
                    }

                    options.textureCaps.maxSize = size.value();
                }
                else if (flag == "--texels-per-metre")
                {
                    auto density = parseNumber<float>(flag, value);

                    if (!density || !(density.value() > 0.0f) || !std::isfinite(density.value()))
                    {
                        return invalid("--texels-per-metre must be a positive number");
                    }

                    options.textureCaps.texelsPerMetre = density.value();
                }
                else if (flag == k_noAutoCap)
                {
                    options.textureCaps.automatic = false;
                }
                else if (flag == k_limbs)
                {
                    options.splitLimbs = true;
                }
                else if (flag == "--wound")
                {
                    options.wound = std::filesystem::path(value);
                }
                else if (flag == "--height-dir")
                {
                    options.surfaces.heightDirectories.emplace_back(value);
                }
                else if (flag == k_noSurfaceDetail)
                {
                    options.surfaces.detail = false;
                }
                else if (flag == k_singleSided)
                {
                    options.singleSided = true;
                }
                else
                {
                    return invalid("unknown option " + std::string(flag));
                }
            }

            if (options.out.empty() || options.name.empty())
            {
                return invalid("cook needs --out and --name");
            }

            return CookerCommand{options};
        }
    } //namespace

    Result<CookerCommand> parseCookerArguments(std::span<const char* const> arguments)
    {
        auto parsed = splitArguments(arguments);

        if (!parsed)
        {
            return parsed.error();
        }

        if (!parsed.value().positional.empty() && parsed.value().positional[0] == "pack")
        {
            return parsePack(parsed.value());
        }

        return parseCook(parsed.value());
    }
} //namespace lockstep::cooker
