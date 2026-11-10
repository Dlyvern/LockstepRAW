#ifndef LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_RELOAD_HPP
#define LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_RELOAD_HPP

#include "Characters/ViewModel.hpp"

namespace game
{
    // Looks up the reload clips (ReloadTactical, ReloadEmpty) and the switch clips (Draw, Holster)
    // of a first-person model and logs missing reload clips.
    // model: the model, updated.
    void findReloadClips(FirstPersonModel& model);

    // model:   a first-person model.
    // Returns: true if it has both reload clips.
    [[nodiscard]] bool hasReloadClips(const FirstPersonModel& model);

    // model:   a first-person model.
    // Returns: true if it has both switch clips.
    [[nodiscard]] bool hasSwitchClips(const FirstPersonModel& model);

    // Plays the switch clips while a weapon switch runs (Holster, then Draw, each stretched to its
    // phase, not looping; a phase joined late starts without a fade), otherwise starts the reload
    // clip when a reload starts (stretched to the reload's length; the clips start and end in the
    // aiming pose), and fades back to the aiming pose when a clip's phase ends. Call before the
    // instance updates.
    // state:     the viewmodel state.
    // model:     the first-person model.
    // reload:    this frame's reload.
    // switching: this frame's weapon switch phase.
    void playReload(ViewModelState& state, const FirstPersonModel& model, const ViewModelReload& reload,
        const ViewModelSwitch& switching);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_RELOAD_HPP
