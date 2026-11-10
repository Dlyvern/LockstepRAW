#version 450

const float k_edgeValue = 128.0 / 255.0;
const float k_screenPixelEdge = 0.7;
const float k_shadeSolid = 1.0;
const float k_shadeImage = 2.0;

layout(set = 2, binding = 0) uniform sampler2D overlayTexture;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inSoftness;
layout(location = 3) in float inShade;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 sampled = texture(overlayTexture, inUv);

    if (inShade > k_shadeImage - 0.5)
    {
        outColor = inColor * sampled;
        return;
    }

    if (inShade > k_shadeSolid - 0.5)
    {
        outColor = inColor;
        return;
    }

    float distance = sampled.r;
    float halfWidth = max(fwidth(distance) * k_screenPixelEdge, inSoftness);
    float edge = k_edgeValue - inSoftness * 0.5;
    float coverage = smoothstep(edge - halfWidth, edge + halfWidth, distance);
    outColor = vec4(inColor.rgb, inColor.a * coverage);
}
