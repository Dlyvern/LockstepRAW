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

layout(push_constant) uniform TonemapConstants
{
    float exposure;
    uint tonemapper;
    float bloomStrength;
    uint view;
} constants;

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

void main()
{
    vec2 uv = (gl_FragCoord.xy) / vec2(textureSize(sceneColor, 0));

    if (constants.view == k_viewOcclusion)
    {
        outColor = vec4(vec3(textureLod(ambientOcclusion, uv, 0.0).r), 1.0);
        return;
    }

    vec3 color = sceneHdr(uv);

    if (constants.bloomStrength > 0.0)
    {
        color = mix(color, textureLod(bloom, uv, 0.0).rgb / k_bloomLevels, constants.bloomStrength);
    }

    color *= constants.exposure;

    if (constants.tonemapper == k_aces)
    {
        color = aces(color);
    }
    else if (constants.tonemapper == k_reinhard)
    {
        color = reinhard(color);
    }

    outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
