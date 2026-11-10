#version 450
#extension GL_GOOGLE_include_directive : require

#include "scene_hdr.glsl"

// Must match TonemapConstants in PostPasses.cpp.
const uint k_clamp = 0u;
const uint k_reinhard = 1u;
const uint k_aces = 2u;
const uint k_viewScene = 0u;
const uint k_viewOcclusion = 1u;
const float k_bloomLevels = 5.0;

// Grading (display-referred, in sRGB-encoded space): lift.w 1 to grade, gamma.w contrast, gain.w
// saturation, shadowTint.w split-tone strength, highlightTint.w split-tone balance, vignette x
// intensity, y radius, z smoothness, w aspect ratio. dirt x: how strongly bloom lights the lens dirt
// (0 off).
layout(push_constant) uniform TonemapConstants
{
    float exposure;
    uint tonemapper;
    float bloomStrength;
    uint view;
    vec4 lift;
    vec4 gamma;
    vec4 gain;
    vec4 shadowTint;
    vec4 highlightTint;
    vec4 vignette;
    vec4 dirt;
} constants;

layout(set = 1, binding = 0) uniform sampler2D lensDirt;

// rgb bloom, a the exposure multiplier (auto exposure).
layout(set = 2, binding = 3) uniform sampler2D bloom;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

vec3 aces(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 reinhard(vec3 color)
{
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));

    return color / (1.0 + luminance);
}

vec3 toSrgb(vec3 linear)
{
    return mix(linear * 12.92, 1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), linear));
}

vec3 toLinear(vec3 srgb)
{
    return mix(srgb / 12.92, pow((srgb + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), srgb));
}

vec3 graded(vec3 linear)
{
    vec3 color = toSrgb(clamp(linear, 0.0, 1.0));
    color = constants.gain.rgb * (color + constants.lift.rgb * (1.0 - color));
    color = pow(max(color, vec3(0.0)), 1.0 / max(constants.gamma.rgb, vec3(0.01)));

    vec3 sigmoid = color * color * (3.0 - 2.0 * color);
    color = constants.gamma.w >= 1.0 ? mix(color, sigmoid, constants.gamma.w - 1.0)
        : mix(vec3(0.5), color, constants.gamma.w);

    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, constants.gain.w);

    float highlights = smoothstep(constants.highlightTint.w - 0.25, constants.highlightTint.w + 0.25, luma);
    vec3 tint = mix(constants.shadowTint.rgb, constants.highlightTint.rgb, highlights);
    vec3 toned = color * tint / max(dot(tint, vec3(0.2126, 0.7152, 0.0722)), 1e-3);
    color = mix(color, toned, constants.shadowTint.w);

    return toLinear(clamp(color, 0.0, 1.0));
}

float vignette(vec2 uv)
{
    vec2 offset = (uv - 0.5) * vec2(constants.vignette.w, 1.0) * 2.0 / sqrt(1.0 + constants.vignette.w
        * constants.vignette.w);
    float distanceFromCentre = length(offset) * 1.41421356;
    float falloff = smoothstep(constants.vignette.y, constants.vignette.y + constants.vignette.z, distanceFromCentre);

    return 1.0 - falloff * constants.vignette.x;
}

void main()
{
    vec2 uv = (gl_FragCoord.xy) / vec2(textureSize(sceneColor, 0));

    if (constants.view == k_viewOcclusion)
    {
        outColor = vec4(vec3(textureLod(ambientOcclusion, uv, 0.0).r), 1.0);
        return;
    }

    vec3 color = sceneHdr(uv);
    vec4 bloomExposure = textureLod(bloom, uv, 0.0);

    if (constants.bloomStrength > 0.0)
    {
        color = mix(color, bloomExposure.rgb / k_bloomLevels, constants.bloomStrength);
    }

    if (constants.dirt.x > 0.0)
    {
        color += bloomExposure.rgb / k_bloomLevels * textureLod(lensDirt, uv, 0.0).rgb * constants.dirt.x;
    }

    color *= constants.exposure * bloomExposure.a;

    if (constants.tonemapper == k_aces)
    {
        color = aces(color);
    }
    else if (constants.tonemapper == k_reinhard)
    {
        color = reinhard(color);
    }

    color = clamp(color, 0.0, 1.0);

    if (constants.lift.w > 0.5)
    {
        color = graded(color);
    }

    if (constants.vignette.x > 0.0)
    {
        color *= vignette(uv);
    }

    outColor = vec4(color, 1.0);
}
