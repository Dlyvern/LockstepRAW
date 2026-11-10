#version 450
#extension GL_GOOGLE_include_directive : require

// Screen-space reflection rays at half resolution. Each pixel reflects the view ray off a
// microfacet normal drawn from its GGX lobe (jittered per frame, so rough surfaces blur over
// time), marches it through the full-resolution depth buffer with perspective-correct depth,
// refines the first crossing by bisection and writes where it landed.
// Output: xy hit uv, z confidence (0: no hit), w roughness.

#include "surface_buffer.glsl"
#include "view_reconstruct.glsl"

const float k_pi = 3.14159265359;
const uint k_refinementSteps = 5u;
const float k_nearPlaneMargin = 0.05;

layout(push_constant) uniform TraceConstants
{
    vec4 projection;
    vec4 ray;
    vec4 fade;
} constants;

layout(set = 2, binding = 0) uniform sampler2D sceneDepth;
layout(set = 2, binding = 1) uniform sampler2D surfaceNormal;
layout(set = 2, binding = 2) uniform sampler2D surfaceSpecular;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outHit;

float sceneViewDepth(vec2 uv)
{
    return viewDepthFromBuffer(textureLod(sceneDepth, uv, 0.0).r, constants.projection);
}

vec3 sampleGgxNormal(vec3 normal, float alpha, vec2 xi)
{
    float phi = 2.0 * k_pi * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 helper = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(helper, normal));
    vec3 bitangent = cross(normal, tangent);

    return normalize(tangent * (cos(phi) * sinTheta) + bitangent * (sin(phi) * sinTheta) + normal * cosTheta);
}

float screenEdgeFade(vec2 uv)
{
    vec2 edge = min(uv, 1.0 - uv);
    return smoothstep(0.0, constants.fade.x, min(edge.x, edge.y));
}

void main()
{
    vec2 fullSize = vec2(textureSize(sceneDepth, 0));
    ivec2 fullPixel = min(ivec2(gl_FragCoord.xy) * 2, ivec2(fullSize) - 1);
    vec2 uv = (vec2(fullPixel) + 0.5) / fullSize;
    float depth = texelFetch(sceneDepth, fullPixel, 0).r;
    vec4 surface = texelFetch(surfaceNormal, fullPixel, 0);
    float roughness = surface.z;
    float maxRoughness = constants.ray.w;
    float specularWeight = abs(decodeSpecularWeight(texelFetch(surfaceSpecular, fullPixel, 0).a));

    if (depth >= 1.0 || roughness > maxRoughness || specularWeight < 0.004)
    {
        outHit = vec4(0.0, 0.0, 0.0, roughness);
        return;
    }

    vec3 position = viewPositionFromBuffer(uv, depth, constants.projection);
    vec3 normal = decodeOctahedral(surface.xy);
    vec3 view = normalize(position);
    vec2 xi = vec2(interleavedGradientNoise(gl_FragCoord.xy + constants.fade.y * 5.588238),
        interleavedGradientNoise(gl_FragCoord.yx + constants.fade.y * 3.1416 + 17.0));
    vec3 microNormal = sampleGgxNormal(normal, roughness * roughness, xi);
    vec3 direction = reflect(view, microNormal);

    if (dot(direction, normal) <= 0.0)
    {
        direction = reflect(view, normal);
    }

    float rayLength = constants.ray.x;
    float limitZ = viewDepthFromBuffer(0.0, constants.projection) - k_nearPlaneMargin;

    if (position.z + direction.z * rayLength > limitZ)
    {
        rayLength = (limitZ - position.z) / direction.z;
    }

    vec3 origin = position + normal * (0.002 * -position.z);
    vec3 end = origin + direction * rayLength;
    vec2 uvStart = projectViewToUv(origin, constants.projection);
    vec2 uvEnd = projectViewToUv(end, constants.projection);
    float inverseZStart = 1.0 / origin.z;
    float inverseZEnd = 1.0 / end.z;
    uint stepCount = uint(constants.ray.z);
    float jitter = interleavedGradientNoise(gl_FragCoord.xy * 1.37 + constants.fade.y * 11.0);
    float previousT = 0.0;
    float hitT = -1.0;

    for (uint step = 1u; step <= stepCount; ++step)
    {
        float t = (float(step) - 1.0 + jitter) / float(stepCount);
        t = max(t, 1.0 / float(stepCount * 4u));
        vec2 sampleUv = mix(uvStart, uvEnd, t);

        if (any(lessThan(sampleUv, vec2(0.0))) || any(greaterThan(sampleUv, vec2(1.0))))
        {
            break;
        }

        float rayZ = 1.0 / mix(inverseZStart, inverseZEnd, t);
        float previousRayZ = 1.0 / mix(inverseZStart, inverseZEnd, previousT);
        float behind = sceneViewDepth(sampleUv) - rayZ;
        float thickness = constants.ray.y + abs(rayZ - previousRayZ);

        if (behind > 0.0 && behind < thickness)
        {
            hitT = t;
            break;
        }

        previousT = t;
    }

    if (hitT < 0.0)
    {
        outHit = vec4(0.0, 0.0, 0.0, roughness);
        return;
    }

    float low = previousT;
    float high = hitT;

    for (uint step = 0u; step < k_refinementSteps; ++step)
    {
        float middle = 0.5 * (low + high);
        float rayZ = 1.0 / mix(inverseZStart, inverseZEnd, middle);

        if (sceneViewDepth(mix(uvStart, uvEnd, middle)) - rayZ > 0.0)
        {
            high = middle;
        }
        else
        {
            low = middle;
        }
    }

    vec2 hitUv = mix(uvStart, uvEnd, high);
    ivec2 hitPixel = clamp(ivec2(hitUv * fullSize), ivec2(0), ivec2(fullSize) - 1);
    vec3 hitNormal = decodeOctahedral(texelFetch(surfaceNormal, hitPixel, 0).xy);
    float facing = smoothstep(0.0, 0.2, -dot(hitNormal, direction));
    float towardsCamera = 1.0 - smoothstep(0.3, 0.8, direction.z);
    float distanceFade = 1.0 - smoothstep(0.75, 1.0, high);
    float roughnessFade = 1.0 - smoothstep(maxRoughness * 0.7, maxRoughness, roughness);
    float confidence = screenEdgeFade(hitUv) * facing * towardsCamera * distanceFade * roughnessFade;

    outHit = vec4(hitUv, confidence, roughness);
}
