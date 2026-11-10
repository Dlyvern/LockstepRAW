#ifndef LOCKSTEP_RENDER_RENDER_SETTINGS_HPP
#define LOCKSTEP_RENDER_RENDER_SETTINGS_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Render/DetailSettings.hpp"
#include "Lockstep/Render/IndirectLightingSettings.hpp"
#include "Lockstep/Render/WetnessSettings.hpp"

#include <cstdint>

namespace lockstep::render
{
    // Most shadow cascades the sun can use.
    inline constexpr uint32_t k_maxShadowCascades{4};

    // Shadow atlas tiles shared by shadow-casting point and spot lights (a point light takes six).
    inline constexpr uint32_t k_maxLocalShadowTiles{16};

    // Most shadow-casting point and spot lights in one frame.
    inline constexpr uint32_t k_maxShadowedLocalLights{8};

    // Which lights and shading terms lit materials use.
    // directional:      use the sun (the submitted directional light, or the sky's sun).
    // localLights:      use point and spot lights.
    // specular:         add specular reflections (direct and from the environment).
    // ambientColor:     linear RGB light reaching every surface from everywhere, on top of
    //                   image-based lighting. Off (0) by default; for scenes without a sky.
    // ambientIntensity: multiplier on `ambientColor`.
    struct LightingSettings
    {
        bool directional{true};
        bool localLights{true};
        bool specular{true};
        math::Vec3 ambientColor{1.0f, 1.0f, 1.0f};
        float ambientIntensity{0.0f};
    };

    // Procedural daylight sky (Preetham) and the sun in it. The sky is drawn behind everything and
    // lights the scene through image-based lighting. When no DirectionalLight is submitted in a
    // frame, the sun described here is the frame's directional light (casting cascaded shadows);
    // a submitted DirectionalLight replaces it and the sky follows that light's direction.
    // enabled:      draw the sky and provide its sun.
    // sunAzimuth:   compass angle of the sun in radians: 0 is towards -Z, pi/2 towards +X.
    // sunElevation: angle of the sun above the horizon in radians.
    // sunColor:     linear RGB colour of sunlight above the atmosphere.
    // sunIntensity: brightness of sunlight; 1 lights a white surface facing the sun to 1.
    // atmosphericSunTint: redden and dim the sun by the air it passes through (low sun is orange).
    // turbidity:    haziness of the air, 2 (clear) to 10 (hazy).
    // skyIntensity: brightness multiplier of the sky radiance.
    // groundAlbedo: linear RGB reflectance of the ground below the horizon (lights from below).
    // sunDisk:      draw the sun's disk in the sky.
    // glowColor:    linear RGB of a sunset glow added to the sky near the horizon (the analytic model
    //               alone stays pale at low sun). It also lights the scene through the sky's
    //               image-based light.
    // glowIntensity: radiance of the glow at the horizon towards the sun; 0 turns it off.
    // glowHeight:   how high the glow reaches, as the sine of the elevation where it falls to 1/e.
    // glowSpread:   how tightly the glow gathers around the sun's azimuth; higher is narrower.
    // glowOpposite: share of the glow left on the horizon opposite the sun, 0 to 1.
    // zenithTint:   linear RGB multiplier on the sky overhead, fading to none at the horizon
    //               (deepens the blue above a sunset); white is neutral.
    struct SkySettings
    {
        bool enabled{true};
        float sunAzimuth{math::radians(35.0f)};
        float sunElevation{math::radians(42.0f)};
        math::Vec3 sunColor{1.0f, 0.97f, 0.92f};
        float sunIntensity{2.6f};
        bool atmosphericSunTint{true};
        float turbidity{2.5f};
        float skyIntensity{1.0f};
        math::Vec3 groundAlbedo{0.25f, 0.24f, 0.22f};
        bool sunDisk{true};
        math::Vec3 glowColor{1.0f, 0.45f, 0.2f};
        float glowIntensity{0.0f};
        float glowHeight{0.12f};
        float glowSpread{3.0f};
        float glowOpposite{0.2f};
        math::Vec3 zenithTint{1.0f, 1.0f, 1.0f};
    };

    // Image-based lighting from the sky: diffuse irradiance and prefiltered specular reflections,
    // with a split-sum BRDF lookup table. Needs the sky.
    // enabled:           light surfaces from the environment.
    // diffuseIntensity:  multiplier on environment diffuse light.
    // specularIntensity: multiplier on environment reflections.
    struct ImageBasedLightingSettings
    {
        bool enabled{true};
        float diffuseIntensity{1.0f};
        float specularIntensity{1.0f};
    };

    // Interiors inside the AmbientZones set with Renderer::setAmbientZones(): the sky's image-based
    // light is scaled down there and a flat fill light added, blended per pixel by world position.
    // enabled:           apply the zones.
    // diffuseIntensity:  multiplier on the sky's diffuse light fully inside a zone.
    // specularIntensity: multiplier on the sky's reflections fully inside a zone.
    // fillColor:         linear RGB of the fill light inside zones (light bounced off walls and
    //                    ceilings, which the sky-only image-based light lacks).
    // fillIntensity:     brightness of the fill light; lights diffuse and specular like an even
    //                    environment of that radiance.
    // blendDistance:     metres inside a zone's faces over which it fades in from nothing.
    // normalOffset:      metres the lookup is moved along the surface normal, so the outer face
    //                    of a wall on the zone's border stays outdoor and its inner face indoor.
    // debugView:         tint surfaces by zone weight (magenta inside, unchanged outside).
    struct InteriorAmbientSettings
    {
        bool enabled{true};
        float diffuseIntensity{0.12f};
        float specularIntensity{0.18f};
        math::Vec3 fillColor{0.62f, 0.68f, 0.78f};
        float fillIntensity{0.05f};
        float blendDistance{0.6f};
        float normalOffset{0.3f};
        bool debugView{false};
    };

    // Shadows: cascaded shadow maps for the sun and perspective maps for shadow-casting point and
    // spot lights, in one atlas. The sun's cascades take the atlas's left half (1024 texel tiles);
    // local lights share the right half, cut into k_maxLocalShadowTiles tiles of 512 texels: a spot
    // light takes one tile, a point light six (cube faces). Those sizes are for the default 4096
    // atlas; GraphicsQuality::shadowAtlasSize scales them.
    // enabled:       render and sample shadow maps.
    // cascadeCount:  sun cascades, 1 to k_maxShadowCascades. Replaced by
    //                GraphicsQuality::shadowCascades when rendering.
    // distance:      view distance the cascades cover, in metres. Beyond it the sun casts none.
    // splitLambda:   cascade split distribution, 0 uniform to 1 logarithmic.
    // depthBias:     slope-scaled depth bias rendered into the shadow map against acne, roughly in
    //                shadow texels of depth along the caster's slope. A change rebuilds the shadow
    //                materials (depth bias is pipeline state), so avoid changing it every frame.
    // normalBias:    offset of the shadow lookup along the surface normal against acne, in shadow
    //                texels.
    // debugCascades: tint the scene by the cascade each pixel reads.
    // maskedCasterCascades: how many of the nearest cascades alpha-masked casters (foliage) render
    //                into; farther cascades skip them, as their alpha-tested shadow draws are the
    //                most expensive part of the shadow pass on foliage-heavy levels.
    // skinnedCasterCascades: how many of the nearest cascades skinned casters (characters) render
    //                into; farther cascades skip them (each skinned shadow draw skins its mesh again,
    //                and a character's shadow in the farthest cascade is only a few texels).
    // localLights:   let point and spot lights with castsShadows cast shadows.
    // maxLocalLights: most shadow-casting local lights per frame, 0 to k_maxShadowedLocalLights.
    //                Replaced by GraphicsQuality::shadowedLocalLights when rendering.
    //                Lights whose range reaches into the view are chosen nearest first; one that does
    //                not fit (over this count, or out of atlas tiles) lights without a shadow.
    // localNormalBias: normal offset of local light shadow lookups against acne, in shadow texels.
    // localFilterRadius: PCF kernel radius of local light shadows in tile texels, 0.5 (hard) to 3
    //                (soft); costs the same at any value.
    struct ShadowSettings
    {
        bool enabled{true};
        uint32_t cascadeCount{k_maxShadowCascades};
        float distance{70.0f};
        float splitLambda{0.8f};
        float depthBias{1.0f};
        float normalBias{1.5f};
        bool debugCascades{false};
        uint32_t maskedCasterCascades{2};
        uint32_t skinnedCasterCascades{3};
        bool localLights{true};
        uint32_t maxLocalLights{4};
        float localNormalBias{2.0f};
        float localFilterRadius{1.5f};
    };

    // Exponential-squared distance fog, applied to lit materials.
    // enabled:   apply fog.
    // color:     linear RGB colour surfaces fade into.
    // density:   how quickly the fog thickens with distance, per metre.
    // start:     distance from the camera where fog begins, in metres.
    // maxAmount: upper bound of fog coverage in [0, 1], so far surfaces never vanish completely.
    // skyBlend:  0 fades into `color`; 1 fades into the sky's radiance at the horizon behind the
    //            surface (aerial perspective: distant buildings fade into the sunset glow towards the
    //            sun and into blue haze away from it). Needs the sky.
    struct FogSettings
    {
        bool enabled{false};
        math::Vec3 color{0.6f, 0.7f, 0.85f};
        float density{0.005f};
        float start{20.0f};
        float maxAmount{0.6f};
        float skyBlend{0.0f};
    };

    // Volumetric fog: haze in the air that the sun and point/spot lights scatter towards the camera,
    // shadowed by the shadow atlas, so light shafts through windows, spot cones and the glow around
    // lamps become visible. Raymarched per pixel at reduced resolution from the camera to the first
    // opaque surface (or maxDistance), with per-pixel jitter, reprojected over frames and upsampled
    // by depth, then composited into the HDR scene before bloom and tonemapping.
    // Density follows an exponential height profile: density * exp(-heightFalloff * (y - baseHeight)).
    // enabled:            compute and composite it.
    // density:            extinction per metre at baseHeight (0.01 thin haze, 0.05 thick smoke).
    // albedo:             linear RGB colour of the haze: the share of extinguished light scattered
    //                     (rather than absorbed) per channel; also tints the ambient term.
    // anisotropy:         Henyey-Greenstein g, -0.9 to 0.9: 0 scatters evenly, towards 1 mostly forward
    //                     (shafts bright when looking towards the light).
    // heightFalloff:      how quickly density drops with height, per metre; 0 is even everywhere.
    // baseHeight:         world height where `density` applies.
    // sunIntensity:       multiplier on light scattered from the sun.
    // localLightIntensity: multiplier on light scattered from point and spot lights (each light's own
    //                     `volumetric` factor applies on top).
    // ambientIntensity:   radiance of an even, unshadowed glow of the haze (light bounced everywhere),
    //                     so it reads in the dark too; 0 for none.
    // interiorDensityScale: density multiplier while the camera is inside an interior ambient zone
    //                     (fades over a couple of metres at the zone faces); 1 for none.
    // maxDistance:        metres along the view ray the fog is raymarched and absorbs; farther
    //                     surfaces are left to the distance fog.
    // steps:              raymarch samples for the sun, 8 to 64 (each point/spot light the ray passes
    //                     through gets half as many, within its range).
    // resolutionScale:    size of the raymarch relative to the window, 0.25 to 1. A change rebuilds
    //                     its attachments.
    // temporal:           blend each pixel with last frame's reprojected result (hides the jitter
    //                     noise; can ghost slightly behind fast-moving lights).
    // historyWeight:      share of last frame kept by `temporal`, 0 to 0.98.
    struct VolumetricFogSettings
    {
        bool enabled{false};
        float density{0.015f};
        math::Vec3 albedo{0.9f, 0.9f, 0.9f};
        float anisotropy{0.45f};
        float heightFalloff{0.0f};
        float baseHeight{0.0f};
        float sunIntensity{1.0f};
        float localLightIntensity{1.0f};
        float ambientIntensity{0.0f};
        float interiorDensityScale{1.0f};
        float maxDistance{40.0f};
        uint32_t steps{24};
        float resolutionScale{0.5f};
        bool temporal{true};
        float historyWeight{0.9f};
    };

    // Screen-space ambient occlusion (GTAO: horizon search in a few screen slices around each
    // pixel, integrated against its normal) at half resolution. Darkens only ambient
    // (environment) light, never direct light.
    // enabled:           compute and apply it.
    // radius:            world-space search radius in metres.
    // intensity:         strength of the darkening.
    // power:             contrast curve applied to the visibility.
    // slices:            screen directions searched per pixel, 1 to 4.
    // steps:             depth samples per direction and side, 2 to 12.
    // multiBounce:       brighten occlusion by light bounced between the occluding surfaces,
    //                    tinted by the surface colour (less dirty-looking corners on light walls).
    // multiBounceAlbedo: assumed surface reflectance for the multi-bounce, 0 to 1.
    // specularOcclusion: also occlude environment and screen-space reflections, by the visible
    //                    cone of the reflection lobe (reflections stop glowing in corners).
    // showOnly:          display the occlusion buffer instead of the scene.
    struct AmbientOcclusionSettings
    {
        bool enabled{true};
        float radius{0.6f};
        float intensity{1.0f};
        float power{1.3f};
        uint32_t slices{2};
        uint32_t steps{6};
        bool multiBounce{true};
        float multiBounceAlbedo{0.5f};
        bool specularOcclusion{true};
        bool showOnly{false};
    };

    // Physically based bloom: a mip chain of the HDR scene blended back with a small weight.
    // enabled:  compute and apply it.
    // strength: blend weight of the blurred image, 0 to 1.
    struct BloomSettings
    {
        bool enabled{true};
        float strength{0.04f};
    };

    // Curve that maps HDR scene colour to the displayable range.
    // Clamp: no curve, values above 1 clip. Reinhard: soft, low contrast. Aces: filmic, contrasty.
    enum class Tonemapper : uint8_t
    {
        Clamp = 0,
        Reinhard,
        Aces
    };

    // HDR to display conversion of the final image.
    // enabled:   apply exposure and the curve; when false the scene colour is shown as is (clipped).
    // tonemapper: the curve.
    // exposure:  linear multiplier applied before the curve (on top of auto exposure).
    // dither:    add sub-quantum noise against banding in dark gradients.
    struct TonemapSettings
    {
        bool enabled{true};
        Tonemapper tonemapper{Tonemapper::Aces};
        float exposure{1.0f};
        bool dither{true};
    };

    // Eye adaptation: the exposure follows the scene's average log luminance (centre-weighted),
    // measured on the GPU every frame, so dark interiors brighten and the street outside dims, with
    // a delay like the eye's. Needs tonemapping on.
    // enabled:        adapt; when off the exposure is TonemapSettings::exposure alone.
    // targetLuminance: average scene luminance the adaptation maps to mid-grey; lower is darker.
    // minEv, maxEv:   limits of the adapted exposure in stops (EV) relative to 1. Keep maxEv low so
    //                 dark places stay dark.
    // compensationEv: stops added after adaptation.
    // brightenSpeed:  adaptation rate towards a brighter exposure (eyes opening in the dark), per
    //                 second; higher is faster.
    // darkenSpeed:    adaptation rate towards a darker exposure (stepping into daylight), per second.
    // centreWeight:   0 meters the whole screen evenly, 1 mostly the centre.
    struct AutoExposureSettings
    {
        bool enabled{true};
        float targetLuminance{0.12f};
        float minEv{-2.0f};
        float maxEv{1.5f};
        float compensationEv{0.0f};
        float brightenSpeed{0.8f};
        float darkenSpeed{2.5f};
        float centreWeight{0.6f};
    };

    // Colour grading of the tonemapped image (display-referred, 0 to 1).
    // enabled:     apply it.
    // lift:        added to shadows (per channel, around 0; negative crushes blacks).
    // gamma:       power on midtones (per channel, around 1; above 1 brightens).
    // gain:        multiplier on highlights (per channel, around 1).
    // contrast:    S-curve strength around mid-grey, 1 neutral.
    // saturation:  1 neutral, 0 greyscale.
    // shadowTint:  linear RGB the shadows are pulled towards (split toning).
    // highlightTint: linear RGB the highlights are pulled towards.
    // splitToneStrength: amount of both tints, 0 off.
    // splitToneBalance: luminance where shadows end and highlights begin, 0 to 1.
    struct ColorGradingSettings
    {
        bool enabled{true};
        math::Vec3 lift{0.0f, 0.0f, 0.0f};
        math::Vec3 gamma{1.0f, 1.0f, 1.0f};
        math::Vec3 gain{1.0f, 1.0f, 1.0f};
        float contrast{1.0f};
        float saturation{1.0f};
        math::Vec3 shadowTint{0.45f, 0.6f, 0.75f};
        math::Vec3 highlightTint{0.85f, 0.7f, 0.5f};
        float splitToneStrength{0.0f};
        float splitToneBalance{0.45f};
    };

    // Darkening towards the screen corners, applied with grading.
    // enabled:    apply it.
    // intensity:  darkness at the corners, 0 to 1.
    // radius:     distance from the centre (0 centre, 1 corner) where darkening starts.
    // smoothness: width of the falloff.
    struct VignetteSettings
    {
        bool enabled{true};
        float intensity{0.0f};
        float radius{0.45f};
        float smoothness{0.6f};
    };

    // Animated film grain added to the final image (after anti-aliasing, so TAA does not smear it).
    // enabled:   apply it.
    // intensity: grain amplitude in the darks and mids.
    // response:  how much it fades out in highlights, 0 (even) to 1 (none in white).
    // size:      grain size in pixels, 1 to 3.
    struct FilmGrainSettings
    {
        bool enabled{true};
        float intensity{0.0f};
        float response{0.7f};
        float size{1.5f};
    };

    // How edges are anti-aliased.
    // Off:    nothing.
    // Fxaa:   FXAA on the tonemapped image (cheap, softens edges, cannot fix sub-pixel shimmer).
    // Msaa2x, Msaa4x: multisampled lit scene pass (geometry edges only, costs fill rate and
    //         memory), lowered to what the GPU supports.
    // Taa:    temporal anti-aliasing: the projection is jittered by a sub-pixel offset every frame
    //         and the tonemapped image is accumulated over frames, reprojected with per-pixel
    //         motion vectors, then sharpened. Also smooths shading and sub-pixel geometry.
    enum class AntiAliasingMode : uint8_t
    {
        Off = 0,
        Fxaa,
        Msaa2x,
        Msaa4x,
        Taa
    };

    // Anti-aliasing of the final image.
    // mode:            the technique; changing to or from an MSAA mode rebuilds the frame's
    //                  attachments. Replaced by GraphicsQuality::antiAliasing when rendering.
    // historyWeight:   TAA: share of the accumulated history kept each frame while the image is
    //                  still, 0 to 1; higher is smoother and slower to react.
    // motionWeight:    TAA: history share at fast motion (see motionPixels); lower reduces blur and
    //                  ghosting on movement.
    // motionPixels:    TAA: screen motion in pixels per frame at which motionWeight applies fully.
    // sharpness:       TAA: strength of the sharpening applied after accumulation, 0 to 1.
    // jitter:          TAA: jitter the projection (off shows the history alone, for debugging).
    struct AntiAliasingSettings
    {
        AntiAliasingMode mode{AntiAliasingMode::Taa};
        float historyWeight{0.9f};
        float motionWeight{0.8f};
        float motionPixels{24.0f};
        float sharpness{0.35f};
        bool jitter{true};
    };

    // Planar reflections (mirrors) of the reflectors Game submits: the nearest visible one is
    // rendered each frame from the mirrored camera into a reduced-resolution texture and
    // composited on its surface.
    // enabled:         render and composite them.
    // resolutionScale: size of the reflection texture relative to the window, 0.25 to 1. A change
    //                  rebuilds its attachments.
    // maxDistance:     reflectors farther from the camera than this, in metres, are not rendered
    //                  (their surface falls back to the environment reflection).
    struct PlanarReflectionSettings
    {
        bool enabled{true};
        float resolutionScale{0.5f};
        float maxDistance{8.0f};
    };

    // Magnified optics (Renderer::submitScope()): the second view rendered for the scope's
    // eyepiece.
    // enabled:     render scope views; off, the lens draw shows its plain glass material.
    // resolution:  side of the square scope view texture in texels, 256 to 1024: the most the view
    //              is drawn with. A change rebuilds its attachments.
    // texelsPerPixel: scope view texels per screen pixel across the magnified field; the view is
    //              drawn into a corner of the texture just big enough for that (capped by
    //              `resolution`), so a small lens on a small window costs less.
    // fog:         approximate the volumetric fog in the scope view (height fog transmittance and
    //              its ambient glow; no light shafts).
    // shadow:      draw the scope shadow (exit pupil and field stop); off shows the whole image.
    // eyeBoxScale: multiplier on ScopeView::exitPupil (larger is more forgiving).
    // holographic: draw holographic sight reticles (Renderer::submitSight()).
    // reticleBrightness: multiplier on HolographicSight::radiance.
    struct ScopeSettings
    {
        bool enabled{true};
        uint32_t resolution{512};
        float texelsPerPixel{1.0f};
        bool fog{true};
        bool shadow{true};
        float eyeBoxScale{1.0f};
        bool holographic{true};
        float reticleBrightness{1.0f};
    };

    // CPU-side visibility.
    // frustumCulling: skip draws whose bounds are outside the camera frustum (and, for shadows,
    //                 outside each cascade).
    // sunCasterCulling: with frustumCulling, also skip sun shadow casters whose shadow cannot fall
    //                 into the view (the view volume stretched towards the sun); off while a
    //                 planar reflection is rendered, as the mirror sees receivers outside the view.
    struct CullingSettings
    {
        bool frustumCulling{true};
        bool sunCasterCulling{true};
    };

    // Every renderer feature switch and tunable. Game sets its look once; the dev tools edit it live.
    struct RenderSettings
    {
        LightingSettings lighting{};
        SkySettings sky{};
        ImageBasedLightingSettings ibl{};
        InteriorAmbientSettings interiorAmbient{};
        ShadowSettings shadows{};
        FogSettings fog{};
        VolumetricFogSettings volumetricFog{};
        AmbientOcclusionSettings ambientOcclusion{};
        ContactShadowSettings contactShadows{};
        ScreenSpaceReflectionSettings screenSpaceReflections{};
        BloomSettings bloom{};
        TonemapSettings tonemap{};
        AutoExposureSettings autoExposure{};
        ColorGradingSettings colorGrading{};
        VignetteSettings vignette{};
        FilmGrainSettings filmGrain{};
        AntiAliasingSettings antiAliasing{};
        PlanarReflectionSettings reflections{};
        ScopeSettings scope{};
        CullingSettings culling{};
        SurfaceDetailSettings surfaceDetail{};
        MotionBlurSettings motionBlur{};
        ChromaticAberrationSettings chromaticAberration{};
        LensDirtSettings lensDirt{};
        DepthOfFieldSettings depthOfField{};
        WetnessSettings wetness{};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDER_SETTINGS_HPP
