#version 450

layout(push_constant) uniform BloomConstants
{
    vec4 sourceTexel;
} constants;

layout(set = 2, binding = 0) uniform sampler2D source;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

vec3 tap(vec2 offset)
{
    return textureLod(source, inUv + offset * constants.sourceTexel.xy, 0.0).rgb;
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

    vec3 color = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
    outColor = vec4(color, 1.0);
}
