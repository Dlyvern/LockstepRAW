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
        constexpr std::string_view k_drawClip{"Draw"};
        constexpr std::string_view k_holsterClip{"Holster"};
        constexpr uint8_t k_holsterPlaying{1};
        constexpr uint8_t k_drawPlaying{2};
        constexpr float k_startFade{0.1f};
        constexpr float k_endFade{0.15f};
        constexpr float k_minSeconds{0.1f};

        bool playSwitch(ViewModelState& state, const FirstPersonModel& model, const ViewModelSwitch& switching)
        {
            anim::Animator& animator = state.instance.animator();
            const uint8_t wanted = switching.isHolstering ? k_holsterPlaying : switching.isDrawing ? k_drawPlaying : 0;
            const std::optional<uint32_t> clip = wanted == k_holsterPlaying ? model.holsterClip
                : wanted == k_drawPlaying ? model.drawClip
                : std::nullopt;

            if (wanted != 0 && clip && state.switchClip != wanted)
            {
                const float length = model.character.skinned().clips[*clip].duration;
                const float phase = std::max(switching.seconds, k_minSeconds);
                const float started = std::clamp(1.0f - switching.left / phase, 0.0f, 1.0f);
                animator.play(*clip, started > 0.5f ? 0.0f : k_startFade, anim::PlaybackOptions{.loop = false,
                    .speed = length / phase});
                state.switchClip = wanted;
                state.isReloadPlaying = false;
                return true;
            }

            if (wanted == 0 && state.switchClip != 0)
            {
                animator.play(model.aimClip, k_endFade);
                state.switchClip = 0;
            }

            return state.switchClip != 0;
        }
    } //namespace

    void findReloadClips(FirstPersonModel& model)
    {
        const anim::SkinnedModel& skinned = model.character.skinned();
        model.tacticalReloadClip = anim::findClip(skinned.clips, k_tacticalClip);
        model.emptyReloadClip = anim::findClip(skinned.clips, k_emptyClip);
        model.drawClip = anim::findClip(skinned.clips, k_drawClip);
        model.holsterClip = anim::findClip(skinned.clips, k_holsterClip);

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

    bool hasSwitchClips(const FirstPersonModel& model)
    {
        return model.drawClip.has_value() && model.holsterClip.has_value();
    }

    void playReload(ViewModelState& state, const FirstPersonModel& model, const ViewModelReload& reload,
        const ViewModelSwitch& switching)
    {
        if (playSwitch(state, model, switching))
        {
            return;
        }

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
