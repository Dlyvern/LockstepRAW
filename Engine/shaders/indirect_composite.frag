#version 450
#extension GL_GOOGLE_include_directive : require

// Full-resolution composite of the indirect light, between the lit scene and the fog.
// Rebuilds the ambient target from the lit pass's ambientLit:
//   diffuse ambient times multi-bounce ambient occlusion
// + environment specular blended towards screen-space reflections by their confidence, times
//   specular occlusion,
// all divided by the diffuse visibility, because every later reader multiplies ambient by it
// (shaders/scene_hdr.glsl). The colour target (direct light) is darkened by the contact shadow
// multiplier through alpha blending with black.

#include "surface_buffer.glsl"

const float k_minVisibility = 0.05;

layout(push_constant) uniform CompositeConstants
{
    vec4 params;
} constants;

layout(set = 2, binding = 0) uniform sampler2D ambientLit;
layout(set = 2, binding = 1) uniform sampler2D surfaceSpecular;
layout(set = 2, binding = 2) uniform sampler2D occlusion;
layout(set = 2, binding = 3) uniform sampler2D reflections;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outAmbient;
layout(location = 1) out vec4 outColor;

float maxComponent(vec3 value)
{
    return max(value.r, max(value.g, value.b));
}

// Jimenez et al. 2016: visibility brightened by interreflections for a given albedo.
vec3 multiBounce(float visibility, vec3 albedo)
{
    vec3 a = 2.0404 * albedo - 0.3324;
    vec3 b = -4.7951 * albedo + 0.6417;
    vec3 c = 2.7552 * albedo + 0.6903;

    return max(vec3(visibility), ((visibility * a + b) * visibility + c) * visibility);
}

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec3 ambient = texelFetch(ambientLit, pixel, 0).rgb;
    vec4 specularData = texelFetch(surfaceSpecular, pixel, 0);
    vec4 occluded = textureLod(occlusion, inUv, 0.0);
    vec4 reflection = textureLod(reflections, inUv, 0.0);
    float ssrIntensity = constants.params.x;
    float multiBounceAlbedo = constants.params.y;
    bool debugReflections = constants.params.z > 0.5;

    vec3 environmentSpecular = specularData.rgb;
    float signedWeight = decodeSpecularWeight(specularData.a);
    float specularWeight = abs(signedWeight);
    float visibility = max(occluded.r, k_minVisibility);
    vec3 diffuse = max(ambient - environmentSpecular, vec3(0.0));

    if (multiBounceAlbedo > 0.0)
    {
        vec3 albedoTint = diffuse / max(maxComponent(diffuse), 1e-6);
        diffuse *= multiBounce(occluded.r, multiBounceAlbedo * albedoTint);
    }
    else
    {
        diffuse *= occluded.r;
    }

    vec3 metalTint = vec3(1.0);

    if (signedWeight < 0.0 && maxComponent(environmentSpecular) > 1e-5)
    {
        metalTint = environmentSpecular / maxComponent(environmentSpecular);
    }

    vec3 screenSpace = specularWeight * metalTint * reflection.rgb * ssrIntensity;
    float confidence = clamp(reflection.a * ssrIntensity, 0.0, 1.0);
    vec3 specular = (environmentSpecular * (1.0 - confidence) + screenSpace) * occluded.g;

    if (debugReflections)
    {
        outAmbient = vec4(reflection.rgb * ssrIntensity / visibility, 1.0);
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    outAmbient = vec4((diffuse + specular) / visibility, 1.0);
    outColor = vec4(0.0, 0.0, 0.0, 1.0 - occluded.b);
}
