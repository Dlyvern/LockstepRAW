#version 450
#extension GL_GOOGLE_include_directive : require

// The output pass: FXAA 3.11 (quality preset 12) after T. Lottes on the tonemapped image, or
// sharpening of the TAA result, or a plain copy; then the lens effects (lens_effects.glsl), film
// grain (grain x intensity, y highlight response, z size in pixels), the screen fade and output
// dithering.

const float k_edgeThreshold = 0.125;
const float k_edgeThresholdMin = 0.0312;
const float k_subpixelQuality = 0.75;
const int k_searchSteps = 8;
const uint k_filterNone = 0u;
const uint k_filterFxaa = 1u;
const uint k_filterTemporalSharpen = 2u;
const float k_stepSizes[k_searchSteps] = float[](1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);

layout(push_constant) uniform OutputConstants
{
    vec4 fade;
    vec2 inverseSize;
    uint filterMode;
    float dither;
    uint frameIndex;
    float sharpness;
    vec2 padding;
    vec4 grain;
    vec4 motionBlur;
    vec4 depthOfField;
    vec4 lens;
    vec4 depthParams;
} constants;

layout(set = 2, binding = 0) uniform sampler2D source;
layout(set = 2, binding = 1) uniform sampler2D temporal;
layout(set = 2, binding = 2) uniform sampler2D depthTexture;
layout(set = 2, binding = 3) uniform sampler2D velocityTexture;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

float luma(vec3 color)
{
    return dot(sqrt(max(color, vec3(0.0))), vec3(0.299, 0.587, 0.114));
}

float lumaAt(vec2 uv)
{
    return luma(textureLod(source, uv, 0.0).rgb);
}

float lumaOffset(vec2 uv, vec2 offset)
{
    return lumaAt(uv + offset * constants.inverseSize);
}

vec3 fxaa(vec2 uv)
{
    vec2 rcpFrame = constants.inverseSize;
    vec3 colorM = textureLod(source, uv, 0.0).rgb;
    float lumaM = luma(colorM);
    float lumaS = lumaOffset(uv, vec2(0, 1));
    float lumaE = lumaOffset(uv, vec2(1, 0));
    float lumaN = lumaOffset(uv, vec2(0, -1));
    float lumaW = lumaOffset(uv, vec2(-1, 0));

    float maxLuma = max(max(max(lumaN, lumaW), max(lumaE, lumaS)), lumaM);
    float minLuma = min(min(min(lumaN, lumaW), min(lumaE, lumaS)), lumaM);
    float range = maxLuma - minLuma;

    if (range < max(k_edgeThresholdMin, maxLuma * k_edgeThreshold))
    {
        return colorM;
    }

    float lumaNW = lumaOffset(uv, vec2(-1, -1));
    float lumaSE = lumaOffset(uv, vec2(1, 1));
    float lumaNE = lumaOffset(uv, vec2(1, -1));
    float lumaSW = lumaOffset(uv, vec2(-1, 1));

    float lumaNS = lumaN + lumaS;
    float lumaWE = lumaW + lumaE;
    float subpixRcpRange = 1.0 / range;
    float subpixNSWE = lumaNS + lumaWE;
    float edgeHorz1 = -2.0 * lumaM + lumaNS;
    float edgeVert1 = -2.0 * lumaM + lumaWE;
    float lumaNESE = lumaNE + lumaSE;
    float lumaNWNE = lumaNW + lumaNE;
    float edgeHorz2 = -2.0 * lumaE + lumaNESE;
    float edgeVert2 = -2.0 * lumaN + lumaNWNE;
    float lumaNWSW = lumaNW + lumaSW;
    float lumaSWSE = lumaSW + lumaSE;
    float edgeHorz4 = abs(edgeHorz1) * 2.0 + abs(edgeHorz2);
    float edgeVert4 = abs(edgeVert1) * 2.0 + abs(edgeVert2);
    float edgeHorz3 = -2.0 * lumaW + lumaNWSW;
    float edgeVert3 = -2.0 * lumaS + lumaSWSE;
    float edgeHorz = abs(edgeHorz3) + edgeHorz4;
    float edgeVert = abs(edgeVert3) + edgeVert4;
    float subpixNWSWNESE = lumaNWSW + lumaNESE;

    float lengthSign = rcpFrame.x;
    bool horzSpan = edgeHorz >= edgeVert;
    float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;

    if (!horzSpan)
    {
        lumaN = lumaW;
        lumaS = lumaE;
    }
    else
    {
        lengthSign = rcpFrame.y;
    }

    float subpixB = subpixA * (1.0 / 12.0) - lumaM;
    float gradientN = lumaN - lumaM;
    float gradientS = lumaS - lumaM;
    float lumaNN = lumaN + lumaM;
    float lumaSS = lumaS + lumaM;
    bool pairN = abs(gradientN) >= abs(gradientS);
    float gradient = max(abs(gradientN), abs(gradientS));

    if (pairN)
    {
        lengthSign = -lengthSign;
    }

    float subpixC = clamp(abs(subpixB) * subpixRcpRange, 0.0, 1.0);

    vec2 posB = uv;
    vec2 offNP = horzSpan ? vec2(rcpFrame.x, 0.0) : vec2(0.0, rcpFrame.y);

    if (!horzSpan)
    {
        posB.x += lengthSign * 0.5;
    }
    else
    {
        posB.y += lengthSign * 0.5;
    }

    vec2 posN = posB - offNP * k_stepSizes[0];
    vec2 posP = posB + offNP * k_stepSizes[0];
    float subpixD = -2.0 * subpixC + 3.0;
    float lumaEndN = lumaAt(posN);
    float subpixE = subpixC * subpixC;
    float lumaEndP = lumaAt(posP);

    if (!pairN)
    {
        lumaNN = lumaSS;
    }

    float gradientScaled = gradient * 0.25;
    float lumaMM = lumaM - lumaNN * 0.5;
    float subpixF = subpixD * subpixE;
    bool lumaMLTZero = lumaMM < 0.0;

    lumaEndN -= lumaNN * 0.5;
    lumaEndP -= lumaNN * 0.5;
    bool doneN = abs(lumaEndN) >= gradientScaled;
    bool doneP = abs(lumaEndP) >= gradientScaled;

    for (int step = 1; step < k_searchSteps && !(doneN && doneP); ++step)
    {
        if (!doneN)
        {
            posN -= offNP * k_stepSizes[step];
            lumaEndN = lumaAt(posN) - lumaNN * 0.5;
            doneN = abs(lumaEndN) >= gradientScaled;
        }

        if (!doneP)
        {
            posP += offNP * k_stepSizes[step];
            lumaEndP = lumaAt(posP) - lumaNN * 0.5;
            doneP = abs(lumaEndP) >= gradientScaled;
        }
    }

    float dstN = horzSpan ? uv.x - posN.x : uv.y - posN.y;
    float dstP = horzSpan ? posP.x - uv.x : posP.y - uv.y;
    bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
    bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
    float spanLength = dstP + dstN;
    bool directionN = dstN < dstP;
    float dst = min(dstN, dstP);
    bool goodSpan = directionN ? goodSpanN : goodSpanP;
    float subpixG = subpixF * subpixF;
    float pixelOffset = dst * (-1.0 / spanLength) + 0.5;
    float subpixH = subpixG * k_subpixelQuality;
    float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;
    float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);

    vec2 finalUv = uv;

    if (!horzSpan)
    {
        finalUv.x += pixelOffsetSubpix * lengthSign;
    }
    else
    {
        finalUv.y += pixelOffsetSubpix * lengthSign;
    }

    return textureLod(source, finalUv, 0.0).rgb;
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

#include "lens_effects.glsl"

float hash(vec3 point)
{
    point = fract(point * vec3(0.1031, 0.1030, 0.0973));
    point += dot(point, point.yxz + 33.33);
    return fract((point.x + point.y) * point.z);
}

// Animated grain in sRGB units: two hashes per grain cell summed into a triangular distribution,
// fading out in highlights.
float filmGrain(vec2 fragment, vec3 color)
{
    vec2 cell = floor(fragment / max(constants.grain.z, 1.0));
    float seed = float(constants.frameIndex % 1024u) * 1.618;
    float noise = hash(vec3(cell, seed)) + hash(vec3(cell, seed + 17.0)) - 1.0;
    float luma = dot(sqrt(clamp(color, 0.0, 1.0)), vec3(0.2126, 0.7152, 0.0722));
    float strength = constants.grain.x * (1.0 - constants.grain.y * luma * luma);

    return noise * 2.0 * strength;
}

// Contrast-limited sharpening of the TAA result: a negative-lobe cross filter whose weight
// shrinks where the neighbourhood is already contrasty, clamped to the neighbourhood so it never
// rings.
vec3 sharpenedTemporal(ivec2 pixel)
{
    ivec2 last = textureSize(temporal, 0) - 1;
    vec3 center = texelFetch(temporal, pixel, 0).rgb;
    vec3 north = texelFetch(temporal, clamp(pixel + ivec2(0, -1), ivec2(0), last), 0).rgb;
    vec3 south = texelFetch(temporal, clamp(pixel + ivec2(0, 1), ivec2(0), last), 0).rgb;
    vec3 west = texelFetch(temporal, clamp(pixel + ivec2(-1, 0), ivec2(0), last), 0).rgb;
    vec3 east = texelFetch(temporal, clamp(pixel + ivec2(1, 0), ivec2(0), last), 0).rgb;

    vec3 low = min(center, min(min(north, south), min(west, east)));
    vec3 high = max(center, max(max(north, south), max(west, east)));
    vec3 headroom = min(low, 1.0 - high) / max(high, 1e-4);
    vec3 amplitude = sqrt(clamp(headroom, 0.0, 1.0));
    vec3 lobe = -amplitude * mix(0.0, 0.2, clamp(constants.sharpness, 0.0, 1.0));
    vec3 sharpened = (center + (north + south + west + east) * lobe) / (1.0 + 4.0 * lobe);

    return clamp(sharpened, low, high);
}

void main()
{
    vec2 uv = gl_FragCoord.xy * constants.inverseSize;
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec3 color;

    if (constants.filterMode == k_filterTemporalSharpen)
    {
        color = sharpenedTemporal(pixel);
    }
    else if (constants.filterMode == k_filterFxaa)
    {
        color = fxaa(uv);
    }
    else
    {
        color = texelFetch(source, pixel, 0).rgb;
    }

    color = lensEffects(color, uv);

    if (constants.grain.x > 0.0)
    {
        color = toLinear(clamp(toSrgb(color) + filmGrain(gl_FragCoord.xy, color), 0.0, 1.0));
    }

    color = mix(color, constants.fade.rgb, constants.fade.a);

    if (constants.dither > 0.0)
    {
        vec2 pixel = gl_FragCoord.xy + float(constants.frameIndex % 64u) * vec2(5.588238, 5.588238);
        float noise = interleavedGradientNoise(pixel) - 0.5;
        color = toLinear(clamp(toSrgb(color) + noise * constants.dither, 0.0, 1.0));
    }

    outColor = vec4(color, 1.0);
}
