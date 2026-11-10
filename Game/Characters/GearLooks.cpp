#include "Characters/GearLooks.hpp"

#include <cctype>
#include <limits>
#include <string>
#include <utility>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        constexpr std::array<std::pair<std::string_view, GearLook>, 3> k_lookSuffixes{{
            {"full", GearLook::Full},
            {"nonvg", GearLook::NoNvg},
            {"balaclava", GearLook::Balaclava}}};
        constexpr std::array<GearLook, 5> k_squadMix{GearLook::Full, GearLook::NoNvg, GearLook::Full,
            GearLook::Balaclava, GearLook::NoNvg};
        constexpr std::array<GearLook, 4> k_mercMix{GearLook::Balaclava, GearLook::Balaclava, GearLook::NoNvg,
            GearLook::Balaclava};

        std::string lowered(std::string_view text)
        {
            std::string result(text);

            for (char& character : result)
            {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }

            return result;
        }

        std::string_view withoutBlenderEnding(std::string_view name)
        {
            return name.substr(0, name.find('.'));
        }

        uint32_t markerNumber(std::string_view name)
        {
            const std::string_view base = withoutBlenderEnding(name);
            std::size_t end = base.size();

            while (end > 0 && std::isdigit(static_cast<unsigned char>(base[end - 1])) == 0)
            {
                --end;
            }

            std::size_t begin = end;

            while (begin > 0 && std::isdigit(static_cast<unsigned char>(base[begin - 1])) != 0)
            {
                --begin;
            }

            if (begin == end)
            {
                uint32_t hash = 2166136261u;

                for (const char character : base)
                {
                    hash = (hash ^ static_cast<unsigned char>(character)) * 16777619u;
                }

                return hash;
            }

            uint32_t number = 0;

            for (std::size_t index = begin; index < end && index < begin + 9; ++index)
            {
                number = number * 10 + static_cast<uint32_t>(base[index] - '0');
            }

            return number;
        }

        anim::PartMask partBit(std::size_t index)
        {
            return index < std::numeric_limits<anim::PartMask>::digits ? anim::PartMask{1} << index : 0;
        }
    } //namespace

    GearMasks findGearMasks(const anim::SkinnedModel& model)
    {
        anim::PartMask nvg = 0;
        anim::PartMask helmetAndHeadset = 0;

        for (std::size_t index = 0; index < model.parts.size(); ++index)
        {
            const std::string_view material = model.parts[index].materialName;

            if (material == k_gearMaterials.nvg)
            {
                nvg |= partBit(index);
            }
            else if (material == k_gearMaterials.helmet || material == k_gearMaterials.headset)
            {
                helmetAndHeadset |= partBit(index);
            }
        }

        return GearMasks{anim::k_allParts, anim::k_allParts & ~nvg, anim::k_allParts & ~nvg & ~helmetAndHeadset};
    }

    anim::PartMask gearMask(const GearMasks& masks, GearLook look)
    {
        return masks[static_cast<std::size_t>(look)];
    }

    std::optional<GearLook> gearLookSuffix(std::string_view name)
    {
        std::string_view rest = withoutBlenderEnding(name);

        while (!rest.empty())
        {
            const std::size_t separator = rest.find('_');
            const std::string part = lowered(rest.substr(0, separator));

            for (const auto& [suffix, look] : k_lookSuffixes)
            {
                if (part == suffix)
                {
                    return look;
                }
            }

            rest = separator == std::string_view::npos ? std::string_view{} : rest.substr(separator + 1);
        }

        return std::nullopt;
    }

    GearLook playerGearLook(lockstep::world::EntityOwner owner)
    {
        return owner == lockstep::world::EntityOwner::Player2 ? GearLook::NoNvg : GearLook::Full;
    }

    GearLook squadGearLook(std::string_view name)
    {
        return gearLookSuffix(name).value_or(k_squadMix[markerNumber(name) % k_squadMix.size()]);
    }

    GearLook mercGearLook(std::string_view name)
    {
        return gearLookSuffix(name).value_or(k_mercMix[markerNumber(name) % k_mercMix.size()]);
    }
} //namespace game
