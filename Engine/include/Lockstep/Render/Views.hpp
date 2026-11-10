#ifndef LOCKSTEP_RENDER_VIEWS_HPP
#define LOCKSTEP_RENDER_VIEWS_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Transform.hpp"
#include "Lockstep/Render/Resources.hpp"

#include <cstdint>

namespace lockstep::render
{
    // Render layers: a draw carries a bit mask of layers, and each view (the camera, a planar
    // reflector) draws only what shares a bit with its own mask. Engine gives meaning only to the
    // default layer and the viewmodel layer; Game assigns the other 30 bits (for example "only seen
    // in mirrors").
    using RenderLayers = uint32_t;

    // The layer every draw is on unless Game says otherwise, and the one every view sees by default.
    inline constexpr RenderLayers k_defaultRenderLayer{1u};

    // The first-person viewmodel layer (the local player's arms and weapon), reserved by Engine. A
    // draw with this bit is a viewmodel draw, whatever its other bits: it is drawn only in the
    // camera view (even when Camera::layers lacks the bit), with its own projection
    // (Camera::viewModelFov) and its depth squeezed in front of all world geometry, so it never
    // clips into walls but still occludes itself. It is never drawn in planar reflections, casts
    // no shadows (it receives them), and its ambient light is kept out of SSAO. It must use lit
    // (PBR) materials; other viewmodel draws are dropped. Game places it relative to the camera
    // every frame (camera transform times a hand offset), so when only the camera turns its motion
    // vectors are zero and TAA does not smear it.
    inline constexpr RenderLayers k_viewModelLayer{1u << 31};

    // Every layer.
    inline constexpr RenderLayers k_allRenderLayers{0xFFFFFFFFu};

    // A flat mirror for one frame. The reflector is a mesh instance Game also submits normally; the
    // Renderer finds that draw (same mesh and transform) and composites the reflection on it. The
    // mirror plane is derived from the mesh: its normal is the thinnest axis of the mesh's local
    // bounds, facing the camera, and it lies on the bounds face on the camera's side.
    // mesh:      the mirror's mesh.
    // transform: the mirror's world placement, exactly as submitted.
    // layers:    the layers the reflection shows; Game can show the local pawn (hidden from the
    //            first-person camera) or something else entirely.
    // tint:      linear RGB multiplier on the reflection (1 for a clean mirror).
    struct PlanarReflector
    {
        MeshHandle mesh{};
        math::Transform transform{};
        RenderLayers layers{k_defaultRenderLayer};
        math::Vec3 tint{1.0f, 1.0f, 1.0f};
    };
    // The reticle etched in a ScopeView's eyepiece, in the second focal plane: it stays the same
    // size on screen whatever the magnification and always marks the scope's axis. Sizes are
    // relative to the radius of the field of view (the field stop is 1).
    // color:            linear RGB of the etched lines (near black).
    // lineWidth:        width of the fine crosshair.
    // postWidth:        width of the heavy outer posts (0 for none).
    // postGap:          distance from the centre where the heavy posts start.
    // stadiaSpacing:    spacing of the tick marks on the fine lines (0 for none).
    // stadiaLength:     half-length of a tick mark.
    // dotRadius:        radius of the centre dot.
    // illumination:     linear RGB radiance of the illuminated centre dot (0 leaves it etched).
    struct ScopeReticle
    {
        math::Vec3 color{0.004f, 0.004f, 0.004f};
        float lineWidth{0.006f};
        float postWidth{0.035f};
        float postGap{0.42f};
        float stadiaSpacing{0.07f};
        float stadiaLength{0.018f};
        float dotRadius{0.02f};
        math::Vec3 illumination{0.0f, 0.0f, 0.0f};
    };

    // A magnified optic for one frame (picture in picture): the world is rendered a second time,
    // from the scope's objective along its axis with the scope's narrow field of view, into a
    // small texture, and shown on the eyepiece lens draw, magnified about the scope's axis as seen
    // from the camera, with the reticle on top. Because what the lens shows depends on the
    // direction from the eye through each lens pixel, sway, breathing and recoil move the image and
    // the reticle inside the lens, and an eye off the axis or away from the eye relief sees the
    // dark scope shadow. The second view never shows viewmodel draws.
    // lens:          the eyepiece glass mesh. The Renderer finds the draw with this mesh and
    //                transform (normally a k_viewModelLayer draw) and shows the scope on it.
    // lensTransform: the eyepiece glass placement, exactly as submitted.
    // eyepiece:      in the space the lens draw is placed in: the centre of the eyepiece glass,
    //                rotated so its -z is the scope's axis (towards the target) and +y its top.
    // objective:     world placement the magnified view is rendered from: the objective lens,
    //                -z along the axis, +y up. For a viewmodel scope, where the world camera shows
    //                the objective (the same correction as the viewmodel's muzzle socket).
    // magnification: how much the eyepiece magnifies, e.g. 4.
    // fieldOfView:   true field of view, full angle in radians: the angle of world the field stop
    //                shows. The eyepiece shows it magnified, so the apparent field is
    //                fieldOfView * magnification.
    // eyeRelief:     metres behind the eyepiece where the exit pupil is (the eye's place).
    // exitPupil:     radius of the exit pupil in metres; an eye farther off it sees the scope
    //                shadow. Gameplay values are a few times the optical one.
    // layers:        the render layers the scope view shows (leave out first-person bodies).
    // reticle:       the reticle.
    struct ScopeView
    {
        MeshHandle lens{};
        math::Transform lensTransform{};
        math::Transform eyepiece{};
        math::Transform objective{};
        float magnification{4.0f};
        float fieldOfView{math::radians(6.0f)};
        float eyeRelief{0.085f};
        float exitPupil{0.006f};
        RenderLayers layers{k_defaultRenderLayer};
        ScopeReticle reticle{};
    };

    // A holographic (reflex) sight for one frame: a 1x reticle projected at infinity, seen only
    // through the sight's window. Each window pixel shows the reticle where the ray from the eye
    // through it points along `direction`, so the reticle stays on the sight line while the window
    // frame moves with sway and recoil, drifts inside it, and is cut off at the window's edges when
    // the eye is off the axis. The reticle is an emissive ring with a centre dot (antialiased,
    // never thinner than a pixel), added on top of the window's glass.
    // window:          the window glass mesh (an alpha-blended lit material). The Renderer finds
    //                  the draw with this mesh and transform (normally a k_viewModelLayer draw).
    // windowTransform: the window placement, exactly as submitted.
    // direction:       unit direction of the sight line (where the reticle's centre lies, at
    //                  infinity), in the space the window draw is placed in.
    // radiance:        linear RGB radiance of the reticle (HDR: above 1 blooms).
    // ringRadius:      angular radius of the ring, radians (an EOTech 68 MOA ring is 34 MOA).
    // ringWidth:       angular width of the ring line, radians.
    // dotRadius:       angular radius of the centre dot, radians.
    struct HolographicSight
    {
        MeshHandle window{};
        math::Transform windowTransform{};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        math::Vec3 radiance{6.0f, 0.25f, 0.12f};
        float ringRadius{math::radians(34.0f / 60.0f)};
        float ringWidth{math::radians(2.0f / 60.0f)};
        float dotRadius{math::radians(0.5f / 60.0f)};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_VIEWS_HPP
