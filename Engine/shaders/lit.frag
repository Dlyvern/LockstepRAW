#version 450

// Must match FrameLighting.hpp.
const uint k_maxLocalLights = 16;
const float k_casterNone = -2.0;
const float k_casterDirectional = -1.0;
const float k_spotLight = 1.0;

struct LocalLight
{
    vec4 positionRange;
    vec4 color;
    vec4 directionCosOuter;
    vec4 cosInnerKind;
};

layout(push_constant) uniform DrawConstants
{
    mat4 model;
    vec4 surface;
} draw;

layout(set = 1, binding = 0) uniform sampler2D albedoTexture;

layout(set = 1, binding = 1, std430) readonly buffer FrameLighting
{
    mat4 shadowViewProjection;
    vec4 ambient;
    vec4 directionalDirection;
    vec4 directionalColor;
    vec4 fogColor;
    vec4 fogParams;
    vec4 shadowParams;
    uvec4 counts;
    LocalLight lights[k_maxLocalLights];
} frame;

layout(set = 2, binding = 0) uniform sampler2D shadowMap;

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inToCamera;

layout(location = 0) out vec4 outColor;

float bilinear(vec4 quad, vec2 weight)
{
    vec2 rows = mix(quad.wx, quad.zy, weight.x);
    return mix(rows.x, rows.y, weight.y);
}

vec4 compareQuad(vec2 uv, float depth)
{
    return step(vec4(depth), textureGather(shadowMap, uv, 0));
}

float sampleShadow(vec3 worldPosition, vec3 normal, vec3 toLight, float lightDistance)
{
    float normalOffset = frame.shadowParams.z * mix(1.0, lightDistance, frame.shadowParams.w);
    vec3 biased = worldPosition + normal * normalOffset + toLight * frame.shadowParams.y;
    vec4 clip = frame.shadowViewProjection * vec4(biased, 1.0);

    if (clip.w <= 0.0)
    {
        return 1.0;
    }

    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;

    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || ndc.z >= 1.0)
    {
        return 1.0;
    }

    float depth = ndc.z;
    vec2 size = vec2(textureSize(shadowMap, 0));
    vec2 texel = 1.0 / size;
    vec2 position = uv * size - 0.5;
    vec2 base = floor(position);
    vec2 weight = position - base;

    // 4x4 texels around the sample, compared in four gathers, filtered as 3x3 bilinear PCF.
    vec4 q00 = compareQuad((base + vec2(0.0, 0.0)) * texel, depth);
    vec4 q10 = compareQuad((base + vec2(2.0, 0.0)) * texel, depth);
    vec4 q01 = compareQuad((base + vec2(0.0, 2.0)) * texel, depth);
    vec4 q11 = compareQuad((base + vec2(2.0, 2.0)) * texel, depth);

    // Gather order: x = (0,1), y = (1,1), z = (1,0), w = (0,0).
    float t[16];
    t[0] = q00.w;  t[1] = q00.z;  t[2] = q10.w;  t[3] = q10.z;
    t[4] = q00.x;  t[5] = q00.y;  t[6] = q10.x;  t[7] = q10.y;
    t[8] = q01.w;  t[9] = q01.z;  t[10] = q11.w; t[11] = q11.z;
    t[12] = q01.x; t[13] = q01.y; t[14] = q11.x; t[15] = q11.y;

    float sum = 0.0;

    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            int i = y * 4 + x;
            sum += bilinear(vec4(t[i + 4], t[i + 5], t[i + 1], t[i]), weight);
        }
    }

    return sum / 9.0;
}

vec3 shade(vec3 albedo, vec3 normal, vec3 toCamera, vec3 toLight, vec3 radiance)
{
    float lambert = max(dot(normal, toLight), 0.0);

    if (lambert <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 result = albedo * lambert;

    if (frame.counts.y != 0u)
    {
        vec3 halfVector = normalize(toLight + toCamera);
        float shininess = draw.surface.y;
        float normalization = (shininess + 8.0) / 8.0;
        result += vec3(draw.surface.x * normalization * pow(max(dot(normal, halfVector), 0.0), shininess) * lambert);
    }

    return result * radiance;
}

float distanceFalloff(float lightDistance, float range)
{
    float ratio = lightDistance / range;
    float window = clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);

    return window * window / max(lightDistance * lightDistance, 0.01);
}

void main()
{
    vec3 albedo = texture(albedoTexture, inUv).rgb;
    vec3 normal = normalize(inNormal);
    float cameraDistance = length(inToCamera);
    vec3 toCamera = inToCamera / max(cameraDistance, 1e-4);

    if (dot(normal, toCamera) < 0.0)
    {
        normal = -normal;
    }

    vec3 color = albedo * (frame.ambient.rgb + vec3(draw.surface.z));

    if (frame.directionalDirection.w > 0.0)
    {
        vec3 toLight = -frame.directionalDirection.xyz;
        float shadow = frame.shadowParams.x == k_casterDirectional ? sampleShadow(inWorldPosition, normal, toLight, 1.0) : 1.0;
        color += shade(albedo, normal, toCamera, toLight, frame.directionalColor.rgb) * shadow;
    }

    for (uint index = 0u; index < frame.counts.x; ++index)
    {
        LocalLight light = frame.lights[index];
        vec3 offset = light.positionRange.xyz - inWorldPosition;
        float lightDistance = length(offset);

        if (lightDistance >= light.positionRange.w)
        {
            continue;
        }

        vec3 toLight = offset / max(lightDistance, 1e-4);
        float attenuation = distanceFalloff(lightDistance, light.positionRange.w);

        if (light.cosInnerKind.y == k_spotLight)
        {
            float cosAngle = dot(-toLight, light.directionCosOuter.xyz);
            attenuation *= smoothstep(light.directionCosOuter.w, light.cosInnerKind.x, cosAngle);
        }

        if (attenuation <= 0.0)
        {
            continue;
        }

        if (frame.shadowParams.x == float(index))
        {
            attenuation *= sampleShadow(inWorldPosition, normal, toLight, lightDistance);
        }

        color += shade(albedo, normal, toCamera, toLight, light.color.rgb * attenuation);
    }

    if (frame.fogParams.y > 0.0)
    {
        float fogDistance = max(cameraDistance - frame.fogParams.x, 0.0) * frame.fogColor.w;
        float fog = min(1.0 - exp(-fogDistance * fogDistance), frame.fogParams.z);
        color = mix(color, frame.fogColor.rgb, fog);
    }

    outColor = vec4(color, 1.0);
}
