#version 450
#extension GL_GOOGLE_include_directive : require

#define FRAME_LIGHTING_BINDING 0
#include "frame_lighting.glsl"
#include "octahedral.glsl"
#include "sky_model.glsl"

const float k_pi = 3.14159265359;
const uint k_specularSamples = 32u;
const uint k_irradianceSamples = 64u;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

vec2 hammersley(uint index, uint count)
{
    uint bits = bitfieldReverse(index);
    return vec2(float(index) / float(count), float(bits) * 2.3283064365386963e-10);
}

mat3 basisAround(vec3 n)
{
    vec3 up = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, n));
    vec3 bitangent = cross(n, tangent);

    return mat3(tangent, bitangent, n);
}

vec3 irradianceOverPi(vec3 n)
{
    mat3 basis = basisAround(n);
    vec3 sum = vec3(0.0);

    for (uint index = 0u; index < k_irradianceSamples; ++index)
    {
        vec2 xi = hammersley(index, k_irradianceSamples);
        float phi = 2.0 * k_pi * xi.y;
        float cosTheta = sqrt(1.0 - xi.x);
        float sinTheta = sqrt(xi.x);
        sum += skyRadiance(basis * vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta));
    }

    return sum / float(k_irradianceSamples);
}

vec3 prefiltered(vec3 n, float roughness)
{
    mat3 basis = basisAround(n);
    float alpha = roughness * roughness;
    vec3 sum = vec3(0.0);
    float weight = 0.0;

    for (uint index = 0u; index < k_specularSamples; ++index)
    {
        vec2 xi = hammersley(index, k_specularSamples);
        float phi = 2.0 * k_pi * xi.y;
        float cosTheta = sqrt((1.0 - xi.x) / (1.0 + (alpha * alpha - 1.0) * xi.x));
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        vec3 h = basis * vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
        vec3 l = reflect(-n, h);
        float nDotL = dot(n, l);

        if (nDotL > 0.0)
        {
            sum += skyRadiance(l) * nDotL;
            weight += nDotL;
        }
    }

    return sum / max(weight, 1e-4);
}

void main()
{
    vec2 pixel = gl_FragCoord.xy;
    float tile = floor(pixel.x / k_environmentTileSize);
    vec2 local = vec2(pixel.x - tile * k_environmentTileSize, pixel.y) / k_environmentTileSize;
    vec3 direction = octahedralDecode(local);
    vec3 radiance;

    if (tile >= k_irradianceTile)
    {
        radiance = irradianceOverPi(direction);
    }
    else if (tile < 0.5)
    {
        radiance = skyRadiance(direction);
    }
    else
    {
        radiance = prefiltered(direction, tile / (k_specularLevels - 1.0));
    }

    outColor = vec4(radiance, 1.0);
}
