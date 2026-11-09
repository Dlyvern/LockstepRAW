#version 450

layout(push_constant) uniform TextConstants
{
    vec2 pixelToClip;
    vec2 padding;
} textConstants;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;
layout(location = 3) in float inSoftness;

layout(location = 0) out vec2 outUv;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outSoftness;

vec3 srgbToLinear(vec3 srgb)
{
    vec3 low = srgb / 12.92;
    vec3 high = pow((srgb + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, lessThanEqual(srgb, vec3(0.04045)));
}

void main()
{
    outUv = inUv;
    outColor = vec4(srgbToLinear(inColor.rgb), inColor.a);
    outSoftness = inSoftness;
    gl_Position = vec4(inPosition * textConstants.pixelToClip - 1.0, 0.0, 1.0);
}
