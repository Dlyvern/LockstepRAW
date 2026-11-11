// Animated surfaces of the lit pass (SurfaceEffects.hpp): glowing screens (ShadingModel::Screen)
// and drip puddles with ripple rings. Include in pbr.frag after the material textures, the draw
// constants, the frame block and surface_detail.glsl.
//
// frame.surfaceEffects: x animation seconds, y drip puddles, z scanline depth (0: screens static),
//                       w ripple slope.
// frame.dripPuddles:    per puddle: xyz centre, w radius; x seconds since the last drop, y seconds
//                       between drops, z strength.
// draw.shading (screen): x 3 (4: with the procedural terminal image), y effect strength, z scroll in
//                        UV units per second.

const float k_screenShading = 3.0;
const float k_terminalShading = 4.0;
const vec2 k_terminalCells = vec2(46.0, 22.0);
const float k_scanlinesPerUv = 150.0;
const float k_rollingBandSpeed = 0.13;
const float k_rippleSpeed = 0.32;
const float k_rippleWavelength = 0.035;
const float k_rippleLifetime = 1.6;
const float k_dripPuddleHeightReach = 0.3;
const float k_dripPuddleRoughness = 0.04;
const float k_dripPuddleDarkening = 0.55;
const int k_ripplesPerPuddle = 3;

float effectHash(vec2 point)
{
    return fract(sin(dot(point, vec2(127.1, 311.7))) * 43758.5453);
}

float effectNoise(vec2 point)
{
    vec2 cell = floor(point);
    vec2 f = fract(point);
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(effectHash(cell), effectHash(cell + vec2(1.0, 0.0)), u.x),
        mix(effectHash(cell + vec2(0.0, 1.0)), effectHash(cell + vec2(1.0, 1.0)), u.x), u.y);
}

// A dim terminal: dark blue-grey background, a lighter title bar, ragged rows of text blocks and a
// blinking cursor after the last row; times the texel's brightness.
vec3 terminalImage(vec2 uv, float time)
{
    vec2 cell = floor(fract(uv) * k_terminalCells);
    vec2 inCell = fract(fract(uv) * k_terminalCells);
    float row = cell.y;
    float lineLength = floor(effectHash(vec2(row, 3.1)) * k_terminalCells.x * 0.85);
    float isText = step(cell.x, lineLength) * step(0.3, effectHash(cell + 0.7)) * step(1.0, row)
        * step(row, k_terminalCells.y - 3.0);
    float glyph = isText * step(0.15, inCell.x) * step(inCell.x, 0.85) * step(0.2, inCell.y) * step(inCell.y, 0.8);
    float cursorRow = k_terminalCells.y - 2.0;
    float cursor = step(abs(row - cursorRow), 0.5) * step(abs(cell.x - 2.0), 0.5) * step(0.5, fract(time * 1.1));
    float titleBar = step(row, 0.5);
    vec3 color = vec3(0.03, 0.04, 0.055);
    color = mix(color, vec3(0.1, 0.14, 0.2), titleBar);
    color = mix(color, vec3(0.2, 0.27, 0.31), max(glyph, cursor));

    return color;
}

// The emissive texel of a screen material with its animation; other materials keep `sampled`.
// sampled: the emissive texture at this pixel (before the material's emissive factor).
vec3 screenEmission(vec3 sampled)
{
    bool isTerminal = draw.shading.x == k_terminalShading;

    if ((draw.shading.x != k_screenShading && !isTerminal) || draw.shading.y <= 0.0)
    {
        return sampled;
    }

    float time = frame.surfaceEffects.x;
    vec2 uv = g_surfaceUv;

    if (draw.shading.z != 0.0)
    {
        uv.y += time * draw.shading.z;
        sampled = textureGrad(emissiveTexture, uv, g_surfaceUvDx, g_surfaceUvDy).rgb;
    }

    if (isTerminal)
    {
        sampled = terminalImage(uv, time) * max(max(sampled.r, sampled.g), sampled.b);
    }

    float depth = frame.surfaceEffects.z * min(draw.shading.y, 2.0);

    if (depth <= 0.0)
    {
        return sampled;
    }

    float lineFade = 1.0 - clamp(length(fwidth(g_surfaceUv)) * k_scanlinesPerUv, 0.0, 1.0);
    float lines = 1.0 - depth * 0.5 * lineFade * (0.5 + 0.5 * sin(g_surfaceUv.y * k_scanlinesPerUv * 6.2831853));
    float band = fract(g_surfaceUv.y - time * k_rollingBandSpeed);
    float rolling = 1.0 + depth * 0.6 * smoothstep(0.0, 0.08, band) * (1.0 - smoothstep(0.08, 0.2, band));
    float shimmer = 1.0 + depth * 0.08 * (effectHash(vec2(floor(time * 24.0), 0.37)) - 0.5);

    return sampled * lines * rolling * shimmer;
}

// Darkens and smooths the surface inside the submitted drip puddles and bends its normal with the
// rings of the latest drops. Only upward-facing surfaces near a puddle's height take part.
// position:       world position.
// geometryNormal: interpolated normal facing the drawn side.
void applyDripPuddles(vec3 position, vec3 geometryNormal, inout vec3 baseColor, inout float roughness,
    inout vec3 shadingNormal)
{
    uint count = min(uint(frame.surfaceEffects.y), k_maxDripPuddles);

    if (count == 0u || geometryNormal.y < 0.7)
    {
        return;
    }

    for (uint index = 0u; index < count; ++index)
    {
        vec4 placement = frame.dripPuddles[index * 2u];
        vec4 drops = frame.dripPuddles[index * 2u + 1u];
        vec2 offset = position.xz - placement.xz;
        float distanceFromCentre = length(offset);
        float radius = placement.w;

        if (distanceFromCentre > radius * 1.4 || abs(position.y - placement.y) > k_dripPuddleHeightReach)
        {
            continue;
        }

        float ragged = (effectNoise(position.xz * 9.0) - 0.5) * 0.5 + (effectNoise(position.xz * 23.0) - 0.5) * 0.2;
        float water = (1.0 - smoothstep(0.75, 1.0, distanceFromCentre / radius + ragged)) * drops.z;

        if (water <= 0.0)
        {
            continue;
        }

        baseColor *= mix(1.0, k_dripPuddleDarkening, water);
        roughness = mix(roughness, k_dripPuddleRoughness, water);

        vec2 slope = vec2(0.0);
        vec2 outward = distanceFromCentre > 1e-4 ? offset / distanceFromCentre : vec2(0.0);

        for (int ring = 0; ring < k_ripplesPerPuddle; ++ring)
        {
            float age = drops.x + float(ring) * drops.y;

            if (age > k_rippleLifetime)
            {
                break;
            }

            float front = distanceFromCentre - age * k_rippleSpeed;
            float envelope = exp(-front * front / (k_rippleWavelength * k_rippleWavelength * 2.0));
            float fade = (1.0 - age / k_rippleLifetime) / (1.0 + age * 4.0);
            slope += outward * cos(front / k_rippleWavelength * 6.2831853) * envelope * fade;
        }

        vec3 still = normalize(mix(shadingNormal, vec3(0.0, 1.0, 0.0), water));
        shadingNormal = normalize(still - vec3(slope.x, 0.0, slope.y) * frame.surfaceEffects.w * water);
    }
}
