#ifndef LOCKSTEP_GAME_WEAPONS_OPTICS_HPP
#define LOCKSTEP_GAME_WEAPONS_OPTICS_HPP

#include "Weapons/WeaponKind.hpp"

#include "Weapons/OpticKind.hpp"

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/Resources.hpp>
#include <Lockstep/Render/Views.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // Which optic each weapon of the local loadout carries. The partner sees the carried optic on
    // the third-person weapon (PlayerCommand::optic, replicated through PlayerStatus::optic).
    // byWeapon: per WeaponKind, the optic on its Optic rail socket: the M4 carries the holographic
    //           sight by default, the KRISS iron sights; the Glock has no rail for one.
    struct OpticLoadout
    {
        std::array<OpticKind, k_weaponCount> byWeapon{OpticKind::Holographic, OpticKind::None, OpticKind::None};
    };

    // How the holographic sight looks through and handles; editable from the dev panel.
    // eyeDistance:  metres from the rear window to the eye when aiming (a cheek weld; a
    //               holographic sight works at any distance).
    // radiance:     linear RGB radiance of the reticle at brightness 1 (HDR, it blooms a little).
    // brightness:   the sight's brightness setting, a multiplier on `radiance`.
    // ringRadius:   angular radius of the ring, MOA (the 68 MOA ring is 34).
    // ringWidth:    angular width of the ring line, MOA.
    // dotRadius:    angular radius of the centre dot, MOA.
    struct HolographicTuning
    {
        float eyeDistance{0.14f};
        math::Vec3 radiance{5.0f, 0.2f, 0.1f};
        float brightness{1.0f};
        float ringRadius{34.0f};
        float ringWidth{2.0f};
        float dotRadius{0.5f};
    };

    // How the 4x scope looks through and handles; editable from the dev panel.
    // magnification:  eyepiece magnification.
    // fieldOfView:    true field of view, full angle, radians.
    // eyeRelief:      metres from the eyepiece glass to the eye when aiming.
    // exitPupil:      radius of the eye box in metres (larger is more forgiving; the optical exit
    //                 pupil of a 4x32 is 4 mm).
    // swayScale:      multiplier on the breathing sway while aiming through it.
    // sensitivityPower: aimed look sensitivity is divided by magnification to this power (1 keeps
    //                 the magnified image moving at the unmagnified speed; 0 ignores the magnification).
    // illuminated:    light the reticle's centre dot.
    // illumination:   linear RGB radiance of the lit dot.
    // reticle:        the etched reticle.
    // outerBlur:      depth of field strength while aimed through it (the view around the scope
    //                 softens; the scope stays sharp), 0 for none.
    struct OpticTuning
    {
        float magnification{4.0f};
        float fieldOfView{math::radians(6.5f)};
        float eyeRelief{0.075f};
        float exitPupil{0.006f};
        float swayScale{1.6f};
        float sensitivityPower{1.0f};
        bool illuminated{true};
        math::Vec3 illumination{6.0f, 0.15f, 0.05f};
        lockstep::render::ScopeReticle reticle{};
        float outerBlur{0.35f};
    };

    // One drawn part of an optic model, in the optic's space: origin at the mount's rail contact,
    // -z towards the muzzle, +y up (the cooked scope's node space).
    // mesh, material: what to draw.
    // local:          its placement in the optic's space.
    // isLens:         the eyepiece glass the scope view, or the window the reticle, is shown on.
    struct OpticPart
    {
        lockstep::render::MeshHandle mesh{};
        lockstep::render::MaterialHandle material{};
        math::Transform local{};
        bool isLens{false};
    };

    // A loaded optic model: the 4x scope (Content/source/weapons/scope1/scope4x.glb, cooked as
    // characters/scope4x/scene) or the holographic sight (eotech.glb, characters/eotech/scene).
    // kind:      which optic it is.
    // scene:     the loaded scene (owns the GPU handles).
    // parts:     the parts drawn.
    // eyepiece:  the eyepiece glass (rear window) centre in the optic's space, -z along the axis.
    // objective: the objective glass (front window) centre in the optic's space.
    struct OpticModel
    {
        OpticKind kind{OpticKind::None};
        lockstep::assets::Scene scene;
        std::vector<OpticPart> parts;
        math::Transform eyepiece{};
        math::Transform objective{};
    };

    // The optics of this machine.
    // holographic:       the holographic sight model, or nothing when it isn't mounted.
    // scope:             the 4x scope model, or nothing when it isn't mounted.
    // holographicTuning: how the holographic sight looks and handles.
    // scopeTuning:       how the scope looks and handles.
    // loadout:           the local player's optic choices.
    struct Optics
    {
        std::optional<OpticModel> holographic{};
        std::optional<OpticModel> scope{};
        HolographicTuning holographicTuning{};
        OpticTuning scopeTuning{};
        OpticLoadout loadout{};
    };

    // Loads the optic models from the mounted content (characters.pak).
    // renderer: uploads the models.
    // files:    the mounted content.
    // Returns:  the optics; a model missing from the content is logged and left out.
    [[nodiscard]] Optics loadOptics(lockstep::render::Renderer& renderer, const lockstep::assets::FileSystem& files);

    // optics:  the optics.
    // weapon:  a loadout weapon.
    // Returns: the model of the optic the local loadout puts on it, or null for none (or not loaded).
    [[nodiscard]] const OpticModel* weaponOptic(const Optics& optics, WeaponKind weapon);

    // optics:  the optics.
    // kind:    an optic.
    // Returns: its model, or null for none (or not loaded).
    [[nodiscard]] const OpticModel* opticModel(const Optics& optics, OpticKind kind);

    // mount: an optic's placement (its rail contact on a weapon's optic socket).
    // local: a placement in the optic's space.
    // Returns: that placement in the mount's space.
    [[nodiscard]] math::Transform placeOnMount(const math::Transform& mount, const math::Transform& local);

    // Queues every part of an optic for drawing.
    // renderer: the renderer.
    // model:    the optic.
    // mount:    where it sits (WeaponSocket::optic).
    // layers:   the render layers to draw on.
    void drawOptic(lockstep::render::Renderer& renderer, const OpticModel& model, const math::Transform& mount,
        lockstep::render::RenderLayers layers);

#if LOCKSTEP_DEV_TOOLS
    // Draws the optics section of the Game dev panel: the loadout choice and the holographic sight
    // and scope tuning.
    // optics: the optics.
    void opticsDevSection(Optics& optics);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_OPTICS_HPP
