#version 450
#extension GL_GOOGLE_include_directive : require

// Volumetric clouds at reduced resolution: raymarches the cloud slab along each sky pixel's view
// ray (rgb in-scattered light, a transmittance). Pixels whose full-resolution footprint holds no
// sky are skipped. Composited over the sky by clouds_composite.frag.

#define FRAME_LIGHTING_BINDING 0
#include "frame_lighting.glsl"
#include "sky_model.glsl"

layout(set = 1, binding = 1) uniform sampler2D cloudNoise;

#include "clouds.glsl"

const float k_skyDepth = 1.0;
const float k_minTransmittance = 0.02;
const float k_maxSlabFactor = 5.0;
const float k_extinction = 0.0045;

layout(push_constant) uniform CloudConstants
{
    mat4 inverseViewProjection;
    vec4 cameraLod;
} constants;

layout(set = 2, binding = 0) uniform sampler2D sceneDepth;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outClouds;

float interleavedGradientNoise(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

float cloudDensity(vec3 position, float height, float lod, bool withDetail)
{
    vec2 uv = cloudUv(position.xz);
    float cover = cloudCover(broadNoise(textureLod(cloudNoise, uv, lod)));
    float top = cover * 0.95;
    float density = clamp((top - height) / 0.22, 0.0, 1.0) * smoothstep(0.0, 0.07, height) * cover;

    if (withDetail && density > 0.0)
    {
        vec4 fine = textureLod(cloudNoise, uv * 9.0 + vec2(height * 0.6, -height * 0.4), lod);
        float detail = fine.b * 0.7 + fine.a * 0.3;
        density = clamp(density - detail * 0.8 * (1.0 - density * 0.6), 0.0, 1.0);
    }

    return density * frame.cloudParams.y;
}

void main()
{
    vec4 depths = textureGather(sceneDepth, inUv, 0);

    if (max(max(depths.x, depths.y), max(depths.z, depths.w)) < k_skyDepth || frame.cloudShape.w < k_cloudModeVolumetric)
    {
        outClouds = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 farPoint = constants.inverseViewProjection * vec4(inUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 direction = normalize(farPoint.xyz / farPoint.w - constants.cameraLod.xyz);

    if (direction.y <= k_cloudMinElevation)
    {
        outClouds = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float base = frame.cloudParams.z;
    float thickness = frame.cloudParams.w;
    float start = base / direction.y;

    if (start > k_cloudFarDistance)
    {
        outClouds = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float end = min((base + thickness) / direction.y, start + thickness * k_maxSlabFactor);
    uint steps = uint(frame.cloudMarch.z);
    float stepLength = (end - start) / float(steps);
    float jitter = fract(interleavedGradientNoise(gl_FragCoord.xy) + frame.cloudMarch.w);
    vec3 toSun = -frame.skySunDirection.xyz;
    float cosAngle = dot(direction, toSun);
    vec3 sunLight = frame.skySunRadiance.rgb * frame.cloudMarch.x * cloudPhase(cosAngle);
    vec3 skyLight = cloudSkyLight();
    vec3 flash = lightningGlow(direction) * 2.0;
    float sunStep = thickness * 0.12 / max(toSun.y, 0.25);
    float lod = max(log2(start * constants.cameraLod.w), 0.0);

    vec3 light = vec3(0.0);
    float transmittance = 1.0;

    for (uint index = 0u; index < steps; ++index)
    {
        float distance = start + (float(index) + jitter) * stepLength;
        vec3 position = direction * distance;
        float height = (position.y - base) / thickness;
        float density = cloudDensity(position, height, lod, true);

        if (density <= 0.0)
        {
            continue;
        }

        float towardsSun = cloudDensity(position + toSun * sunStep, height + toSun.y * sunStep / thickness, lod + 1.0,
            false) + cloudDensity(position + toSun * sunStep * 3.0, height + toSun.y * sunStep * 3.0 / thickness,
            lod + 1.5, false);
        float sunTransmittance = exp(-towardsSun * sunStep * k_extinction * 2.0);
        float powder = 1.0 - exp(-density * 4.0);
        vec3 ambient = skyLight * mix(0.35, 1.0, clamp(height, 0.0, 1.0));
        vec3 scattered = sunLight * sunTransmittance * mix(1.0, powder, 0.5) + ambient + flash;

        float extinction = density * k_extinction;
        float stepTransmittance = exp(-extinction * stepLength);
        light += transmittance * scattered * (1.0 - stepTransmittance);
        transmittance *= stepTransmittance;

        if (transmittance < k_minTransmittance)
        {
            break;
        }
    }

    float fade = smoothstep(k_cloudFarDistance, k_cloudFarDistance * 0.35, start);
    outClouds = vec4(light * fade, mix(1.0, transmittance, fade));
}
