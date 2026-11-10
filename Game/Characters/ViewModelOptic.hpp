#ifndef LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_OPTIC_HPP
#define LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_OPTIC_HPP

#include "Characters/ViewModel.hpp"
#include "Weapons/Optics.hpp"

#include <Lockstep/Render/Views.hpp>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // Sets up aiming through the optic on the viewmodel's rail before drawViewModel(): the aiming
    // placement that puts the eye on the optic's axis at its eye relief (computed from the
    // model's aiming pose, the optic level and uncanted; the scope's eye relief or the holographic
    // sight's cheek weld distance) and the sway. Without an optic it clears the optic aim, so the
    // iron sights are used.
    // state:  the viewmodel state, updated.
    // model:  the first-person model.
    // optic:  the mounted optic, or null for none.
    // optics: the optics' tuning.
    void prepareViewModelOptic(ViewModelState& state, const FirstPersonModel& model, const OpticModel* optic,
        const Optics& optics);

    // After drawViewModel(): draws the optic on the viewmodel's rail (k_viewModelLayer), submits
    // the holographic sight's reticle on its rear window (Renderer::submitSight(), whenever the
    // sight is drawn) or, while aiming, the scope view of the scope's eyepiece
    // (Renderer::submitScope()), and records where it points (ViewModelOpticAim::axis, which shots
    // follow). Does nothing while the viewmodel is hidden.
    // renderer: the renderer.
    // model:    the first-person model.
    // state:    the viewmodel state, drawn this frame.
    // optic:    the mounted optic, or null for none.
    // optics:   how the optics look.
    // worldFov: the world camera's vertical field of view this frame, radians.
    // layers:   the render layers the scope view shows.
    void presentViewModelOptic(lockstep::render::Renderer& renderer, const FirstPersonModel& model,
        ViewModelState& state, const OpticModel* optic, const Optics& optics, float worldFov,
        lockstep::render::RenderLayers layers);

    // optics:  the scope's magnification and sensitivity power.
    // optic:   the mounted optic, or null for none.
    // Returns: the multiplier on the aimed look sensitivity (1 without a magnified optic).
    [[nodiscard]] float opticSensitivityScale(const Optics& optics, const OpticModel* optic);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_OPTIC_HPP
