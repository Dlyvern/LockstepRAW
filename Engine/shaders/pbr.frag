#version 450
#extension GL_GOOGLE_include_directive : require

#define FRAME_LIGHTING_BINDING 5
#include "frame_lighting.glsl"
#include "octahedral.glsl"
#include "sky_model.glsl"
#include "surface_buffer.glsl"
#include "wetness.glsl"

const float k_pi = 3.14159265359;
const float k_minRoughness = 0.045;
const vec3 k_cascadeTints[4] = vec3[](vec3(1.0, 0.3, 0.3), vec3(0.3, 1.0, 0.3), vec3(0.3, 0.5, 1.0),
    vec3(1.0, 1.0, 0.3));

const float k_reflectionView = 1.0;
const float k_reflectorSurface = 2.0;
const float k_viewModelView = 3.0;
const float k_scopeView = 4.0;
const float k_scopeLensView = 5.0;
const float k_sightWindowView = 6.0;
const float k_reflectorAlignment = 0.995;
const vec3 k_zoneDebugTint = vec3(1.0, 0.1, 0.9);
const float k_decalBlend = 2.0;
const float k_minGlassCoverage = 0.02;
const uint k_softCascades = 2u;

layout(push_constant) uniform DrawConstants
{
    mat4 model;
    mat4 previousModel;
    vec4 baseColorFactor;
    vec4 emissiveNormalScale;
    vec4 surface;
    vec4 flags;
    vec4 detail;
    vec4 shading;
} draw;

layout(set = 0, binding = 0) uniform Camera
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 position;
} camera;

layout(set = 1, binding = 0) uniform sampler2D baseColorTexture;
layout(set = 1, binding = 1) uniform sampler2D normalTexture;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessTexture;
layout(set = 1, binding = 3) uniform sampler2D occlusionTexture;
layout(set = 1, binding = 4) uniform sampler2D emissiveTexture;

layout(set = 2, binding = 0) uniform sampler2DShadow shadowAtlas;
layout(set = 2, binding = 1) uniform sampler2D environmentAtlas;
layout(set = 2, binding = 2) uniform sampler2D shadowDepth;
layout(set = 1, binding = 21) uniform sampler2D brdfLut;
layout(set = 2, binding = 3) uniform sampler2D planarReflection;

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inCurrentClip;
layout(location = 5) in vec4 inPreviousClip;
layout(location = 6) in vec3 inObjectPosition;

#include "surface_detail.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAmbient;
layout(location = 2) out vec4 outVelocity;
layout(location = 3) out vec4 outSurfaceNormal;
layout(location = 4) out vec4 outSurfaceSpecular;

struct Surface
{
    vec3 normal;
    vec3 toCamera;
    vec3 diffuseColor;
    vec3 f0;
    float roughness;
    float nDotV;
};

// 3x3 bilinear PCF: nine hardware-filtered comparisons (each a 2x2 bilinear PCF), `spacing`
// texels apart; at spacing 1 the kernel covers 4x4 texels with tent weights.
float filteredShadow(vec2 uv, float depth, float spacing)
{
    vec2 texel = spacing / vec2(textureSize(shadowAtlas, 0));
    float sum = 0.0;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            sum += texture(shadowAtlas, vec3(uv + vec2(x, y) * texel, depth));
        }
    }

    return sum / 9.0;
}

// Looks a world position up in one tile of the shadow atlas. Returns -1 when it falls outside
// the tile (minus a margin that keeps the PCF footprint inside), else the lit fraction.
float tileShadow(uint tile, vec3 position, float spacing)
{
    vec4 clip = frame.shadowTileViewProjection[tile] * vec4(position, 1.0);

    if (clip.w <= 0.0)
    {
        return -1.0;
    }

    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    vec4 rect = frame.shadowTileRects[tile];
    vec2 local = (uv - rect.xy) / rect.zw;
    float margin = (spacing + 2.0) / (rect.z * float(textureSize(shadowAtlas, 0).x));

    if (any(lessThan(local, vec2(margin))) || any(greaterThan(local, vec2(1.0 - margin))) || ndc.z >= 1.0)
    {
        return -1.0;
    }

    return filteredShadow(uv, ndc.z, spacing);
}

#include "soft_shadows.glsl"

float sunShadow(vec3 position, vec3 geometryNormal, out int cascade)
{
    cascade = -1;
    uint count = uint(frame.shadowParams.x);

    for (uint index = 0u; index < count; ++index)
    {
        float texel = frame.cascadeTexelSize[index];
        vec3 biased = position + geometryNormal * (texel * frame.shadowParams.z);
        float lit = frame.shadowSoftness.x > 0.5 && index < k_softCascades
            ? tileShadowSoft(index, biased, frame.shadowSoftness.w)
            : tileShadow(index, biased, 1.0);

        if (lit >= 0.0)
        {
            cascade = int(index);
            return lit;
        }
    }

    return 1.0;
}

// Cube face (+X, -X, +Y, -Y, +Z, -Z) a direction from a point light falls in.
uint cubeFace(vec3 direction)
{
    vec3 magnitude = abs(direction);

    if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z)
    {
        return direction.x > 0.0 ? 0u : 1u;
    }

    if (magnitude.y >= magnitude.z)
    {
        return direction.y > 0.0 ? 2u : 3u;
    }

    return direction.z > 0.0 ? 4u : 5u;
}

float localShadow(LocalLight light, vec3 position, vec3 geometryNormal, float lightDistance)
{
    vec3 biased = position + geometryNormal * (light.cosInnerKind.w * lightDistance);
    uint tile = uint(light.cosInnerKind.z);

    if (light.cosInnerKind.y != k_spotLight)
    {
        tile += cubeFace(biased - light.positionRange.xyz);
    }

    float lit = frame.shadowSoftness.x > 0.5 ? tileShadowSoft(tile, biased,
            max(frame.localShadowSoftness.x, light.tube.w * 0.5))
        : tileShadow(tile, biased, frame.localShadowParams.x);

    return lit < 0.0 ? 1.0 : lit;
}

// How far inside the interior ambient zones a surface is: 0 outside every zone, 1 deeper than the
// blend distance inside one. Looked up off the surface along its normal, so a wall's outer face on
// a zone's border reads as outdoor and its inner face as indoor.
float interiorWeight(vec3 position, vec3 geometryNormal)
{
    uint count = uint(frame.zoneParams.x);
    vec3 probe = position + geometryNormal * frame.zoneParams.z;
    float weight = 0.0;

    for (uint index = 0u; index < count; ++index)
    {
        vec3 halfSize = (frame.zones[index].maximum.xyz - frame.zones[index].minimum.xyz) * 0.5;
        vec3 offset = abs(probe - (frame.zones[index].maximum.xyz + frame.zones[index].minimum.xyz) * 0.5) - halfSize;
        float depth = -max(offset.x, max(offset.y, offset.z));
        weight = max(weight, smoothstep(0.0, frame.zoneParams.y, depth));
    }

    return weight;
}

float distributionGgx(float nDotH, float alpha)
{
    float alpha2 = alpha * alpha;
    float denominator = nDotH * nDotH * (alpha2 - 1.0) + 1.0;

    return alpha2 / (k_pi * denominator * denominator);
}

float visibilitySmithGgxCorrelated(float nDotV, float nDotL, float alpha)
{
    float alpha2 = alpha * alpha;
    float lambdaV = nDotL * sqrt(nDotV * nDotV * (1.0 - alpha2) + alpha2);
    float lambdaL = nDotV * sqrt(nDotL * nDotL * (1.0 - alpha2) + alpha2);

    return 0.5 / max(lambdaV + lambdaL, 1e-5);
}

vec3 fresnelSchlick(vec3 f0, float cosine)
{
    float factor = pow(1.0 - cosine, 5.0);
    return f0 + (1.0 - f0) * factor;
}

#include "material_shading.glsl"
#include "surface_effects.glsl"

// Outgoing radiance from one light, added to `diffuse` and `specular`. Light colours are scaled so
// that a white Lambert surface facing a light of intensity 1 reflects exactly 1 (the pi of the
// Lambert BRDF is folded in).
void shadeLight(Surface surface, vec3 toLight, vec3 lightColor, inout vec3 diffuse, inout vec3 specular)
{
    float nDotL = dot(surface.normal, toLight);

    if ((g_shadingModel != 0.0 || g_isReliefDecal)
        && shadeMaterialLight(surface, toLight, lightColor, nDotL, diffuse, specular))
    {
        return;
    }

    if (nDotL <= 0.0)
    {
        return;
    }

    vec3 diffuseTerm = surface.diffuseColor;

    if (frame.counts.y != 0u)
    {
        vec3 halfVector = normalize(toLight + surface.toCamera);
        float nDotH = max(dot(surface.normal, halfVector), 0.0);
        float vDotH = max(dot(surface.toCamera, halfVector), 0.0);
        float alpha = surface.roughness * surface.roughness;
        vec3 fresnel = fresnelSchlick(surface.f0, vDotH);
        specular += k_pi * distributionGgx(nDotH, alpha) * visibilitySmithGgxCorrelated(surface.nDotV, nDotL, alpha)
            * fresnel * lightColor * nDotL;
        diffuseTerm *= 1.0 - fresnel;
    }

    diffuse += diffuseTerm * lightColor * nDotL;
}

#include "light_shapes.glsl"
#include "irradiance_volume.glsl"

float distanceFalloff(float lightDistance, float range)
{
    float ratio = lightDistance / range;
    float window = clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);

    return window * window / max(lightDistance * lightDistance, 0.01);
}

vec3 environmentSpecular(vec3 direction, float roughness)
{
    float level = roughness * (k_specularLevels - 1.0);
    float lower = floor(level);
    float upper = min(lower + 1.0, k_specularLevels - 1.0);
    vec3 a = textureLod(environmentAtlas, environmentAtlasUv(lower, direction), 0.0).rgb;
    vec3 b = textureLod(environmentAtlas, environmentAtlasUv(upper, direction), 0.0).rgb;

    return mix(a, b, level - lower);
}

vec3 perturbedNormal(vec3 normal, vec3 position)
{
    vec3 sampled = vec3(surfaceTexture(normalTexture).xy * 2.0 - 1.0, 0.0);
    sampled.z = sqrt(max(1.0 - dot(sampled.xy, sampled.xy), 0.0));
    sampled.xy = sampled.xy * draw.emissiveNormalScale.w + g_detailNormal;

    vec3 tangent;
    vec3 bitangent;

    if (dot(inTangent.xyz, inTangent.xyz) > 1e-6)
    {
        tangent = normalize(inTangent.xyz - normal * dot(normal, inTangent.xyz));
        bitangent = cross(normal, tangent) * (inTangent.w < 0.0 ? -1.0 : 1.0);
    }
    else
    {
        vec3 dpdx = dFdx(position);
        vec3 dpdy = dFdy(position);
        vec2 duvdx = dFdx(inUv);
        vec2 duvdy = dFdy(inUv);
        vec3 dpdyPerp = cross(dpdy, normal);
        vec3 dpdxPerp = cross(normal, dpdx);
        tangent = dpdyPerp * duvdx.x + dpdxPerp * duvdy.x;
        bitangent = dpdyPerp * duvdx.y + dpdxPerp * duvdy.y;
        float scale = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-12));
        tangent *= scale;
        bitangent *= scale;
    }

    return normalize(mat3(tangent, bitangent, normal) * sampled);
}

// The planar reflection seen at this surface point: the reflection view projects the point into
// its texture. Faces that are not parallel to the mirror plane (a frame's bevels) get none.
// Returns rgb radiance and a weight in [0, 1].
vec4 planarReflectionAt(vec3 position, vec3 normal)
{
    if (frame.reflectionCamera.w <= 0.0)
    {
        return vec4(0.0);
    }

    float alignment = dot(normal, frame.reflectionPlane.xyz);

    if (alignment < k_reflectorAlignment)
    {
        return vec4(0.0);
    }

    vec4 clip = frame.reflectionViewProjection * vec4(position, 1.0);
    vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
    vec3 radiance = textureLod(planarReflection, uv, 0.0).rgb * frame.reflectionTint.rgb;
    float weight = smoothstep(k_reflectorAlignment, 1.0, alignment);

    return vec4(radiance, weight);
}

// Screen-space motion (xy) and last frame's view depth (z) as TAA reads it back from the depth
// buffer. Viewmodel depth is squeezed, so its z is the camera depth its squeezed depth decodes to.
vec4 motionVector(bool isViewModel)
{
    vec2 current = inCurrentClip.xy / inCurrentClip.w;
    vec2 previous = inPreviousClip.xy / inPreviousClip.w;
    float previousDepth = max(inPreviousClip.w, 0.0);

    if (isViewModel && inPreviousClip.w > 0.0)
    {
        float squeezedDepth = inPreviousClip.z / inPreviousClip.w;
        previousDepth = abs(frame.sceneDepthParams.y / (squeezedDepth + frame.sceneDepthParams.x));
    }

    return vec4((current - previous) * 0.5, previousDepth, 0.0);
}

#include "scope_lens.glsl"

void main()
{
    if (draw.flags.z == k_scopeLensView)
    {
        shadeScopeLens();
        return;
    }

    bool isScopeView = draw.flags.z == k_scopeView;

    if (isScopeView && outsideScopeField(inWorldPosition))
    {
        discard;
    }

    bool isReflectionView = draw.flags.z == k_reflectionView || isScopeView;
    vec3 cameraPosition = isScopeView ? frame.scopeCamera.xyz
        : (draw.flags.z == k_reflectionView ? frame.reflectionCamera.xyz : camera.position.xyz);
    vec3 surfaceOffset = cameraPosition - inWorldPosition;
    float surfaceDistance = length(surfaceOffset);
    vec3 parallaxNormal = normalize(inNormal) * (draw.flags.x > 0.5 && !gl_FrontFacing ? -1.0 : 1.0);
    surfacePrepare(parallaxNormal, inWorldPosition, surfaceOffset / max(surfaceDistance, 1e-4), surfaceDistance,
        !isReflectionView);
    vec4 baseColor = surfaceTexture(baseColorTexture) * draw.baseColorFactor;
    baseColor.rgb *= g_detailAlbedo;
    bool isMasked = draw.surface.w >= 0.0;
    bool isBlended = draw.flags.y > 0.5;

    if (isMasked && baseColor.a < draw.surface.w)
    {
        discard;
    }

    float cameraDistance = length(cameraPosition - inWorldPosition);
    vec3 toCamera = (cameraPosition - inWorldPosition) / max(cameraDistance, 1e-4);
    vec3 geometryNormal = normalize(inNormal);

    if (draw.flags.x > 0.5 && !gl_FrontFacing)
    {
        geometryNormal = -geometryNormal;
    }

    vec4 metallicRoughness = surfaceTexture(metallicRoughnessTexture);
    float roughness = clamp(metallicRoughness.g * draw.surface.y + g_detailRoughness, k_minRoughness, 1.0);
    float metallic = clamp(metallicRoughness.b * draw.surface.x, 0.0, 1.0);
    float occlusion = 1.0 + draw.surface.z * (surfaceTexture(occlusionTexture).r - 1.0);
    vec3 emissive = screenEmission(surfaceTexture(emissiveTexture).rgb) * draw.emissiveNormalScale.rgb;
    float interior = frame.zoneParams.x > 0.0 ? interiorWeight(inWorldPosition, geometryNormal) : 0.0;
    vec3 shadingNormal = perturbedNormal(geometryNormal, inWorldPosition);
    bool isGlass = isBlended && draw.flags.y < k_decalBlend - 0.5;
    float wet = 0.0;
    float puddle = 0.0;

    if (floatBitsToUint(draw.shading.w) != 0u)
    {
        wet = applyCharacterSurface(inObjectPosition, 1.0 - interior, baseColor.rgb, roughness, metallic);
    }
    else if (frame.wetness.x > 0.0 && draw.flags.z != k_viewModelView && draw.flags.z != k_sightWindowView && !isGlass)
    {
        applyWetness(inWorldPosition, geometryNormal, 1.0 - interior, cameraDistance, baseColor.rgb, roughness,
            metallic, shadingNormal, wet, puddle);
    }

    if (draw.flags.z != k_viewModelView && draw.flags.z != k_sightWindowView && !isGlass)
    {
        applyDripPuddles(inWorldPosition, geometryNormal, baseColor.rgb, roughness, shadingNormal);
    }

    prepareDecalRelief(geometryNormal, shadingNormal, inWorldPosition);
    prepareMaterialShading(baseColor.rgb, metallic, geometryNormal, inWorldPosition);

    Surface surface;
    surface.normal = shadingNormal;
    surface.toCamera = toCamera;
    surface.diffuseColor = baseColor.rgb * (1.0 - metallic);
    surface.f0 = mix(vec3(0.04), baseColor.rgb, metallic);
    surface.roughness = roughness;
    surface.nDotV = max(dot(surface.normal, toCamera), 1e-4);

    vec3 direct = vec3(0.0);
    vec3 specular = vec3(0.0);
    int cascade = -1;

    vec3 sunLight = vec3(0.0);

    if (frame.sunDirection.w > 0.0)
    {
        vec3 toLight = -frame.sunDirection.xyz;
        float shadow = sunShadow(inWorldPosition, geometryNormal, cascade) * parallaxSelfShadow(toLight);
        vec3 sunSpecular = vec3(0.0);
        shadeLight(surface, toLight, frame.sunRadiance.rgb * shadow, sunLight, sunSpecular);
        direct += sunLight;
        specular += sunSpecular;
        sunLight += sunSpecular;
    }

    uint lightMask = floatBitsToUint(draw.detail.w) & ((1u << frame.counts.x) - 1u);

    if (!isReflectionView && draw.flags.z != k_viewModelView && draw.flags.z != k_sightWindowView)
    {
        uvec2 cell = min(uvec2(gl_FragCoord.xy * frame.lightCellScale.xy), uvec2(k_lightCellsX - 1u, k_lightCellsY - 1u));
        lightMask &= frame.lightCells[cell.y * k_lightCellsX + cell.x];
    }

    while (lightMask != 0u)
    {
        uint index = uint(findLSB(lightMask));
        lightMask &= lightMask - 1u;
        LocalLight light = frame.lights[index];
        vec3 offset = light.positionRange.xyz - inWorldPosition;
        float lightDistance = length(offset);

        bool isTube = light.tube.w > 0.0;

        if (lightDistance >= light.positionRange.w + light.tube.w)
        {
            continue;
        }

        vec3 toLight = offset / max(lightDistance, 1e-4);
        float attenuation = isTube ? 1.0 : distanceFalloff(lightDistance, light.positionRange.w);
        attenuation *= lightProfileFactor(light, -toLight);

        if (light.cosInnerKind.y == k_spotLight)
        {
            float cosAngle = dot(-toLight, light.directionCosOuter.xyz);
            attenuation *= smoothstep(light.directionCosOuter.w, light.cosInnerKind.x, cosAngle);
        }

        if (attenuation <= 0.0)
        {
            continue;
        }

        if (light.cosInnerKind.z >= 0.0)
        {
            attenuation *= localShadow(light, inWorldPosition, geometryNormal, lightDistance);
        }

        if (frame.counts.w != 0u)
        {
            attenuation *= parallaxSelfShadow(toLight);
        }

        if (isTube)
        {
            shadeTubeLight(surface, light, inWorldPosition, attenuation, direct, specular);
            continue;
        }

        shadeLight(surface, toLight, light.color.rgb * attenuation, direct, specular);
    }

    vec3 ambient = surface.diffuseColor * frame.ambient.rgb * occlusion;
    vec2 scaleBias = texture(brdfLut, vec2(surface.nDotV, roughness)).rg;
    vec3 specularColor = surface.f0 * scaleBias.x + scaleBias.y;
    float specularOcclusion = clamp(pow(surface.nDotV + occlusion, exp2(-16.0 * roughness - 1.0)) - 1.0 + occlusion,
        0.0, 1.0);
    vec3 ambientSpecular = vec3(0.0);
    IrradianceSample bounce = sampleIrradianceVolume(inWorldPosition, geometryNormal);
    vec3 reflectedView = reflect(-toCamera, surface.normal);

    if (frame.iblParams.w > 0.0)
    {
        vec3 irradiance = textureLod(environmentAtlas, environmentAtlasUv(k_irradianceTile, surface.normal), 0.0).rgb;
        vec3 reflected = reflectedView;
        float diffuseScale = frame.iblParams.x * mix(mix(1.0, frame.zoneLight.x, interior),
            irradianceSkyFactor(bounce, surface.normal), bounce.weight);
        float specularScale = frame.iblParams.y * mix(mix(1.0, frame.zoneLight.y, interior),
            irradianceSkyFactor(bounce, reflected), bounce.weight);

        ambient += surface.diffuseColor * (1.0 - specularColor) * irradiance * occlusion * diffuseScale;
        ambient += materialAmbient(surface, irradiance * occlusion * diffuseScale);

        if (frame.counts.y != 0u)
        {
            ambientSpecular += environmentSpecular(reflected, roughness) * specularColor * specularOcclusion
                * specularScale;
        }
    }

    if (bounce.weight > 0.0)
    {
        vec3 bounceLight = irradianceBounce(bounce, surface.normal) * bounce.weight;
        ambient += surface.diffuseColor * (1.0 - specularColor) * bounceLight * occlusion;

        if (frame.counts.y != 0u)
        {
            ambientSpecular += specularColor * irradianceBounce(bounce, reflectedView) * bounce.weight
                * specularOcclusion;
        }

        if (frame.irradianceParams.x > 1.5)
        {
            direct = vec3(0.0);
            specular = vec3(0.0);
            emissive = vec3(0.0);
            ambient = bounceLight;
            ambientSpecular = vec3(0.0);
        }
    }

    if (interior > 0.0 && frame.irradianceParams.x < 1.5)
    {
        vec3 fill = frame.zoneFill.rgb * interior * mix(1.0, frame.irradianceParams.w, bounce.weight);
        ambient += surface.diffuseColor * (1.0 - specularColor) * fill * occlusion;

        if (frame.counts.y != 0u)
        {
            ambientSpecular += specularColor * fill * specularOcclusion;
        }
    }

    vec3 color = direct + specular + emissive;
    vec3 reflections = specular + ambientSpecular;
    ambient += ambientSpecular;
    float specularWeight = frame.counts.y != 0u
        ? max(specularColor.r, max(specularColor.g, specularColor.b)) * specularOcclusion : 0.0;

    if (draw.flags.z == k_reflectorSurface)
    {
        vec4 planar = planarReflectionAt(inWorldPosition, surface.normal);

        if (planar.a > 0.0)
        {
            ambient *= 1.0 - planar.a * specularColor;
            ambientSpecular *= 1.0 - planar.a * specularColor;
            color += planar.rgb * specularColor * planar.a;
            specularWeight = 0.0;
        }
    }

    if (frame.shadowParams.w > 0.5 && cascade >= 0)
    {
        color *= k_cascadeTints[cascade];
        ambient *= k_cascadeTints[cascade];
    }

    if (frame.wetnessShading.w > 0.5)
    {
        color = wetnessDebugColor(color, wet, puddle);
        ambient = wetnessDebugColor(ambient, wet, puddle);
    }

    if (frame.zoneParams.w > 0.5)
    {
        color = mix(color, k_zoneDebugTint * (dot(color + ambient, vec3(0.333)) + 0.05), interior);
        ambient *= 1.0 - interior;
    }

    if (frame.fogParams.y > 0.0)
    {
        float fogDistance = max(cameraDistance - frame.fogParams.x, 0.0) * frame.fogColor.w;
        float fog = min(1.0 - exp(-fogDistance * fogDistance), frame.fogParams.z);
        vec3 fogColor = frame.fogColor.rgb;

        if (frame.fogParams.w > 0.0)
        {
            vec3 horizon = normalize(vec3(-toCamera.x, max(-toCamera.y, 0.02), -toCamera.z));
            fogColor = mix(fogColor, skyRadiance(horizon), frame.fogParams.w);
        }

        color = mix(color, fogColor, fog);
        ambient *= 1.0 - fog;
        reflections *= 1.0 - fog;
        ambientSpecular *= 1.0 - fog;
        sunLight *= 1.0 - fog;
        specularWeight *= 1.0 - fog;
    }

    bool isViewModel = draw.flags.z == k_viewModelView || draw.flags.z == k_sightWindowView;
    outVelocity = motionVector(isViewModel);
    float colorLuminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float sunShare = colorLuminance > 1e-5 ? dot(sunLight, vec3(0.2126, 0.7152, 0.0722)) / colorLuminance : 0.0;
    vec3 viewNormal = normalize(mat3(camera.view) * surface.normal);
    outSurfaceNormal = encodeSurfaceNormal(viewNormal, roughness, sunShare);
    outSurfaceSpecular = encodeSurfaceSpecular(ambientSpecular, specularWeight, metallic > 0.5);

    if (draw.flags.y > k_decalBlend - 0.5)
    {
        decalOutputs(color, ambient, ambientSpecular, baseColor.a, viewNormal, roughness);
        return;
    }

    if (isBlended)
    {
        // Glass: reflections are not faded by alpha, and Fresnel makes the surface more opaque at
        // grazing angles. Blending is src * a + dst * (1 - a), so the colour is pre-divided by the
        // coverage it gets: diffuse * alpha + reflections over (1 - alpha) * (1 - F) of what is behind.
        vec3 diffuse = max(color + ambient - reflections, vec3(0.0));
        float fresnel = clamp(dot(specularColor, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
        float coverage = max(1.0 - (1.0 - baseColor.a) * (1.0 - fresnel), k_minGlassCoverage);
        vec3 reticle = draw.flags.z == k_sightWindowView ? holographicReticle() : vec3(0.0);
        outColor = vec4((diffuse * baseColor.a + reflections + reticle) / coverage, coverage);
        outAmbient = vec4(0.0, 0.0, 0.0, coverage);
        outSurfaceNormal = vec4(0.0);
        outSurfaceSpecular = vec4(0.0, 0.0, 0.0, coverage);
        return;
    }

    if (isScopeView)
    {
        outColor = vec4(applyScopeFog(color + ambient, -toCamera.y, cameraDistance), 1.0);
        outAmbient = vec4(0.0, 0.0, 0.0, 1.0);
        outSurfaceNormal = k_neutralSurfaceNormal;
        outSurfaceSpecular = k_neutralSurfaceSpecular;
        return;
    }

    if (isReflectionView || isViewModel)
    {
        outColor = vec4(color + ambient, 1.0);
        outAmbient = vec4(0.0, 0.0, 0.0, 1.0);
        outSurfaceNormal = k_neutralSurfaceNormal;
        outSurfaceSpecular = k_neutralSurfaceSpecular;
        return;
    }

    outColor = vec4(color, 1.0);
    outAmbient = vec4(ambient, 1.0);
}
