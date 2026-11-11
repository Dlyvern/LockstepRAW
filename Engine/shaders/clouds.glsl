// The cloud layer and lightning in the sky (CloudSettings, LightningSettings). Requires
// frame_lighting.glsl and sky_model.glsl, and a `sampler2D cloudNoise` declared before including
// it: the tiling cloud noise (CloudNoise.cpp: r broad shapes, g medium, b fine detail, a cells).
// The noise tile spans 1 / frame.cloudShape.z metres.

const float k_cloudFarDistance = 36000.0;
const float k_cloudMinElevation = 0.015;
const float k_flatLayerShare = 0.35;

const float k_cloudModeFlat = 1.0;
const float k_cloudModeVolumetric = 2.0;

vec2 cloudUv(vec2 xz)
{
    return (xz + frame.cloudShape.xy) * frame.cloudShape.z;
}

// 0 (clear) to 1 (cloud) for a broad noise value, by the coverage.
float cloudCover(float broad)
{
    float threshold = 1.0 - frame.cloudParams.x * 1.1;
    return smoothstep(threshold - 0.02, threshold + 0.3, broad);
}

float broadNoise(vec4 noise)
{
    return noise.r * 0.7 + noise.g * 0.3;
}

// Light reaching the clouds from the sky around them (no sun): brighter above, darker below.
vec3 cloudSkyLight()
{
    vec3 toSun = -frame.skySunDirection.xyz;
    vec3 sideways = normalize(vec3(toSun.x, 0.25, toSun.z) + vec3(1e-4, 0.0, 0.0));
    return (skyRadiance(vec3(0.0, 1.0, 0.0)) * 0.6 + skyRadiance(sideways) * 0.4) * frame.cloudLight.rgb;
}

float henyeyGreenstein(float cosAngle, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(max(1.0 + g2 - 2.0 * g * cosAngle, 1e-4), 1.5));
}

// Scattering phase towards the camera: a forward lobe (the silver lining) over an even part.
float cloudPhase(float cosAngle)
{
    float silver = clamp(frame.cloudLight.w, 0.0, 0.9);
    return mix(0.08, henyeyGreenstein(cosAngle, silver) * 4.0 * 3.14159265, 0.5) * 0.9
        + henyeyGreenstein(cosAngle, -0.25) * 0.4;
}

// Light added by a lightning flash to sky or cloud seen along `direction`.
vec3 lightningGlow(vec3 direction)
{
    if (frame.lightningFlash.w <= 0.0)
    {
        return vec3(0.0);
    }

    float towards = clamp(dot(direction, frame.lightningDirection.xyz) * 0.5 + 0.5, 0.0, 1.0);
    float above = smoothstep(-0.05, 0.2, direction.y);
    return frame.lightningFlash.rgb * (0.2 + 0.8 * pow(towards, 6.0)) * above;
}

// Mean cloud colour for image-based light: sky light and a share of sunlight.
vec3 cloudAverageRadiance(vec3 skyLight)
{
    float sunHeight = clamp(-frame.skySunDirection.y, 0.0, 1.0);
    return skyLight * 0.75 + frame.skySunRadiance.rgb * frame.cloudMarch.x * (0.04 + 0.06 * sunHeight);
}

// The sky seen along `direction` with the clouds' coverage folded in, for image-based light: the
// sky greys towards the clouds' average colour by the coverage (less near the horizon below).
vec3 overcastSky(vec3 direction, vec3 sky, vec3 cloudColor)
{
    if (frame.cloudShape.w <= 0.0)
    {
        return sky;
    }

    float coverage = clamp(frame.cloudParams.x, 0.0, 1.0);
    float amount = coverage * clamp(frame.cloudMarch.y, 0.0, 1.0) * smoothstep(-0.1, 0.25, direction.y);
    return mix(sky, cloudColor, amount) + lightningGlow(direction) * coverage;
}

// The flat cloud layer along `direction` (y > 0): rgb radiance, a coverage. `sky` is the sky's
// radiance behind it and `skyLight` cloudSkyLight().
vec4 flatCloudLayer(vec3 direction, vec3 skyLight, float lod)
{
    if (direction.y <= k_cloudMinElevation)
    {
        return vec4(0.0);
    }

    float height = frame.cloudParams.z + frame.cloudParams.w * k_flatLayerShare;
    float distance = height / direction.y;
    vec3 position = direction * distance;
    vec2 uv = cloudUv(position.xz);
    float broad = broadNoise(textureLod(cloudNoise, uv, lod));
    float cover = cloudCover(broad);
    float detail = textureLod(cloudNoise, uv * 6.0, lod).b;
    float density = clamp(cover * (1.0 - 0.45 * detail * (1.0 - cover)), 0.0, 1.0);
    float alpha = clamp(density * frame.cloudParams.y * 1.4, 0.0, 1.0);
    alpha *= smoothstep(k_cloudFarDistance, k_cloudFarDistance * 0.4, distance);

    vec3 toSun = -frame.skySunDirection.xyz;
    vec2 sunStep = toSun.xz * (frame.cloudParams.w * 0.5 / max(toSun.y, 0.1));
    float towardsSun = cloudCover(broadNoise(textureLod(cloudNoise, cloudUv(position.xz + sunStep), lod)));
    float sunLight = exp(-2.5 * towardsSun * frame.cloudParams.y) * (1.0 - 0.5 * density);
    float cosAngle = dot(direction, toSun);
    vec3 sun = frame.skySunRadiance.rgb * frame.cloudMarch.x * sunLight * cloudPhase(cosAngle) * 0.25;
    vec3 ambient = skyLight * mix(1.0, 0.45, density);
    vec3 radiance = sun + ambient + lightningGlow(direction) * (0.6 + density);

    return vec4(radiance * alpha, alpha);
}

float boltHash(float value)
{
    return fract(sin(value * 127.1 + frame.lightningBolt.x * 311.7) * 43758.5453);
}

float boltNoise(float value)
{
    float cell = floor(value);
    float t = value - cell;
    return mix(boltHash(cell), boltHash(cell + 1.0), t) * 2.0 - 1.0;
}

// A lightning bolt in the sky below the clouds, towards frame.lightningDirection: radiance along
// `direction`.
vec3 lightningBolt(vec3 direction)
{
    float visibility = frame.lightningDirection.w;

    if (visibility <= 0.0 || direction.y < -0.02)
    {
        return vec3(0.0);
    }

    vec3 target = frame.lightningDirection.xyz;
    vec2 targetAzimuth = normalize(target.xz + vec2(1e-5));
    vec2 rayAzimuth = normalize(direction.xz + vec2(1e-5));
    float sideways = targetAzimuth.x * rayAzimuth.y - targetAzimuth.y * rayAzimuth.x;
    float facing = dot(targetAzimuth, rayAzimuth);
    float topElevation = target.y;
    float elevation = direction.y;

    if (facing <= 0.0 || elevation > topElevation)
    {
        return vec3(0.0);
    }

    float along = 1.0 - elevation / max(topElevation, 1e-3);
    float wander = (boltNoise(along * 7.0) * 0.6 + boltNoise(along * 19.0) * 0.3 + boltNoise(along * 53.0) * 0.1)
        * topElevation * 0.35 * along;
    float width = 0.0009 + 0.0006 * (1.0 - along);
    float core = exp(-pow((sideways - wander) / width, 2.0));

    float branchStart = 0.3 + 0.3 * boltHash(5.0);
    float branchAlong = max(along - branchStart, 0.0);
    float branchSide = wander + sign(boltHash(9.0) - 0.5) * branchAlong * topElevation * 0.5
        + boltNoise(along * 31.0 + 4.0) * topElevation * 0.08 * branchAlong;
    float branch = along > branchStart && along < branchStart + 0.35
        ? exp(-pow((sideways - branchSide) / (width * 0.7), 2.0)) * (1.0 - branchAlong / 0.35) : 0.0;

    float glow = exp(-abs(sideways - wander) / (width * 12.0)) * 0.08;
    return frame.lightningFlash.rgb * 40.0 * visibility * (core + branch * 0.6 + glow) / max(frame.lightningFlash.w, 0.05);
}
