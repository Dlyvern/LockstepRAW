#version 450

// TAA resolve: reprojects last frame's accumulated image with the dilated motion vector, rejects
// it where the surface was not visible last frame (view depth mismatch) and clips it to the
// current 3x3 neighbourhood (variance box in YCoCg), then blends it with the current, jittered
// image. Alpha carries this pixel's view depth for next frame's disocclusion test; 0 marks history
// that is invalid (Vela clears the attachment to 0 whenever it is re-created).

layout(push_constant) uniform TemporalConstants
{
    vec4 texel;
    vec4 depthParams;
    vec4 blend;
} constants;

layout(set = 2, binding = 0) uniform sampler2D currentColor;
layout(set = 2, binding = 1) uniform sampler2D velocityTexture;
layout(set = 2, binding = 2) uniform sampler2D depthTexture;
layout(set = 2, binding = 3) uniform sampler2D historyTexture;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

const float k_varianceGamma = 1.25;

vec3 toYcocg(vec3 rgb)
{
    return vec3(dot(rgb, vec3(0.25, 0.5, 0.25)), dot(rgb, vec3(0.5, 0.0, -0.5)), dot(rgb, vec3(-0.25, 0.5, -0.25)));
}

vec3 toRgb(vec3 ycocg)
{
    return vec3(ycocg.x + ycocg.y - ycocg.z, ycocg.x + ycocg.z, ycocg.x - ycocg.y - ycocg.z);
}

float linearDepth(float depth)
{
    return abs(constants.depthParams.y / (depth + constants.depthParams.x));
}

// Clips `history` towards the box centre until it is inside the box (Playdead / Salvi).
vec3 clipToBox(vec3 history, vec3 low, vec3 high)
{
    vec3 centre = 0.5 * (high + low);
    vec3 extent = 0.5 * (high - low) + 1e-5;
    vec3 offset = history - centre;
    vec3 units = abs(offset / extent);
    float largest = max(units.x, max(units.y, units.z));

    return largest > 1.0 ? centre + offset / largest : history;
}

// Bicubic Catmull-Rom history sample from five bilinear taps (the corner taps are dropped), which
// keeps the history sharp under sub-pixel motion.
vec3 sampleHistory(vec2 uv)
{
    vec2 size = constants.texel.zw;
    vec2 position = uv * size;
    vec2 centre = floor(position - 0.5) + 0.5;
    vec2 f = position - centre;
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);
    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / w12;
    vec2 uv0 = (centre - 1.0) * constants.texel.xy;
    vec2 uv3 = (centre + 2.0) * constants.texel.xy;
    vec2 uv12 = (centre + offset12) * constants.texel.xy;

    vec3 sum = textureLod(historyTexture, vec2(uv12.x, uv0.y), 0.0).rgb * (w12.x * w0.y)
        + textureLod(historyTexture, vec2(uv0.x, uv12.y), 0.0).rgb * (w0.x * w12.y)
        + textureLod(historyTexture, uv12, 0.0).rgb * (w12.x * w12.y)
        + textureLod(historyTexture, vec2(uv3.x, uv12.y), 0.0).rgb * (w3.x * w12.y)
        + textureLod(historyTexture, vec2(uv12.x, uv3.y), 0.0).rgb * (w12.x * w3.y);
    float weight = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;

    return max(sum / weight, vec3(0.0));
}

// True when any texel of the history's bilinear footprint saw the same surface last frame.
bool historyMatches(vec2 uv, float expectedDepth)
{
    vec4 depths = textureGather(historyTexture, uv, 3);
    vec4 tolerance = vec4(expectedDepth * constants.depthParams.z + 0.02);
    bvec4 valid = greaterThan(depths, vec4(0.0));
    bvec4 close = lessThan(abs(depths - expectedDepth), tolerance);

    return any(bvec4(valid.x && close.x, valid.y && close.y, valid.z && close.z, valid.w && close.w));
}

float lumaWeight(vec3 ycocg)
{
    return 1.0 / (1.0 + ycocg.x);
}

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 last = ivec2(constants.texel.zw) - 1;

    vec3 moment1 = vec3(0.0);
    vec3 moment2 = vec3(0.0);
    vec3 low = vec3(1e9);
    vec3 high = vec3(-1e9);
    vec3 current = vec3(0.0);
    float closestDepth = 2.0;
    ivec2 closestPixel = pixel;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            ivec2 tap = clamp(pixel + ivec2(x, y), ivec2(0), last);
            vec3 color = toYcocg(texelFetch(currentColor, tap, 0).rgb);
            float depth = texelFetch(depthTexture, tap, 0).r;

            moment1 += color;
            moment2 += color * color;
            low = min(low, color);
            high = max(high, color);

            if (x == 0 && y == 0)
            {
                current = color;
            }

            if (depth < closestDepth)
            {
                closestDepth = depth;
                closestPixel = tap;
            }
        }
    }

    float pixelDepth = linearDepth(texelFetch(depthTexture, pixel, 0).r);
    vec4 motion = texelFetch(velocityTexture, closestPixel, 0);
    vec2 previousUv = inUv - motion.xy;

    bool offscreen = any(lessThan(previousUv, vec2(0.0))) || any(greaterThan(previousUv, vec2(1.0)));
    bool disoccluded = motion.z > 0.0 ? !historyMatches(previousUv, motion.z)
        : textureLod(historyTexture, previousUv, 0.0).a <= 0.0;

    if (offscreen || disoccluded)
    {
        outColor = vec4(toRgb(current), pixelDepth);
        return;
    }

    vec3 mean = moment1 / 9.0;
    vec3 deviation = sqrt(max(moment2 / 9.0 - mean * mean, vec3(0.0)));
    vec3 boxLow = max(low, mean - deviation * k_varianceGamma);
    vec3 boxHigh = min(high, mean + deviation * k_varianceGamma);

    vec3 history = clipToBox(toYcocg(sampleHistory(previousUv)), boxLow, boxHigh);

    float motionPixels = length(motion.xy * constants.texel.zw);
    float historyWeight = mix(constants.blend.x, constants.blend.y,
        clamp(motionPixels / max(constants.blend.z, 1e-3), 0.0, 1.0));

    float currentWeight = (1.0 - historyWeight) * lumaWeight(current);
    float weightedHistory = historyWeight * lumaWeight(history);
    vec3 resolved = (current * currentWeight + history * weightedHistory) / max(currentWeight + weightedHistory, 1e-5);

    outColor = vec4(toRgb(resolved), pixelDepth);
}
