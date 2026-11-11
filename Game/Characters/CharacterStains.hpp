#ifndef LOCKSTEP_GAME_CHARACTERS_CHARACTER_STAINS_HPP
#define LOCKSTEP_GAME_CHARACTERS_CHARACTER_STAINS_HPP

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/CharacterSurface.hpp>

#include <array>
#include <cstdint>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // How blood, dirt and rain show on characters, editable from the dev panel. Presentation only:
    // both machines add the same stains from the replicated gore events.
    // enabled:      draw the layers (the render setting and graphics quality can turn them off too).
    // hitRadius:    metres a bullet hit's blood reaches on the body.
    // severRadius:  metres a stump's or burst neck's blood reaches.
    // hitDirt:      dirt smeared around a hit, 0 to 1.
    // grime:        general patchy dirt on every character, 0 to 1.
    // drySeconds:   World seconds fresh blood takes to dry dark and matte.
    // soakSeconds:  World seconds a character outdoors in full rain takes to soak.
    // dryOffSeconds: World seconds a soaked character indoors takes to dry.
    // drill:        dev (`--dev-bloody-npcs`): every NPC gets blood at its hips, left elbow,
    //               right hip and head when first drawn, to look at the layers.
    struct CharacterStainTuning
    {
        bool enabled{true};
        float hitRadius{0.075f};
        float severRadius{0.14f};
        float hitDirt{0.5f};
        float grime{0.12f};
        float drySeconds{150.0f};
        float soakSeconds{20.0f};
        float dryOffSeconds{240.0f};
        bool drill{false};
    };

    // The blood and rain on one character's body (an NPC or a pawn).
    // stains:    the stains in the body's bind pose; the oldest is replaced once all are used.
    // count:     stains used.
    // next:      the slot the next stain replaces when full.
    // soak:      rain soak kept from being outdoors, 0 to 1.
    // seed:      noise offset (set from the character's entity).
    // surface:   the character surface queued for this frame's draws.
    struct BodyStains
    {
        std::array<lockstep::render::SurfaceStain, lockstep::render::k_maxSurfaceStains> stains{};
        uint32_t count{0};
        uint32_t next{0};
        float soak{0.0f};
        float seed{0.0f};
        lockstep::render::CharacterSurfaceId surface{lockstep::render::k_noCharacterSurface};
    };

    // Adds a blood stain where a world point lies on a posed body.
    // body:      the body's stains.
    // instance:  the body's animated instance (maps the point into the bind pose).
    // placement: the instance's world placement as drawn.
    // point:     the world point (a hit, a stump).
    // radius:    metres the blood reaches.
    // dirt:      dirt smeared around it, 0 to 1.
    void addBodyStain(BodyStains& body, const lockstep::anim::SkinnedInstance& instance,
        const math::Transform& placement, const math::Vec3& point, float radius, float dirt);

    // Dries the body's blood and follows the rain, then queues its character surface for this
    // frame (BodyStains::surface; none when switched off).
    // renderer:   the renderer.
    // body:       the body's stains.
    // tuning:     the look.
    // rain:       the weather's surface wetness where the character stands, 0 indoors.
    // seconds:    World seconds since the last frame.
    void prepareBodyStains(lockstep::render::Renderer& renderer, BodyStains& body, const CharacterStainTuning& tuning,
        float rain, float seconds);

    // Forgets every stain and the soak.
    // body: the body's stains.
    void clearBodyStains(BodyStains& body);

#if LOCKSTEP_DEV_TOOLS
    // Draws the character stain section of the gore dev panel.
    // tuning: the look.
    void characterStainsDevSection(CharacterStainTuning& tuning);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_CHARACTER_STAINS_HPP
