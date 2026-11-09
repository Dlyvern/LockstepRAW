#version 450

const float k_edgeValue = 128.0 / 255.0;
const float k_screenPixelEdge = 0.7;

layout(set = 2, binding = 0) uniform sampler2D fontAtlas;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inSoftness;

layout(location = 0) out vec4 outColor;

void main()
{
    float distance = texture(fontAtlas, inUv).r;
    float halfWidth = max(fwidth(distance) * k_screenPixelEdge, inSoftness);
    float edge = k_edgeValue - inSoftness * 0.5;
    float coverage = smoothstep(edge - halfWidth, edge + halfWidth, distance);
    outColor = vec4(inColor.rgb, inColor.a * coverage);
}
