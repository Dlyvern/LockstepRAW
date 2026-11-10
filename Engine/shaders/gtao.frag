#version 450
#extension GL_GOOGLE_include_directive : require

// Ground-truth-style ambient occlusion (horizon search per screen slice, cosine-weighted arc
// integral against the projected normal) and sun contact shadows, at half resolution.
// Output: r diffuse visibility, g specular visibility (reflection cone vs. visibility cone),
// b the multiplier contact shadows leave on the colour target (1 - sunShare * shadowed).

#include "surface_buffer.glsl"
#include "view_reconstruct.glsl"

const float k_pi = 3.14159265359;
const float k_halfPi = 1.57079632679;
const float k_falloffShare = 0.6;
const float k_maxPixelRadius = 256.0;

layout(push_constant) uniform GtaoConstants
{
    vec4 projection;
    vec4 occlusion;
    vec4 sampling;
    vec4 sunView;
    vec4 contact;
} constants;

layout(set = 2, binding = 0) uniform sampler2D sceneDepth;
layout(set = 2, binding = 1) uniform sampler2D surfaceNormal;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outOcclusion;

vec3 viewPositionAt(vec2 uv)
{
    return viewPositionFromBuffer(uv, textureLod(sceneDepth, uv, 0.0).r, constants.projection);
}

float fastAcos(float x)
{
    float magnitude = abs(x);
    float result = (-0.156583 * magnitude + k_halfPi) * sqrt(1.0 - magnitude);
    return x >= 0.0 ? result : k_pi - result;
}

float groundTruthVisibility(vec2 uv, vec3 position, vec3 normal, vec3 toCamera, vec2 pixel, vec2 fullSize)
{
    float radius = constants.occlusion.x;
    float pixelRadius = min(radius * abs(constants.projection.y) * 0.5 * fullSize.y / -position.z, k_maxPixelRadius);

    if (pixelRadius < 1.0)
    {
        return 1.0;
    }

    uint sliceCount = uint(constants.sampling.x);
    uint stepCount = uint(constants.sampling.y);
    float noiseSlice = interleavedGradientNoise(pixel + constants.sampling.z * 5.588238);
    float noiseStep = interleavedGradientNoise(pixel.yx + constants.sampling.z * 7.123);
    float falloffRange = k_falloffShare * radius;
    float falloffMul = -1.0 / falloffRange;
    float falloffAdd = (radius - falloffRange) / falloffRange + 1.0;
    vec2 texel = 1.0 / fullSize;
    float visibility = 0.0;

    for (uint slice = 0u; slice < sliceCount; ++slice)
    {
        float angle = (float(slice) + noiseSlice) * k_pi / float(sliceCount);
        vec2 omega = vec2(cos(angle), sin(angle));
        vec3 direction = normalize(vec3(omega.x / constants.projection.x, omega.y / constants.projection.y, 0.0));
        vec3 orthoDirection = direction - dot(direction, toCamera) * toCamera;
        vec3 axis = normalize(cross(orthoDirection, toCamera));
        vec3 projectedNormal = normal - axis * dot(normal, axis);
        float projectedLength = length(projectedNormal);

        if (projectedLength < 1e-4)
        {
            visibility += 1.0;
            continue;
        }

        float signNormal = dot(orthoDirection, projectedNormal) >= 0.0 ? 1.0 : -1.0;
        float cosNormal = clamp(dot(projectedNormal, toCamera) / projectedLength, 0.0, 1.0);
        float n = signNormal * fastAcos(cosNormal);
        float lowCos0 = cos(n + k_halfPi);
        float lowCos1 = cos(n - k_halfPi);
        float horizonCos0 = lowCos0;
        float horizonCos1 = lowCos1;

        for (uint step = 0u; step < stepCount; ++step)
        {
            float fraction = (float(step) + noiseStep) / float(stepCount);
            fraction = fraction * fraction;
            vec2 offset = omega * max(fraction * pixelRadius, float(step) + 1.0) * texel;

            vec3 delta0 = viewPositionAt(uv + offset) - position;
            vec3 delta1 = viewPositionAt(uv - offset) - position;
            float length0 = length(delta0);
            float length1 = length(delta1);
            float weight0 = clamp(length0 * falloffMul + falloffAdd, 0.0, 1.0);
            float weight1 = clamp(length1 * falloffMul + falloffAdd, 0.0, 1.0);
            float cos0 = dot(delta0 / max(length0, 1e-5), toCamera);
            float cos1 = dot(delta1 / max(length1, 1e-5), toCamera);
            horizonCos0 = max(horizonCos0, mix(lowCos0, cos0, weight0));
            horizonCos1 = max(horizonCos1, mix(lowCos1, cos1, weight1));
        }

        float h0 = -fastAcos(horizonCos1);
        float h1 = fastAcos(horizonCos0);
        h0 = n + clamp(h0 - n, -k_halfPi, k_halfPi);
        h1 = n + clamp(h1 - n, -k_halfPi, k_halfPi);
        float sinN = sin(n);
        float arc0 = (cosNormal + 2.0 * h0 * sinN - cos(2.0 * h0 - n)) * 0.25;
        float arc1 = (cosNormal + 2.0 * h1 * sinN - cos(2.0 * h1 - n)) * 0.25;
        visibility += projectedLength * (arc0 + arc1);
    }

    visibility = clamp(visibility / float(sliceCount), 0.0, 1.0);
    visibility = pow(visibility, constants.occlusion.z);

    return 1.0 - (1.0 - visibility) * constants.occlusion.y;
}

float contactShadow(vec3 position, vec3 normal, vec2 pixel)
{
    vec3 toSun = constants.sunView.xyz;

    if (dot(normal, toSun) <= 0.0)
    {
        return 1.0;
    }

    uint stepCount = uint(constants.contact.z);
    float jitter = interleavedGradientNoise(pixel + constants.sampling.z * 3.731);
    vec3 start = position + normal * (0.003 * -position.z);
    vec3 ray = toSun * constants.sunView.w;

    for (uint step = 0u; step < stepCount; ++step)
    {
        float fraction = (float(step) + jitter) / float(stepCount);
        vec3 samplePoint = start + ray * fraction;

        if (samplePoint.z >= -1e-3)
        {
            break;
        }

        vec2 uv = projectViewToUv(samplePoint, constants.projection);

        if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
        {
            break;
        }

        float sceneZ = viewDepthFromBuffer(textureLod(sceneDepth, uv, 0.0).r, constants.projection);
        float inFront = sceneZ - samplePoint.z;
        float bias = 0.0015 * -samplePoint.z + 0.004;

        if (inFront > bias && inFront < constants.contact.x)
        {
            float edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
            return 1.0 - constants.contact.y * smoothstep(0.0, 0.03, edge) * (1.0 - fraction * fraction);
        }
    }

    return 1.0;
}

void main()
{
    vec2 fullSize = vec2(textureSize(sceneDepth, 0));
    ivec2 fullPixel = min(ivec2(gl_FragCoord.xy) * 2, ivec2(fullSize) - 1);
    vec2 uv = (vec2(fullPixel) + 0.5) / fullSize;
    float depth = texelFetch(sceneDepth, fullPixel, 0).r;

    if (depth >= 1.0)
    {
        outOcclusion = vec4(1.0);
        return;
    }

    vec4 surface = texelFetch(surfaceNormal, fullPixel, 0);
    vec3 position = viewPositionFromBuffer(uv, depth, constants.projection);
    vec3 normal = decodeOctahedral(surface.xy);
    vec3 toCamera = normalize(-position);
    float diffuse = 1.0;
    float specular = 1.0;
    float contact = 1.0;

    if (constants.occlusion.w > 0.0)
    {
        diffuse = groundTruthVisibility(uv, position, normal, toCamera, gl_FragCoord.xy, fullSize);
        specular = diffuse;

        if (constants.sampling.w > 0.0)
        {
            float nDotV = clamp(dot(normal, toCamera), 0.0, 1.0);
            float roughness = surface.z;
            specular = clamp(pow(nDotV + diffuse, exp2(-16.0 * roughness - 1.0)) - 1.0 + diffuse, 0.0, 1.0);
        }
    }

    float sunShare = 1.0 - surface.w;

    if (constants.contact.w > 0.0 && sunShare > 1e-3)
    {
        contact = 1.0 - sunShare * (1.0 - contactShadow(position, normal, gl_FragCoord.xy));
    }

    outOcclusion = vec4(diffuse, specular, contact, 1.0);
}
