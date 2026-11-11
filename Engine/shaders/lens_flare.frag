#version 450

// Lens flares and sun glare at quarter resolution (LensFlare.cpp), added to the bloom result by
// bloom_up_flare.frag. Output is in scene (pre-exposure) units, scaled so the tonemap's bloom mix
// adds it at its own brightness.
//
// constants.ghosts:   x ghost brightness (0 off), y exposed brightness threshold, z halo
//                     brightness, w halo radius (screen heights).
// constants.lens:     x chromatic shift (screen heights), y aspect ratio, z depth below which a
//                     pixel is the viewmodel, w factor from flare light to the bloom target.
// constants.sun:      xy the sun's screen uv, z glare brightness (0 none), w starburst share.
// constants.sunColor: rgb the sun's radiance, w the starburst rotation.
// constants.exposure: x 1 to read the auto exposure, y stops added, z fixed exposure multiplier.

layout(push_constant) uniform FlareConstants
{
    vec4 ghosts;
    vec4 lens;
    vec4 sun;
    vec4 sunColor;
    vec4 exposure;
} constants;

layout(set = 2, binding = 0) uniform sampler2D source;
layout(set = 2, binding = 1) uniform sampler2D depthTexture;
layout(set = 2, binding = 2) uniform sampler2D exposureTexture;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

const int k_ghostCount = 5;
const float k_ghostScales[k_ghostCount] = float[](-0.35, -0.7, -1.25, -2.1, 0.55);
const float k_ghostWeights[k_ghostCount] = float[](1.6, 1.0, 0.6, 0.3, 1.2);
const vec3 k_ghostTints[k_ghostCount] = vec3[](vec3(0.55, 0.75, 1.0), vec3(1.0, 0.8, 0.45), vec3(0.5, 1.0, 0.65),
    vec3(0.85, 0.55, 1.0), vec3(1.0, 0.6, 0.5));
const int k_sunTaps = 12;
const float k_excessSoftness = 1.0 / 24.0;
const float k_sunTapRadius = 0.012;

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float exposureMultiplier()
{
    float stops = constants.exposure.x > 0.5 ? texelFetch(exposureTexture, ivec2(0), 0).r + constants.exposure.y : 0.0;

    return exp2(stops) * constants.exposure.z;
}

// The part of the source above the threshold, in exposed units and softly compressed so a very
// bright source gives a bounded ghost; viewmodel and off-screen taps give
// nothing, and taps fade towards the screen edges so ghosts do not pop at the border.
vec3 brightTap(vec2 uv, float exposure)
{
    vec2 fromCentre = abs(uv - 0.5) * 2.0;

    if (max(fromCentre.x, fromCentre.y) >= 1.0)
    {
        return vec3(0.0);
    }

    if (textureLod(depthTexture, uv, 0.0).r < constants.lens.z)
    {
        return vec3(0.0);
    }

    vec3 color = textureLod(source, uv, 0.0).rgb * exposure;
    float bright = luminance(color);
    float excess = max(bright - constants.ghosts.y, 0.0);
    float edge = 1.0 - smoothstep(0.6, 1.0, max(fromCentre.x, fromCentre.y));

    return color * (excess / (max(bright, 1e-4) * (1.0 + excess * k_excessSoftness))) * edge;
}

vec3 ghosts(vec2 uv, float exposure)
{
    vec3 sum = vec3(0.0);

    for (int index = 0; index < k_ghostCount; ++index)
    {
        float scale = k_ghostScales[index];
        vec2 sourceUv = 0.5 + (uv - 0.5) / scale;
        sum += brightTap(sourceUv, exposure) * k_ghostTints[index] * k_ghostWeights[index];
    }

    return sum * constants.ghosts.x;
}

vec3 halo(vec2 uv, float exposure)
{
    vec2 aspect = vec2(constants.lens.y, 1.0);
    vec2 fromCentre = (uv - 0.5) * aspect;
    float distanceFromCentre = length(fromCentre);
    vec2 direction = distanceFromCentre > 1e-4 ? fromCentre / distanceFromCentre : vec2(0.0, 1.0);
    float ring = 1.0 - smoothstep(0.0, 0.06, abs(distanceFromCentre - constants.ghosts.w));

    if (ring <= 0.0)
    {
        return vec3(0.0);
    }

    vec2 mirrored = 0.5 - direction * constants.ghosts.w / aspect;
    vec2 shift = direction * constants.lens.x / aspect;
    vec3 color = vec3(brightTap(mirrored - shift, exposure).r, brightTap(mirrored, exposure).g,
        brightTap(mirrored + shift, exposure).b);

    return color * ring * constants.ghosts.z;
}

float sunVisibility()
{
    float visible = 0.0;

    for (int index = 0; index < k_sunTaps; ++index)
    {
        float angle = float(index) * 2.39996323;
        float radius = sqrt((float(index) + 0.5) / float(k_sunTaps)) * k_sunTapRadius;
        vec2 uv = constants.sun.xy + vec2(cos(angle) / constants.lens.y, sin(angle)) * radius;

        if (all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)))
            && textureLod(depthTexture, uv, 0.0).r >= 1.0)
        {
            visible += 1.0;
        }
    }

    return visible / float(k_sunTaps);
}

vec3 sunGlare(vec2 uv)
{
    float visibility = sunVisibility();

    if (visibility <= 0.0)
    {
        return vec3(0.0);
    }

    vec2 offset = (uv - constants.sun.xy) * vec2(constants.lens.y, 1.0);
    float distanceFromSun = length(offset);
    float squared = distanceFromSun * distanceFromSun;
    float veil = 1.0 / (1.0 + squared * 900.0) + 0.12 / (1.0 + squared * 30.0);
    float angle = atan(offset.y, offset.x) + constants.sunColor.w;
    float rays = pow(abs(cos(angle * 3.0)), 80.0) + 0.5 * pow(abs(cos(angle * 3.0 + 1.5708)), 160.0);
    float burst = rays * exp(-distanceFromSun * 6.0) * 2.0;
    float shape = mix(veil, burst, constants.sun.w);

    return constants.sunColor.rgb * shape * visibility * constants.sun.z;
}

void main()
{
    float exposure = exposureMultiplier();
    vec3 color = vec3(0.0);

    if (constants.ghosts.x > 0.0)
    {
        color += ghosts(inUv, exposure);
    }

    if (constants.ghosts.z > 0.0)
    {
        color += halo(inUv, exposure);
    }

    color /= max(exposure, 1e-6);

    if (constants.sun.z > 0.0)
    {
        color += sunGlare(inUv);
    }

    outColor = vec4(color * constants.lens.w, 1.0);
}
