#ifndef LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_RELOAD_HPP
#define LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_RELOAD_HPP

#include "Characters/ViewModel.hpp"

namespace game
{
    // Looks up the reload clips (ReloadTactical, ReloadEmpty) of a first-person model and logs
    // which are missing.
    // model: the model, updated.
    void findReloadClips(FirstPersonModel& model);

    // model:   a first-person model.
    // Returns: true if it has both reload clips.
    [[nodiscard]] bool hasReloadClips(const FirstPersonModel& model);

    // Starts the reload clip when a reload starts (stretched to the reload's length, not looping;
    // the clips start and end in the aiming pose) and fades back to the aiming pose when it ends.
    // Call before the instance updates.
    // state:  the viewmodel state.
    // model:  the first-person model.
    // reload: this frame's reload.
    void playReload(ViewModelState& state, const FirstPersonModel& model, const ViewModelReload& reload);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_RELOAD_HPP
