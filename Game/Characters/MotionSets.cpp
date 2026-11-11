#include "Characters/MotionSets.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        constexpr std::array<std::string_view, k_motionStanceCount> k_prefixes{"MM_Rifle_", "MM_Aim_", "MM_Crouch_",
            "MM_Pistol_"};
        constexpr std::array<std::string_view, 3> k_oneShotWords{"Start", "Stop", "Turn"};

        bool isOneShot(std::string_view name)
        {
            for (const std::string_view word : k_oneShotWords)
            {
                if (name.find(word) != std::string_view::npos)
                {
                    return true;
                }
            }

            return false;
        }
    } //namespace

    std::optional<MotionSets> buildMotionSets(anim::SkinnedModel& model, const anim::LocomotionRig& rig)
    {
        MotionSets sets{};
        bool hasAny = false;

        for (std::size_t stance = 0; stance < k_motionStanceCount; ++stance)
        {
            std::vector<anim::MotionClipSpec> specs;

            for (uint32_t clip = 0; clip < model.clips.size(); ++clip)
            {
                const std::string_view name = model.clips[clip].name;

                if (name.starts_with(k_prefixes[stance]))
                {
                    specs.push_back(anim::MotionClipSpec{.clip = clip, .isLooping = !isOneShot(name)});
                }
            }

            if (specs.empty())
            {
                continue;
            }

            auto database = anim::MotionDatabase::build(model, rig, specs);

            if (!database)
            {
                std::clog << "Game: motion set " << motionStanceName(static_cast<MotionStance>(stance)) << " of '"
                          << model.name << "' not built: " << database.error().message << '\n';
                continue;
            }

            std::clog << "Game: motion set " << motionStanceName(static_cast<MotionStance>(stance)) << " of '"
                      << model.name << "': " << specs.size() << " clips, " << database.value().frameCount() << " frames\n";
            sets.stances[stance] = std::move(database).value();
            hasAny = true;
        }

        return hasAny ? std::optional<MotionSets>(std::move(sets)) : std::nullopt;
    }

    const anim::MotionDatabase* motionDatabase(const MotionSets& sets, MotionStance stance)
    {
        const std::optional<anim::MotionDatabase>& database = sets.stances[static_cast<std::size_t>(stance)];
        return database ? &*database : nullptr;
    }

    MotionStance motionStance(bool isCrouched, bool isAiming, bool isPistol)
    {
        if (isPistol)
        {
            return MotionStance::Pistol;
        }

        if (isCrouched)
        {
            return MotionStance::Crouch;
        }

        return isAiming ? MotionStance::Aim : MotionStance::Rifle;
    }

    const char* motionStanceName(MotionStance stance)
    {
        switch (stance)
        {
            case MotionStance::Rifle:
                return "rifle";
            case MotionStance::Aim:
                return "aim";
            case MotionStance::Crouch:
                return "crouch";
            case MotionStance::Pistol:
                return "pistol";
        }

        return "?";
    }
} //namespace game
