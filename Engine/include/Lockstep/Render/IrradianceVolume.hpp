#ifndef LOCKSTEP_RENDER_IRRADIANCE_VOLUME_HPP
#define LOCKSTEP_RENDER_IRRADIANCE_VOLUME_HPP

#include "Lockstep/Core/Math.hpp"

#include <cstdint>
#include <vector>

namespace lockstep::render
{
    // Probes one axis of an irradiance volume may have.
    inline constexpr uint32_t k_maxIrradianceProbesPerAxis{256};

    // Probes one irradiance volume may have in total.
    inline constexpr uint32_t k_maxIrradianceProbes{256u * 1024u};

    // Baked indirect light at one point of an irradiance volume, as order-1 spherical harmonics
    // already convolved with the cosine lobe: each term is four floats {a, bx, by, bz}, and the
    // irradiance divided by pi arriving at a surface with unit normal n is max(a + dot(b, n), 0).
    // Multiplied by a surface's diffuse colour it is the light the surface reflects, in the
    // renderer's radiance units (the units a PointLight of intensity 1 lights a white surface 1 m
    // away with). Each colour term holds red, green, blue one after another (12 floats).
    // local:         light of the level's static point and spot lights after one or more bounces
    //                (the direct light itself is not included; the renderer adds that).
    // sun:           light of a sun of radiance 1 from IrradianceVolume::sunTravel after bounces,
    //                direct sunlight excluded; scaled by the frame's sun radiance.
    // sky:           light of a uniform sky of radiance 1 after at least one bounce (direct sky light
    //                excluded); scaled by the frame's mean sky radiance.
    // skyVisibility: how much of a uniform sky of radiance 1 is seen directly (1 everywhere for a
    //                point in the open with n facing up, 0 deep inside a closed room); the renderer
    //                multiplies the sky's image-based diffuse light by it.
    struct IrradianceProbe
    {
        float local[12];
        float sun[12];
        float sky[12];
        float skyVisibility[4];
    };

    // A box-shaped grid of baked irradiance probes covering a level's interiors, sampled by every
    // lit surface (static and dynamic) inside it with trilinear filtering. Probes that ended up
    // inside geometry are filled from their neighbours when baked.
    // origin:    world position of the first probe (index 0, 0, 0).
    // spacing:   distance between neighbouring probes along x, y and z, in metres.
    // countX/Y/Z: probes along each axis, each 2 to k_maxIrradianceProbesPerAxis.
    // sunTravel: unit direction the sunlight of IrradianceProbe::sun travels in; zero if the bake
    //            had no sun (the sun term is then all zero).
    // probes:    countX * countY * countZ probes, x varying fastest, then y, then z.
    struct IrradianceVolume
    {
        math::Vec3 origin{0.0f, 0.0f, 0.0f};
        math::Vec3 spacing{1.0f, 1.0f, 1.0f};
        uint32_t countX{0};
        uint32_t countY{0};
        uint32_t countZ{0};
        math::Vec3 sunTravel{0.0f, 0.0f, 0.0f};
        std::vector<IrradianceProbe> probes;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_IRRADIANCE_VOLUME_HPP
