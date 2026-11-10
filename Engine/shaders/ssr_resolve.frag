#version 450
#extension GL_GOOGLE_include_directive : require

// Screen-space reflection resolve at half resolution: fetches last frame's lit scene where this
// pixel's ray (and, for rough surfaces, its neighbours' rays) hit, reprojected by the motion at
// the hit, then blends with the reflection accumulated so far, reprojected by this surface's
// motion. Output: rgb reflected radiance premultiplied by confidence, a confidence.

#include "view_reconstruct.glsl"

const int k_reuseTaps = 5;
const ivec2 k_reuseOffsets[k_reuseTaps] = ivec2[](ivec2(0, 0), ivec2(1, 1), ivec2(-1, 1), ivec2(1, -1),
    ivec2(-1, -1));
const float k_maxRadiance = 64.0;
const float k_motionPixelsForMinHistory = 16.0;

layout(push_constant) uniform ResolveConstants
{
    vec4 params;
} constants;

layout(set = 2, binding = 0) uniform sampler2D rayHits;
layout(set = 2, binding = 1) uniform sampler2D previousScene;
layout(set = 2, binding = 2) uniform sampler2D sceneVelocity;
layout(set = 2, binding = 3) uniform sampler2D previousReflection;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outReflection;

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 limit = textureSize(rayHits, 0) - 1;
    vec4 centre = texelFetch(rayHits, pixel, 0);
    float roughness = centre.w;
    int spacing = 1 + int(roughness * 6.0);
    int taps = roughness > 0.08 ? k_reuseTaps : 1;
    vec3 radiance = vec3(0.0);
    float confidence = 0.0;
    float weightSum = 0.0;

    for (int index = 0; index < taps; ++index)
    {
        vec4 hit = texelFetch(rayHits, clamp(pixel + k_reuseOffsets[index] * spacing, ivec2(0), limit), 0);
        float weight = exp(-abs(hit.w - roughness) * 12.0);
        weightSum += weight;

        if (hit.z <= 0.0)
        {
            continue;
        }

        vec2 previousUv = hit.xy - textureLod(sceneVelocity, hit.xy, 0.0).xy;

        if (any(lessThan(previousUv, vec2(0.0))) || any(greaterThan(previousUv, vec2(1.0))))
        {
            continue;
        }

        vec3 color = min(textureLod(previousScene, previousUv, 0.0).rgb, vec3(k_maxRadiance));
        float toneWeight = 1.0 / (1.0 + luminance(color));
        radiance += color * toneWeight * hit.z * weight;
        confidence += hit.z * weight;
    }

    vec4 current = vec4(0.0);

    if (confidence > 0.0)
    {
        vec3 average = radiance / confidence;
        average = average / max(1.0 - luminance(average), 1e-3);
        confidence /= max(weightSum, 1e-5);
        current = vec4(average * confidence, confidence);
    }

    float historyWeight = constants.params.x;

    if (historyWeight > 0.0)
    {
        vec2 motion = textureLod(sceneVelocity, inUv, 0.0).xy;
        vec2 previousUv = inUv - motion;
        bool offscreen = any(lessThan(previousUv, vec2(0.0))) || any(greaterThan(previousUv, vec2(1.0)));

        if (!offscreen)
        {
            vec4 history = textureLod(previousReflection, previousUv, 0.0);
            float motionPixels = length(motion * vec2(textureSize(sceneVelocity, 0)));
            float weight = historyWeight * mix(1.0, 0.5, clamp(motionPixels / k_motionPixelsForMinHistory, 0.0, 1.0));
            float historyLuminance = luminance(history.rgb);
            float currentLuminance = luminance(current.rgb);
            float allowed = max(currentLuminance * 4.0 + 0.05 * history.a, 0.02);

            if (current.a > 0.25 && historyLuminance > allowed)
            {
                history *= allowed / historyLuminance;
            }

            current = mix(current, history, weight);
        }
    }

    outReflection = current;
}
