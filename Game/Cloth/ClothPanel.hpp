#ifndef LOCKSTEP_GAME_CLOTH_CLOTH_PANEL_HPP
#define LOCKSTEP_GAME_CLOTH_CLOTH_PANEL_HPP

#include <Lockstep/Core/Math.hpp>

#include <cstdint>
#include <vector>

namespace game
{
    // A hanging cloth panel simulated as a grid of particles (position-based Verlet) with its top row
    // pinned, in the panel's own space: x across (centred), y up with the top edge at 0, z the
    // panel's normal. Presentation only.
    // columns, rows: particle grid size (row 0 is the pinned top edge).
    // width, height: the panel's size in metres.
    // rest:      each particle's rest position (a flat sheet hanging straight down).
    // positions: current positions.
    // previous:  positions one step ago (Verlet velocity).
    // links:     distance constraints: particle pairs and rest lengths (stretch, shear, bend).
    struct ClothPanel
    {
        // A distance constraint between two particles.
        struct Link
        {
            uint32_t first{0};
            uint32_t second{0};
            float length{0.0f};
            float stiffness{1.0f};
        };

        uint32_t columns{0};
        uint32_t rows{0};
        float width{1.0f};
        float height{1.0f};
        std::vector<lockstep::math::Vec3> rest;
        std::vector<lockstep::math::Vec3> positions;
        std::vector<lockstep::math::Vec3> previous;
        std::vector<Link> links;
    };

    // Forces on a panel for one step, in panel space.
    // gravity: acceleration in m/s^2.
    // wind:    air velocity in m/s (pushes the sheet along its normal).
    // damping: share of velocity kept per step, 0 to 1.
    struct ClothForces
    {
        lockstep::math::Vec3 gravity{0.0f, -9.81f, 0.0f};
        lockstep::math::Vec3 wind{0.0f, 0.0f, 0.0f};
        float damping{0.985f};
    };

    // Builds a resting panel.
    // width, height: size in metres.
    // spacing:       target distance between particles in metres (the grid is clamped to a few
    //                particles either way).
    // Returns:       the panel.
    [[nodiscard]] ClothPanel createClothPanel(float width, float height, float spacing);

    // Advances the panel by one fixed step: integrate, then satisfy the links (top row pinned).
    // panel:   the panel.
    // forces:  forces this step.
    // seconds: the step length.
    void stepCloth(ClothPanel& panel, const ClothForces& forces, float seconds);

    // Pushes the particles out of an upright capsule (a person walking through), dragging them
    // along with the person's movement.
    // panel:  the panel.
    // foot:   the bottom of the capsule's axis, in panel space.
    // height: the capsule's length.
    // radius: the capsule's radius.
    // Returns: true if a particle was moved.
    bool pushOutOfCapsule(ClothPanel& panel, const lockstep::math::Vec3& foot, float height, float radius);

    // Kicks the particles near a point (a bullet passing through), as a change of velocity.
    // panel:     the panel.
    // point:     where it hit, in panel space.
    // velocity:  the velocity change at the point in m/s per step, fading out over `radius`.
    // radius:    reach of the kick in metres.
    // seconds:   the step length (converts the velocity into a Verlet displacement).
    void kickCloth(ClothPanel& panel, const lockstep::math::Vec3& point, const lockstep::math::Vec3& velocity,
        float radius, float seconds);

    // panel:   the panel.
    // Returns: the largest distance any particle moved in the last step, in metres.
    [[nodiscard]] float clothMotion(const ClothPanel& panel);

    // Puts every particle back at rest.
    // panel: the panel.
    void restCloth(ClothPanel& panel);

    // Writes each particle's skinning matrix: from its rest frame to its current position and the
    // local orientation of the sheet around it (so normals follow the folds).
    // panel:  the panel.
    // joints: one matrix per particle, row-major grid order; resized.
    void clothJointMatrices(const ClothPanel& panel, std::vector<lockstep::math::Mat4>& joints);
} //namespace game

#endif //LOCKSTEP_GAME_CLOTH_CLOTH_PANEL_HPP
