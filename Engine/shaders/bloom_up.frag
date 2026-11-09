#version 450

layout(push_constant) uniform BloomConstants
{
    vec4 sourceTexel;
} constants;

layout(set = 2, binding = 0) uniform sampler2D lower;
layout(set = 2, binding = 1) uniform sampler2D current;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

vec3 tap(vec2 offset)
{
    return textureLod(lower, inUv + offset * constants.sourceTexel.xy, 0.0).rgb;
}

void main()
{
    vec3 tent = tap(vec2(0.0, 0.0)) * 4.0
        + (tap(vec2(-1.0, 0.0)) + tap(vec2(1.0, 0.0)) + tap(vec2(0.0, -1.0)) + tap(vec2(0.0, 1.0))) * 2.0
        + tap(vec2(-1.0, -1.0)) + tap(vec2(1.0, -1.0)) + tap(vec2(-1.0, 1.0)) + tap(vec2(1.0, 1.0));

    outColor = vec4(textureLod(current, inUv, 0.0).rgb + tent / 16.0, 1.0);
}
