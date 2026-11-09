#version 450

layout(push_constant) uniform BlurConstants
{
    vec4 projection;
    vec4 params;
} constants;

layout(set = 2, binding = 0) uniform sampler2D occlusion;
layout(set = 2, binding = 1) uniform sampler2D sceneDepth;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outOcclusion;

float viewDepthAt(ivec2 halfPixel)
{
    ivec2 fullPixel = min(halfPixel * 2, textureSize(sceneDepth, 0) - 1);
    return constants.projection.w / (texelFetch(sceneDepth, fullPixel, 0).r + constants.projection.z);
}

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 limit = textureSize(occlusion, 0) - 1;
    float centerDepth = viewDepthAt(pixel);
    float sum = 0.0;
    float weight = 0.0;

    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            ivec2 tap = clamp(pixel + ivec2(x, y), ivec2(0), limit);
            float w = max(0.0, 1.0 - abs(viewDepthAt(tap) - centerDepth) / (0.05 * centerDepth + 0.05));
            sum += texelFetch(occlusion, tap, 0).r * w;
            weight += w;
        }
    }

    outOcclusion = vec4(weight > 0.0 ? sum / weight : texelFetch(occlusion, pixel, 0).r);
}
