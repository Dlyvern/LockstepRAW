#include "Characters/ViewModelReload.hpp"

#include <algorithm>
#include <iostream>
#include <string_view>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        constexpr std::string_view k_tacticalClip{"ReloadTactical"};
        constexpr std::string_view k_emptyClip{"ReloadEmpty"};
        constexpr float k_startFade{0.1f};
        constexpr float k_endFade{0.15f};
        constexpr float k_minSeconds{0.1f};
    } //namespace

    void findReloadClips(FirstPersonModel& model)
    {
        const anim::SkinnedModel& skinned = model.character.skinned();
        model.tacticalReloadClip = anim::findClip(skinned.clips, k_tacticalClip);
        model.emptyReloadClip = anim::findClip(skinned.clips, k_emptyClip);

        if (!hasReloadClips(model))
        {
            std::clog << "Game: viewmodel '" << skinned.name << "' has no " << k_tacticalClip << "/" << k_emptyClip
                      << " clips; reloads lower the weapon instead\n";
        }
    }

    bool hasReloadClips(const FirstPersonModel& model)
    {
        return model.tacticalReloadClip.has_value() && model.emptyReloadClip.has_value();
    }

    void playReload(ViewModelState& state, const FirstPersonModel& model, const ViewModelReload& reload)
    {
        if (!hasReloadClips(model))
        {
            return;
        }

        anim::Animator& animator = state.instance.animator();

        if (reload.isActive && !state.isReloadPlaying)
        {
            const uint32_t clip = reload.isEmpty ? *model.emptyReloadClip : *model.tacticalReloadClip;
            const float length = model.character.skinned().clips[clip].duration;
            animator.play(clip, k_startFade, anim::PlaybackOptions{.loop = false,
                .speed = length / std::max(reload.seconds, k_minSeconds)});
            state.isReloadPlaying = true;
        }
        else if (!reload.isActive && state.isReloadPlaying)
        {
            animator.play(model.aimClip, k_endFade);
            state.isReloadPlaying = false;
        }
    }
} //namespace game
