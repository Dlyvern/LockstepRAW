#version 450
#extension GL_GOOGLE_include_directive : require

#include "scene_hdr.glsl"

layout(push_constant) uniform BloomConstants
{
    vec4 sourceTexel;
} constants;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

float karisWeight(vec3 color)
{
    return 1.0 / (1.0 + dot(color, vec3(0.2126, 0.7152, 0.0722)));
}

// Average of four taps, weighted against fireflies (Karis average), as (weighted colour, weight).
vec4 box(vec3 a, vec3 b, vec3 c, vec3 d, float coefficient)
{
    vec3 average = (a + b + c + d) * 0.25;
    float weight = karisWeight(average) * coefficient;

    return vec4(average * weight, weight);
}

vec3 tap(vec2 offset)
{
    return sceneHdr(inUv + offset * constants.sourceTexel.xy);
}

void main()
{
    vec3 a = tap(vec2(-2.0, -2.0));
    vec3 b = tap(vec2(0.0, -2.0));
    vec3 c = tap(vec2(2.0, -2.0));
    vec3 d = tap(vec2(-2.0, 0.0));
    vec3 e = tap(vec2(0.0, 0.0));
    vec3 f = tap(vec2(2.0, 0.0));
    vec3 g = tap(vec2(-2.0, 2.0));
    vec3 h = tap(vec2(0.0, 2.0));
    vec3 i = tap(vec2(2.0, 2.0));
    vec3 j = tap(vec2(-1.0, -1.0));
    vec3 k = tap(vec2(1.0, -1.0));
    vec3 l = tap(vec2(-1.0, 1.0));
    vec3 m = tap(vec2(1.0, 1.0));

    vec4 sum = box(j, k, l, m, 0.5);
    sum += box(a, b, d, e, 0.125);
    sum += box(b, c, e, f, 0.125);
    sum += box(d, e, g, h, 0.125);
    sum += box(e, f, h, i, 0.125);

    outColor = vec4(sum.rgb / max(sum.a, 1e-6), 1.0);
}
