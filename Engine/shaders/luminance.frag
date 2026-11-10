#version 450
#extension GL_GOOGLE_include_directive : require

// Auto exposure, step 1: the scene's log luminance on a small fixed grid, weighted towards the
// screen centre. Writes (weight * log2 luminance, weight).

#include "scene_hdr.glsl"

const float k_minLuminance = 1e-4;

layout(push_constant) uniform LuminanceConstants
{
    vec4 params;
} constants;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

float luminanceAt(vec2 uv)
{
    return dot(sceneHdr(uv), vec3(0.2126, 0.7152, 0.0722));
}

void main()
{
    vec2 cell = 0.25 / constants.params.xy;
    float logSum = log2(max(luminanceAt(inUv + vec2(-cell.x, -cell.y)), k_minLuminance))
        + log2(max(luminanceAt(inUv + vec2(cell.x, -cell.y)), k_minLuminance))
        + log2(max(luminanceAt(inUv + vec2(-cell.x, cell.y)), k_minLuminance))
        + log2(max(luminanceAt(inUv + vec2(cell.x, cell.y)), k_minLuminance));
    float centreDistance = length((inUv - 0.5) * vec2(1.6, 1.0));
    float weight = mix(1.0, 1.0 - smoothstep(0.1, 0.8, centreDistance) * 0.95, constants.params.z);

    outColor = vec4(logSum * 0.25 * weight, weight, 0.0, 1.0);
}
