#ifndef LOCKSTEP_RENDER_MATERIAL_SHADING_SETTINGS_HPP
#define LOCKSTEP_RENDER_MATERIAL_SHADING_SETTINGS_HPP

// Render settings for the extra shading models (PbrMaterial.hpp ShadingModel), character surface
// layers (CharacterSurface.hpp) and decal relief, part of RenderSettings.
namespace lockstep::render
{
    // skin:             shade ShadingModel::Skin materials with subsurface scattering (off: standard).
    // skinScatter:      multiplier on every skin material's scatter strength, 0 to 2.
    // fabricSheen:      add the sheen lobe of ShadingModel::Fabric materials.
    // sheenIntensity:   multiplier on every fabric material's sheen, 0 to 2.
    // characterSurfaces: draw blood, dirt and rain soak passed with skinned draws
    //                   (Renderer::submitCharacterSurface()); off: characters show their plain
    //                   materials (and the world wetness, like any surface).
    // decalRelief:      shade relief decals (MaterialShading::decalRelief) with their relief.
    // decalReliefScale: multiplier on every decal's relief height, 0 to 3.
    struct MaterialShadingSettings
    {
        bool skin{true};
        float skinScatter{1.0f};
        bool fabricSheen{true};
        float sheenIntensity{1.0f};
        bool characterSurfaces{true};
        bool decalRelief{true};
        float decalReliefScale{1.0f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_MATERIAL_SHADING_SETTINGS_HPP
