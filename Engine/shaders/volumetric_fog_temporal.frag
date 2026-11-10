#version 450

// Volumetric fog temporal blend: reprojects each texel's surface into last frame's resolved fog
// and blends it with this frame's jittered raymarch, clamped to the raymarch's 3x3 neighbourhood
// so moving lights and disocclusions do not ghost for long.

layout(push_constant) uniform TemporalConstants
{
    mat4 reprojection;
    vec4 params;
} constants;

layout(set = 2, binding = 0) uniform sampler2D currentFog;
layout(set = 2, binding = 1) uniform sampler2D sceneDepth;
layout(set = 2, binding = 2) uniform sampler2D historyFog;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outFog;

void main()
{
    ivec2 size = textureSize(currentFog, 0);
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 current = texelFetch(currentFog, pixel, 0);
    float weight = constants.params.x;

    if (weight <= 0.0)
    {
        outFog = current;
        return;
    }

    ivec2 depthSize = textureSize(sceneDepth, 0);
    ivec2 fullPixel = min(ivec2(inUv * vec2(depthSize)), depthSize - 1);
    float depth = texelFetch(sceneDepth, fullPixel, 0).r;
    vec2 uv = (vec2(fullPixel) + 0.5) / vec2(depthSize);
    vec4 previous = constants.reprojection * vec4(uv * 2.0 - 1.0, depth, 1.0);

    if (previous.w <= 0.0)
    {
        outFog = current;
        return;
    }

    vec2 previousUv = previous.xy / previous.w * 0.5 + 0.5;

    if (any(lessThan(previousUv, vec2(0.0))) || any(greaterThan(previousUv, vec2(1.0))))
    {
        outFog = current;
        return;
    }

    vec4 low = current;
    vec4 high = current;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec4 neighbour = texelFetch(currentFog, clamp(pixel + ivec2(x, y), ivec2(0), size - 1), 0);
            low = min(low, neighbour);
            high = max(high, neighbour);
        }
    }

    vec4 history = clamp(textureLod(historyFog, previousUv, 0.0), low, high);

    outFog = mix(current, history, weight);
}
