#version 450

// Volumetric fog composite at full resolution, alpha-blended (src * a + dst * (1 - a)) over the lit
// scene's colour and ambient targets. The reduced-resolution fog is upsampled from its four
// nearest texels, each weighted by how close the view depth it was raymarched to is to this
// pixel's, so fog stays inside silhouettes. Colour gets in-scattered light over the transmitted
// scene; ambient (darkened by SSAO later) is only transmitted.

const float k_depthTolerance = 0.1;
const float k_minWeight = 1e-3;
const float k_minCoverage = 1e-3;

layout(push_constant) uniform CompositeConstants
{
    vec4 depthParams;
} constants;

layout(set = 2, binding = 0) uniform sampler2D fogTexture;
layout(set = 2, binding = 1) uniform sampler2D sceneDepth;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAmbient;

float viewDepth(float depth)
{
    return abs(constants.depthParams.y / (depth + constants.depthParams.x));
}

void main()
{
    ivec2 depthSize = textureSize(sceneDepth, 0);
    ivec2 fogSize = textureSize(fogTexture, 0);
    float pixelDepth = viewDepth(texelFetch(sceneDepth, ivec2(gl_FragCoord.xy), 0).r);
    vec2 fogPosition = inUv * vec2(fogSize) - 0.5;
    ivec2 base = ivec2(floor(fogPosition));
    vec2 fraction = fogPosition - vec2(base);
    vec4 sum = vec4(0.0);
    float weightSum = 0.0;

    for (int y = 0; y <= 1; ++y)
    {
        for (int x = 0; x <= 1; ++x)
        {
            ivec2 texel = clamp(base + ivec2(x, y), ivec2(0), fogSize - 1);
            ivec2 fullPixel = min(ivec2((vec2(texel) + 0.5) / vec2(fogSize) * vec2(depthSize)), depthSize - 1);
            float texelDepth = viewDepth(texelFetch(sceneDepth, fullPixel, 0).r);
            vec2 bilinear = mix(vec2(1.0) - fraction, fraction, vec2(x, y));
            float similarity = exp(-abs(texelDepth - pixelDepth) / (pixelDepth * k_depthTolerance));
            float weight = bilinear.x * bilinear.y * max(similarity, k_minWeight);
            sum += texelFetch(fogTexture, texel, 0) * weight;
            weightSum += weight;
        }
    }

    vec4 fog = sum / max(weightSum, 1e-6);
    float coverage = max(1.0 - fog.a, k_minCoverage);

    outColor = vec4(fog.rgb / coverage, coverage);
    outAmbient = vec4(0.0, 0.0, 0.0, coverage);
}
