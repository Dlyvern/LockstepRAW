#version 450
#extension GL_GOOGLE_include_directive : require

// Volumetric fog raymarch at reduced resolution: light scattered towards the camera by height fog
// between the camera and the pixel's surface (capped at the max distance), from the sun (shadowed
// by its cascades) and every point/spot light whose range the ray crosses (shadowed by its atlas
// tiles), plus an even ambient glow. Transmittance is analytic for the exponential height profile.
// Output: rgb in-scattered radiance, a transmittance.

#define FRAME_LIGHTING_BINDING 0
#include "frame_lighting.glsl"

const float k_pi = 3.14159265359;
const float k_maxExponent = 30.0;
const float k_minLightDistanceSquared = 0.05;

layout(push_constant) uniform ScatterConstants
{
    mat4 inverseViewProjection;
    vec4 cameraDensity;
    vec4 heightDistance;
    vec4 albedoAnisotropy;
    vec4 intensities;
} fog;

layout(set = 2, binding = 0) uniform sampler2D sceneDepth;
layout(set = 2, binding = 1) uniform sampler2DShadow shadowAtlas;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outScatter;

float interleavedGradientNoise(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

float densityAt(float height)
{
    float exponent = -fog.heightDistance.x * (height - fog.heightDistance.y);
    return fog.cameraDensity.w * exp(clamp(exponent, -k_maxExponent, k_maxExponent));
}

// Optical depth from the camera to `along` metres along a ray whose direction has height `rayY`.
float opticalDepth(float cameraDensity, float rayY, float along)
{
    float rate = fog.heightDistance.x * rayY;

    if (abs(rate) < 1e-4)
    {
        return cameraDensity * along;
    }

    return cameraDensity * (1.0 - exp(clamp(-rate * along, -k_maxExponent, k_maxExponent))) / rate;
}

float henyeyGreenstein(float cosine)
{
    float g = fog.albedoAnisotropy.w;
    float denominator = 1.0 + g * g - 2.0 * g * cosine;

    return (1.0 - g * g) / (4.0 * k_pi * denominator * sqrt(denominator));
}

// One hardware-compared tap of a shadow atlas tile: the lit fraction, or -1 outside the tile.
float shadowTap(uint tile, vec3 position)
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

    return texture(shadowAtlas, vec3(uv, ndc.z));
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

float distanceFalloff(float lightDistance, float range)
{
    float ratio = lightDistance / range;
    float window = clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);

    return window * window / max(lightDistance * lightDistance, k_minLightDistanceSquared);
}

vec3 sunScatter(vec3 camera, vec3 direction, float rayLength, float jitter, float cameraDensity, uint steps)
{
    uint cascades = uint(frame.shadowParams.x);
    uint cascade = 0u;
    float sum = 0.0;

    for (uint index = 0u; index < steps; ++index)
    {
        float x = (float(index) + jitter) / float(steps);
        float along = rayLength * x * x;
        float stepLength = rayLength * 2.0 * x / float(steps);
        vec3 position = camera + direction * along;
        float lit = 1.0;

        while (cascade < cascades)
        {
            float tap = shadowTap(cascade, position);

            if (tap >= 0.0)
            {
                lit = tap;
                break;
            }

            ++cascade;
        }

        float transmittance = exp(-opticalDepth(cameraDensity, direction.y, along));
        sum += densityAt(position.y) * transmittance * lit * stepLength;
    }

    return sum * frame.sunRadiance.rgb * henyeyGreenstein(dot(frame.sunDirection.xyz, -direction));
}

vec3 localScatter(LocalLight light, vec3 camera, vec3 direction, float rayLength, float jitter, float cameraDensity,
    uint steps)
{
    vec3 center = light.positionRange.xyz;
    float range = light.positionRange.w;
    vec3 fromCenter = camera - center;
    float halfChord = dot(fromCenter, direction);
    float discriminant = halfChord * halfChord - (dot(fromCenter, fromCenter) - range * range);

    if (discriminant <= 0.0)
    {
        return vec3(0.0);
    }

    float root = sqrt(discriminant);
    float enter = max(-halfChord - root, 0.0);
    float leave = min(-halfChord + root, rayLength);

    if (leave <= enter)
    {
        return vec3(0.0);
    }

    bool isSpot = light.cosInnerKind.y == k_spotLight;
    bool hasShadow = light.cosInnerKind.z >= 0.0;
    float stepLength = (leave - enter) / float(steps);
    float sum = 0.0;

    for (uint index = 0u; index < steps; ++index)
    {
        float along = enter + (float(index) + jitter) * stepLength;
        vec3 position = camera + direction * along;
        vec3 offset = position - center;
        float lightDistance = length(offset);
        vec3 travel = offset / max(lightDistance, 1e-4);
        float attenuation = distanceFalloff(lightDistance, range);

        if (isSpot)
        {
            attenuation *= smoothstep(light.directionCosOuter.w, light.cosInnerKind.x,
                dot(travel, light.directionCosOuter.xyz));
        }

        if (attenuation <= 0.0)
        {
            continue;
        }

        if (hasShadow)
        {
            uint tile = uint(light.cosInnerKind.z) + (isSpot ? 0u : cubeFace(offset));
            float tap = shadowTap(tile, position);
            attenuation *= tap < 0.0 ? 1.0 : tap;
        }

        float transmittance = exp(-opticalDepth(cameraDensity, direction.y, along));
        sum += densityAt(position.y) * transmittance * attenuation * henyeyGreenstein(dot(travel, -direction)) * stepLength;
    }

    return sum * light.color.rgb * light.color.w;
}

void main()
{
    ivec2 depthSize = textureSize(sceneDepth, 0);
    ivec2 fullPixel = min(ivec2(inUv * vec2(depthSize)), depthSize - 1);
    float depth = texelFetch(sceneDepth, fullPixel, 0).r;
    vec2 uv = (vec2(fullPixel) + 0.5) / vec2(depthSize);
    vec4 world = fog.inverseViewProjection * vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec3 camera = fog.cameraDensity.xyz;
    vec3 toSurface = world.xyz / world.w - camera;
    float surfaceDistance = length(toSurface);
    vec3 direction = toSurface / max(surfaceDistance, 1e-4);
    float rayLength = min(surfaceDistance, fog.heightDistance.z);
    float jitter = fract(interleavedGradientNoise(vec2(fullPixel)) + fog.intensities.w);
    float cameraDensity = densityAt(camera.y);
    uint steps = uint(fog.heightDistance.w);
    float transmittance = exp(-opticalDepth(cameraDensity, direction.y, rayLength));
    vec3 scattered = vec3(0.0);

    if (frame.sunDirection.w > 0.0 && fog.intensities.x > 0.0)
    {
        scattered += sunScatter(camera, direction, rayLength, jitter, cameraDensity, steps) * fog.intensities.x;
    }

    if (fog.intensities.y > 0.0)
    {
        uint localSteps = max(steps / 2u, 4u);

        for (uint index = 0u; index < frame.counts.x; ++index)
        {
            LocalLight light = frame.lights[index];

            if (light.color.w > 0.0)
            {
                scattered += localScatter(light, camera, direction, rayLength, jitter, cameraDensity, localSteps)
                    * fog.intensities.y;
            }
        }
    }

    vec3 albedo = fog.albedoAnisotropy.rgb;
    vec3 inScattered = scattered * albedo * k_pi + albedo * fog.intensities.z * (1.0 - transmittance);

    outScatter = vec4(inScattered, transmittance);
}
