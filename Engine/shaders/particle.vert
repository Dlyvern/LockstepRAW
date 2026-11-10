#version 450
#extension GL_GOOGLE_include_directive : require

#define FRAME_LIGHTING_BINDING 0
#include "frame_lighting.glsl"
#include "octahedral.glsl"

// One camera-facing particle quad per instance (see GpuParticle in FrameParticles.hpp). Lighting
// is evaluated here, once per vertex of the quad, instead of per pixel.

const uint k_lit = 1u;
const uint k_additive = 2u;
const float k_minStreakMotion = 1e-4;
const float k_nearFadeStart = 0.05;
const float k_nearFadeLength = 0.35;
const float k_baseScattering = 0.6;
const float k_maxPhase = 4.0;

layout(set = 0, binding = 0) uniform Camera
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 position;
} camera;

layout(set = 2, binding = 1) uniform sampler2DShadow shadowAtlas;
layout(set = 2, binding = 2) uniform sampler2D environmentAtlas;

layout(location = 0) in vec4 inPositionSize;
layout(location = 1) in vec4 inVelocityStretch;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec4 inParams;
layout(location = 4) in vec4 inAtlas;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outUvs;
layout(location = 2) out vec2 outFrameBlendSoftness;
layout(location = 3) out float outViewDepth;

const vec2 k_corners[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0),
    vec2(1.0, 1.0), vec2(-1.0, 1.0));

// Lit fraction of a world position in one shadow atlas tile, single tap; -1 outside the tile.
float tileShadow(uint tile, vec3 position)
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

    if (any(lessThan(local, vec2(0.0))) || any(greaterThan(local, vec2(1.0))) || ndc.z >= 1.0)
    {
        return -1.0;
    }

    return textureLod(shadowAtlas, vec3(uv, ndc.z), 0.0);
}

float sunShadow(vec3 position)
{
    uint count = uint(frame.shadowParams.x);

    for (uint index = 0u; index < count; ++index)
    {
        float lit = tileShadow(index, position);

        if (lit >= 0.0)
        {
            return lit;
        }
    }

    return 1.0;
}

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

float localShadow(LocalLight light, vec3 position)
{
    uint tile = uint(light.cosInnerKind.z);

    if (light.cosInnerKind.y != k_spotLight)
    {
        tile += cubeFace(position - light.positionRange.xyz);
    }

    float lit = tileShadow(tile, position);

    return lit < 0.0 ? 1.0 : lit;
}

float interiorWeight(vec3 position)
{
    uint count = uint(frame.zoneParams.x);
    float weight = 0.0;

    for (uint index = 0u; index < count; ++index)
    {
        vec3 halfSize = (frame.zones[index].maximum.xyz - frame.zones[index].minimum.xyz) * 0.5;
        vec3 offset = abs(position - (frame.zones[index].maximum.xyz + frame.zones[index].minimum.xyz) * 0.5) - halfSize;
        float depth = -max(offset.x, max(offset.y, offset.z));
        weight = max(weight, smoothstep(0.0, frame.zoneParams.y, depth));
    }

    return weight;
}

// Henyey-Greenstein phase relative to isotropic scattering (1 when g is 0).
// cosine: cosine between the light's travel direction and the direction towards the camera.
float phase(float cosine, float g)
{
    float g2 = g * g;
    float denominator = max(1.0 + g2 - 2.0 * g * cosine, 1e-4);

    return min((1.0 - g2) / (denominator * sqrt(denominator)), k_maxPhase);
}

float distanceFalloff(float lightDistance, float range)
{
    float ratio = lightDistance / range;
    float window = clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);

    return window * window / max(lightDistance * lightDistance, 0.01);
}

vec3 particleLight(vec3 position, vec3 toCamera, float scattering)
{
    float interior = frame.zoneParams.x > 0.0 ? interiorWeight(position) : 0.0;
    vec3 light = frame.ambient.rgb;

    if (frame.iblParams.w > 0.0)
    {
        vec3 facing = textureLod(environmentAtlas, environmentAtlasUv(k_irradianceTile, toCamera), 0.0).rgb;
        vec3 above = textureLod(environmentAtlas, environmentAtlasUv(k_irradianceTile, vec3(0.0, 1.0, 0.0)), 0.0).rgb;
        light += (facing + above) * 0.5 * frame.iblParams.x * mix(1.0, frame.zoneLight.x, interior);
    }

    light += frame.zoneFill.rgb * interior;

    if (frame.sunDirection.w > 0.0)
    {
        float sunPhase = mix(k_baseScattering, phase(dot(frame.sunDirection.xyz, toCamera), scattering), sign(scattering));
        light += frame.sunRadiance.rgb * sunShadow(position) * sunPhase;
    }

    for (uint index = 0u; index < frame.counts.x; ++index)
    {
        LocalLight local = frame.lights[index];
        vec3 offset = local.positionRange.xyz - position;
        float lightDistance = length(offset);

        if (lightDistance >= local.positionRange.w)
        {
            continue;
        }

        vec3 toLight = offset / max(lightDistance, 1e-4);
        float attenuation = distanceFalloff(lightDistance, local.positionRange.w);

        if (local.cosInnerKind.y == k_spotLight)
        {
            attenuation *= smoothstep(local.directionCosOuter.w, local.cosInnerKind.x, dot(-toLight, local.directionCosOuter.xyz));
        }

        if (attenuation <= 0.0)
        {
            continue;
        }

        if (local.cosInnerKind.z >= 0.0)
        {
            attenuation *= localShadow(local, position);
        }

        float localPhase = mix(k_baseScattering, phase(dot(-toLight, toCamera), scattering), sign(scattering));
        light += local.color.rgb * attenuation * localPhase;
    }

    return light;
}

vec2 atlasUv(float frameIndex, vec2 cornerUv)
{
    float columns = max(inAtlas.x, 1.0);
    float rows = max(inAtlas.y, 1.0);
    vec2 cell = vec2(mod(frameIndex, columns), floor(frameIndex / columns));

    return (cell + cornerUv) / vec2(columns, rows);
}

void main()
{
    vec2 corner = k_corners[gl_VertexIndex];
    vec3 position = inPositionSize.xyz;
    float halfSize = inPositionSize.w * 0.5;
    vec3 viewCenter = (camera.view * vec4(position, 1.0)).xyz;
    vec3 viewOffset;

    vec3 viewMotion = mat3(camera.view) * inVelocityStretch.xyz * inVelocityStretch.w;
    float motionLength = length(viewMotion.xy);

    if (inVelocityStretch.w > 0.0 && motionLength > k_minStreakMotion)
    {
        vec2 along = viewMotion.xy / motionLength;
        vec2 across = vec2(-along.y, along.x);
        float halfLength = halfSize + motionLength * 0.5;
        viewCenter -= vec3(viewMotion.xy * 0.5, 0.0);
        viewOffset = vec3(along * corner.x * halfLength + across * corner.y * halfSize, 0.0);
    }
    else
    {
        float s = sin(inParams.x);
        float c = cos(inParams.x);
        viewOffset = vec3(vec2(c * corner.x - s * corner.y, s * corner.x + c * corner.y) * halfSize, 0.0);
    }

    gl_Position = camera.projection * vec4(viewCenter + viewOffset, 1.0);

    uint flags = uint(inParams.w + 0.5);
    vec3 color = inColor.rgb;
    vec3 toCamera = normalize(camera.position.xyz - position);
    float cameraDistance = length(camera.position.xyz - position);

    if ((flags & k_lit) != 0u)
    {
        color *= particleLight(position, toCamera, inAtlas.z);
    }

    if (frame.fogParams.y > 0.0)
    {
        float fogDistance = max(cameraDistance - frame.fogParams.x, 0.0) * frame.fogColor.w;
        float fog = min(1.0 - exp(-fogDistance * fogDistance), frame.fogParams.z);
        color = (flags & k_additive) != 0u ? color * (1.0 - fog) : mix(color, frame.fogColor.rgb, fog);
    }

    float viewDepth = -viewCenter.z;
    float nearFade = clamp((viewDepth - k_nearFadeStart - halfSize) / k_nearFadeLength, 0.0, 1.0);
    outColor = vec4(color, inColor.a * nearFade);

    vec2 cornerUv = vec2(corner.x * 0.5 + 0.5, 0.5 - corner.y * 0.5);
    float frameIndex = floor(inParams.y);
    float frameBlend = inParams.y - frameIndex;
    outUvs = vec4(atlasUv(frameIndex, cornerUv), atlasUv(frameIndex + (frameBlend > 0.0 ? 1.0 : 0.0), cornerUv));
    outFrameBlendSoftness = vec2(frameBlend, inParams.z);
    outViewDepth = viewDepth;
}
