// Rain-wet surfaces for pbr.frag. Include after frame_lighting.glsl.
//
// frame.wetness:        x amount (0 off), y puddle coverage, z puddle noise frequency per metre,
//                       w ripple animation seconds.
// frame.wetnessShading: x porous albedo multiplier, y share of roughness the water film removes,
//                       z ripple strength, w 1 for the debug tint.

const float k_puddleRoughness = 0.035;
const float k_puddleDarkening = 0.75;
const float k_rippleCell = 0.24;
const float k_rippleRate = 1.4;
const float k_rippleMaxRadius = 0.3;
const float k_rippleWidth = 0.07;
const float k_rippleWaves = 55.0;
const float k_rippleSlope = 0.35;
const float k_rippleFadeStart = 3.0;
const float k_rippleFadeEnd = 12.0;
const vec3 k_wetDebugTint = vec3(0.1, 0.3, 1.0);
const vec3 k_puddleDebugTint = vec3(0.0, 1.0, 0.9);

float wetHash(vec2 point)
{
    vec3 p = fract(vec3(point.xyx) * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float wetNoise(vec2 point)
{
    vec2 cell = floor(point);
    vec2 f = fract(point);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = wetHash(cell);
    float b = wetHash(cell + vec2(1.0, 0.0));
    float c = wetHash(cell + vec2(0.0, 1.0));
    float d = wetHash(cell + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// Slope (d height / d xz, in cell units) of one layer of rain ripples: each cell of the grid
// restarts a ring at a random spot k_rippleRate times a second; a share of the cells stays still
// in light rain.
vec2 rippleLayer(vec2 xz, float time, vec2 offset, float strength)
{
    vec2 point = xz / k_rippleCell + offset;
    vec2 cell = floor(point);
    float cycle = time * k_rippleRate + wetHash(cell + offset * 7.0);
    float index = floor(cycle);
    float age = cycle - index;

    if (wetHash(cell + vec2(index * 5.3, 1.7)) > 0.35 + 0.65 * strength)
    {
        return vec2(0.0);
    }

    vec2 centre = 0.5 + (vec2(wetHash(cell + index * 1.7), wetHash(cell + index * 3.1 + 11.0)) - 0.5) * 0.3;
    vec2 delta = fract(point) - centre;
    float reach = max(length(delta), 1e-4);
    float band = reach - age * k_rippleMaxRadius;
    float window = max(1.0 - abs(band) / k_rippleWidth, 0.0);
    float fade = (1.0 - age) * (1.0 - age);

    return delta / reach * cos(band * k_rippleWaves) * window * window * fade;
}

// Makes a surface wet where it is outdoors and faces the sky: darkens porous albedo, removes
// roughness, and on flat ground fills puddles (near-mirror water with ripples).
// position:       world position.
// geometryNormal: interpolated geometric normal (facing the drawn side).
// outdoor:        1 outdoors, 0 inside an interior zone.
// viewDistance:   metres from the camera (ripples fade out with it).
// albedo, roughness, metallic, normal: the material, changed in place.
// wet, puddle:    how wet the surface and how much puddle there is, 0 to 1.
void applyWetness(vec3 position, vec3 geometryNormal, float outdoor, float viewDistance, inout vec3 albedo,
    inout float roughness, inout float metallic, inout vec3 normal, out float wet, out float puddle)
{
    wet = 0.0;
    puddle = 0.0;
    float amount = frame.wetness.x * outdoor;

    if (amount <= 0.0)
    {
        return;
    }

    float up = geometryNormal.y;
    float exposure = clamp(0.55 + 0.45 * up, 0.0, 1.0) * smoothstep(-0.5, -0.1, up);
    vec2 noisePoint = position.xz * frame.wetness.z;
    float noise = wetNoise(noisePoint) * 0.6 + wetNoise(noisePoint * 2.3 + 7.1) * 0.3
        + wetNoise(noisePoint * 5.1 + 3.7) * 0.1;
    wet = amount * exposure * mix(0.8, 1.0, noise);

    float porosity = (1.0 - metallic) * smoothstep(0.3, 0.8, roughness);
    albedo *= mix(1.0, frame.wetnessShading.x, wet * porosity);
    roughness = mix(roughness, roughness * (1.0 - frame.wetnessShading.y), wet);

    if (frame.wetness.y <= 0.0 || up < 0.9)
    {
        return;
    }

    float threshold = mix(0.85, 0.25, frame.wetness.y);
    puddle = smoothstep(threshold, threshold + 0.06, noise) * smoothstep(0.9, 0.97, up) * smoothstep(0.5, 1.0, amount);

    if (puddle <= 0.0)
    {
        return;
    }

    albedo *= mix(1.0, k_puddleDarkening, puddle);
    metallic *= 1.0 - puddle;
    roughness = mix(roughness, k_puddleRoughness, puddle);
    vec3 water = vec3(0.0, 1.0, 0.0);
    float strength = frame.wetnessShading.z;
    float rippleFade = 1.0 - smoothstep(k_rippleFadeStart, k_rippleFadeEnd, viewDistance);

    if (strength > 0.0 && rippleFade > 0.0)
    {
        float time = frame.wetness.w;
        vec2 slope = rippleLayer(position.xz, time, vec2(0.0), strength)
            + rippleLayer(position.xz, time * 0.87 + 0.5, vec2(0.37, 0.71), strength);
        slope *= k_rippleSlope * strength * rippleFade;
        water = normalize(vec3(-slope.x, 1.0, -slope.y));
    }

    normal = normalize(mix(normal, water, puddle));
}

// The debug tint of wet surfaces (blue) and puddles (cyan) over a pixel's colour.
vec3 wetnessDebugColor(vec3 color, float wet, float puddle)
{
    float brightness = dot(color, vec3(0.333)) + 0.05;
    color = mix(color, k_wetDebugTint * brightness, wet * 0.6);
    return mix(color, k_puddleDebugTint * brightness, puddle);
}
