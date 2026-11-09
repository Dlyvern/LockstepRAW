#version 450

// Must match TonemapPass.cpp.
const uint k_clamp = 0u;
const uint k_reinhard = 1u;
const uint k_aces = 2u;

layout(push_constant) uniform TonemapConstants
{
    float exposure;
    uint tonemapper;
    float dither;
    uint frameIndex;
} constants;

layout(set = 2, binding = 0) uniform sampler2D sceneColor;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

vec3 aces(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 reinhard(vec3 color)
{
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));

    return color / (1.0 + luminance);
}

float interleavedGradientNoise(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

vec3 toSrgb(vec3 linear)
{
    return mix(linear * 12.92, 1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), linear));
}

vec3 toLinear(vec3 srgb)
{
    return mix(srgb / 12.92, pow((srgb + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), srgb));
}

void main()
{
    vec3 color = texelFetch(sceneColor, ivec2(gl_FragCoord.xy), 0).rgb * constants.exposure;

    if (constants.tonemapper == k_aces)
    {
        color = aces(color);
    }
    else if (constants.tonemapper == k_reinhard)
    {
        color = reinhard(color);
    }

    color = clamp(color, 0.0, 1.0);

    if (constants.dither > 0.0)
    {
        vec2 pixel = gl_FragCoord.xy + float(constants.frameIndex % 64u) * vec2(5.588238, 5.588238);
        float noise = interleavedGradientNoise(pixel) - 0.5;
        color = toLinear(clamp(toSrgb(color) + noise * constants.dither, 0.0, 1.0));
    }

    outColor = vec4(color, 1.0);
}
