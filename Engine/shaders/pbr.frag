#version 450
#extension GL_GOOGLE_include_directive : require

#define FRAME_LIGHTING_BINDING 5
#include "frame_lighting.glsl"
#include "octahedral.glsl"

const float k_pi = 3.14159265359;
const float k_minRoughness = 0.045;
const vec3 k_cascadeTints[4] = vec3[](vec3(1.0, 0.3, 0.3), vec3(0.3, 1.0, 0.3), vec3(0.3, 0.5, 1.0),
    vec3(1.0, 1.0, 0.3));

const float k_reflectionView = 1.0;
const float k_reflectorSurface = 2.0;
const float k_reflectorAlignment = 0.995;

layout(push_constant) uniform DrawConstants
{
    mat4 model;
    mat4 previousModel;
    vec4 baseColorFactor;
    vec4 emissiveNormalScale;
    vec4 surface;
    vec4 flags;
} draw;

layout(set = 0, binding = 0) uniform Camera
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 position;
} camera;

layout(set = 1, binding = 0) uniform sampler2D baseColorTexture;
layout(set = 1, binding = 1) uniform sampler2D normalTexture;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessTexture;
layout(set = 1, binding = 3) uniform sampler2D occlusionTexture;
layout(set = 1, binding = 4) uniform sampler2D emissiveTexture;

layout(set = 2, binding = 0) uniform sampler2DShadow shadowAtlas;
layout(set = 2, binding = 1) uniform sampler2D environmentAtlas;
layout(set = 2, binding = 2) uniform sampler2D brdfLut;
layout(set = 2, binding = 3) uniform sampler2D planarReflection;

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inCurrentClip;
layout(location = 5) in vec4 inPreviousClip;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAmbient;
layout(location = 2) out vec4 outVelocity;

struct Surface
{
    vec3 normal;
    vec3 toCamera;
    vec3 diffuseColor;
    vec3 f0;
    float roughness;
    float nDotV;
};

// 3x3 bilinear PCF: nine hardware-filtered comparisons (each a 2x2 bilinear PCF), one texel
// apart, so the kernel covers 4x4 texels with tent weights.
float filteredShadow(vec2 uv, float depth)
{
    vec2 texel = 1.0 / vec2(textureSize(shadowAtlas, 0));
    float sum = 0.0;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            sum += texture(shadowAtlas, vec3(uv + vec2(x, y) * texel, depth));
        }
    }

    return sum / 9.0;
}

// Looks a world position up in one tile of the shadow atlas. Returns -1 when it falls outside
// the tile (minus a margin that keeps the PCF footprint inside), else the lit fraction.
float tileShadow(uint tile, vec3 position)
{
    vec4 clip = frame.shadowTileViewProjection[tile] * vec4(position, 1.0);

    if (clip.w <= 0.0)
    {
        return -1.0;
    }

    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    vec2 tileOrigin = vec2(float(tile & 1u), float(tile >> 1u)) * 0.5;
    vec2 local = (uv - tileOrigin) * 2.0;
    float margin = 3.0 / (float(textureSize(shadowAtlas, 0).x) * 0.5);

    if (any(lessThan(local, vec2(margin))) || any(greaterThan(local, vec2(1.0 - margin))) || ndc.z >= 1.0)
    {
        return -1.0;
    }

    return filteredShadow(uv, ndc.z);
}

float sunShadow(vec3 position, vec3 geometryNormal, out int cascade)
{
    cascade = -1;
    uint count = uint(frame.shadowParams.x);

    for (uint index = 0u; index < count; ++index)
    {
        float texel = frame.cascadeTexelSize[index];
        vec3 biased = position + geometryNormal * (texel * frame.shadowParams.z);
        float lit = tileShadow(index, biased);

        if (lit >= 0.0)
        {
            cascade = int(index);
            return lit;
        }
    }

    return 1.0;
}

float spotShadow(vec3 position, vec3 geometryNormal, float lightDistance)
{
    float normalOffset = frame.spotShadowParams.y * lightDistance;
    vec3 biased = position + geometryNormal * normalOffset;
    float lit = tileShadow(uint(frame.spotShadowParams.w), biased);

    return lit < 0.0 ? 1.0 : lit;
}

float distributionGgx(float nDotH, float alpha)
{
    float alpha2 = alpha * alpha;
    float denominator = nDotH * nDotH * (alpha2 - 1.0) + 1.0;

    return alpha2 / (k_pi * denominator * denominator);
}

float visibilitySmithGgxCorrelated(float nDotV, float nDotL, float alpha)
{
    float alpha2 = alpha * alpha;
    float lambdaV = nDotL * sqrt(nDotV * nDotV * (1.0 - alpha2) + alpha2);
    float lambdaL = nDotV * sqrt(nDotL * nDotL * (1.0 - alpha2) + alpha2);

    return 0.5 / max(lambdaV + lambdaL, 1e-5);
}

vec3 fresnelSchlick(vec3 f0, float cosine)
{
    float factor = pow(1.0 - cosine, 5.0);
    return f0 + (1.0 - f0) * factor;
}

// Outgoing radiance from one light. Light colours are scaled so that a white Lambert surface
// facing a light of intensity 1 reflects exactly 1 (the pi of the Lambert BRDF is folded in).
vec3 shadeLight(Surface surface, vec3 toLight, vec3 lightColor)
{
    float nDotL = dot(surface.normal, toLight);

    if (nDotL <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 diffuse = surface.diffuseColor;
    vec3 specular = vec3(0.0);

    if (frame.counts.y != 0u)
    {
        vec3 halfVector = normalize(toLight + surface.toCamera);
        float nDotH = max(dot(surface.normal, halfVector), 0.0);
        float vDotH = max(dot(surface.toCamera, halfVector), 0.0);
        float alpha = surface.roughness * surface.roughness;
        vec3 fresnel = fresnelSchlick(surface.f0, vDotH);
        specular = k_pi * distributionGgx(nDotH, alpha) * visibilitySmithGgxCorrelated(surface.nDotV, nDotL, alpha)
            * fresnel;
        diffuse *= 1.0 - fresnel;
    }

    return (diffuse + specular) * lightColor * nDotL;
}

float distanceFalloff(float lightDistance, float range)
{
    float ratio = lightDistance / range;
    float window = clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);

    return window * window / max(lightDistance * lightDistance, 0.01);
}

vec3 environmentSpecular(vec3 direction, float roughness)
{
    float level = roughness * (k_specularLevels - 1.0);
    float lower = floor(level);
    float upper = min(lower + 1.0, k_specularLevels - 1.0);
    vec3 a = textureLod(environmentAtlas, environmentAtlasUv(lower, direction), 0.0).rgb;
    vec3 b = textureLod(environmentAtlas, environmentAtlasUv(upper, direction), 0.0).rgb;

    return mix(a, b, level - lower);
}

vec3 perturbedNormal(vec3 normal, vec3 position)
{
    vec3 sampled = vec3(texture(normalTexture, inUv).xy * 2.0 - 1.0, 0.0);
    sampled.z = sqrt(max(1.0 - dot(sampled.xy, sampled.xy), 0.0));
    sampled.xy *= draw.emissiveNormalScale.w;

    vec3 tangent;
    vec3 bitangent;

    if (dot(inTangent.xyz, inTangent.xyz) > 1e-6)
    {
        tangent = normalize(inTangent.xyz - normal * dot(normal, inTangent.xyz));
        bitangent = cross(normal, tangent) * (inTangent.w < 0.0 ? -1.0 : 1.0);
    }
    else
    {
        vec3 dpdx = dFdx(position);
        vec3 dpdy = dFdy(position);
        vec2 duvdx = dFdx(inUv);
        vec2 duvdy = dFdy(inUv);
        vec3 dpdyPerp = cross(dpdy, normal);
        vec3 dpdxPerp = cross(normal, dpdx);
        tangent = dpdyPerp * duvdx.x + dpdxPerp * duvdy.x;
        bitangent = dpdyPerp * duvdx.y + dpdxPerp * duvdy.y;
        float scale = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-12));
        tangent *= scale;
        bitangent *= scale;
    }

    return normalize(mat3(tangent, bitangent, normal) * sampled);
}

// The planar reflection seen at this surface point: the reflection view projects the point into
// its texture. Faces that are not parallel to the mirror plane (a frame's bevels) get none.
// Returns rgb radiance and a weight in [0, 1].
vec4 planarReflectionAt(vec3 position, vec3 normal)
{
    if (frame.reflectionCamera.w <= 0.0)
    {
        return vec4(0.0);
    }

    float alignment = dot(normal, frame.reflectionPlane.xyz);

    if (alignment < k_reflectorAlignment)
    {
        return vec4(0.0);
    }

    vec4 clip = frame.reflectionViewProjection * vec4(position, 1.0);
    vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
    vec3 radiance = textureLod(planarReflection, uv, 0.0).rgb * frame.reflectionTint.rgb;
    float weight = smoothstep(k_reflectorAlignment, 1.0, alignment);

    return vec4(radiance, weight);
}

vec4 motionVector()
{
    vec2 current = inCurrentClip.xy / inCurrentClip.w;
    vec2 previous = inPreviousClip.xy / inPreviousClip.w;

    return vec4((current - previous) * 0.5, max(inPreviousClip.w, 0.0), 0.0);
}

void main()
{
    bool isReflectionView = draw.flags.z == k_reflectionView;
    vec3 cameraPosition = isReflectionView ? frame.reflectionCamera.xyz : camera.position.xyz;
    vec4 baseColor = texture(baseColorTexture, inUv) * draw.baseColorFactor;
    bool isMasked = draw.surface.w >= 0.0;
    bool isBlended = draw.flags.y > 0.5;

    if (isMasked && baseColor.a < draw.surface.w)
    {
        discard;
    }

    float cameraDistance = length(cameraPosition - inWorldPosition);
    vec3 toCamera = (cameraPosition - inWorldPosition) / max(cameraDistance, 1e-4);
    vec3 geometryNormal = normalize(inNormal);

    if (draw.flags.x > 0.5 && !gl_FrontFacing)
    {
        geometryNormal = -geometryNormal;
    }

    vec4 metallicRoughness = texture(metallicRoughnessTexture, inUv);
    float roughness = clamp(metallicRoughness.g * draw.surface.y, k_minRoughness, 1.0);
    float metallic = clamp(metallicRoughness.b * draw.surface.x, 0.0, 1.0);
    float occlusion = 1.0 + draw.surface.z * (texture(occlusionTexture, inUv).r - 1.0);
    vec3 emissive = texture(emissiveTexture, inUv).rgb * draw.emissiveNormalScale.rgb;

    Surface surface;
    surface.normal = perturbedNormal(geometryNormal, inWorldPosition);
    surface.toCamera = toCamera;
    surface.diffuseColor = baseColor.rgb * (1.0 - metallic);
    surface.f0 = mix(vec3(0.04), baseColor.rgb, metallic);
    surface.roughness = roughness;
    surface.nDotV = max(dot(surface.normal, toCamera), 1e-4);

    vec3 direct = vec3(0.0);
    int cascade = -1;

    if (frame.sunDirection.w > 0.0)
    {
        vec3 toLight = -frame.sunDirection.xyz;
        float shadow = sunShadow(inWorldPosition, geometryNormal, cascade);
        direct += shadeLight(surface, toLight, frame.sunRadiance.rgb) * shadow;
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

        if (frame.spotShadowParams.x == float(index))
        {
            attenuation *= spotShadow(inWorldPosition, geometryNormal, lightDistance);
        }

        direct += shadeLight(surface, toLight, light.color.rgb * attenuation);
    }

    vec3 ambient = surface.diffuseColor * frame.ambient.rgb * occlusion;

    if (frame.iblParams.w > 0.0)
    {
        vec2 scaleBias = texture(brdfLut, vec2(surface.nDotV, roughness)).rg;
        vec3 specularColor = surface.f0 * scaleBias.x + scaleBias.y;
        vec3 irradiance = textureLod(environmentAtlas, environmentAtlasUv(k_irradianceTile, surface.normal), 0.0).rgb;
        vec3 reflected = reflect(-toCamera, surface.normal);
        float specularOcclusion = clamp(pow(surface.nDotV + occlusion, exp2(-16.0 * roughness - 1.0)) - 1.0 + occlusion,
            0.0, 1.0);

        ambient += surface.diffuseColor * (1.0 - specularColor) * irradiance * occlusion * frame.iblParams.x;

        if (frame.counts.y != 0u)
        {
            ambient += environmentSpecular(reflected, roughness) * specularColor * specularOcclusion * frame.iblParams.y;
        }
    }

    vec3 color = direct + emissive;

    if (draw.flags.z == k_reflectorSurface)
    {
        vec4 planar = planarReflectionAt(inWorldPosition, surface.normal);
        vec2 scaleBias = texture(brdfLut, vec2(surface.nDotV, roughness)).rg;
        vec3 specularColor = surface.f0 * scaleBias.x + scaleBias.y;

        if (planar.a > 0.0)
        {
            ambient *= 1.0 - planar.a * specularColor;
            color += planar.rgb * specularColor * planar.a;
        }
    }

    if (frame.shadowParams.w > 0.5 && cascade >= 0)
    {
        color *= k_cascadeTints[cascade];
        ambient *= k_cascadeTints[cascade];
    }

    if (frame.fogParams.y > 0.0)
    {
        float fogDistance = max(cameraDistance - frame.fogParams.x, 0.0) * frame.fogColor.w;
        float fog = min(1.0 - exp(-fogDistance * fogDistance), frame.fogParams.z);
        color = mix(color, frame.fogColor.rgb, fog);
        ambient *= 1.0 - fog;
    }

    outVelocity = motionVector();

    if (isBlended || isReflectionView)
    {
        outColor = vec4(color + ambient, isBlended ? baseColor.a : 1.0);
        outAmbient = vec4(0.0, 0.0, 0.0, isBlended ? baseColor.a : 1.0);
        return;
    }

    outColor = vec4(color, 1.0);
    outAmbient = vec4(ambient, 1.0);
}
