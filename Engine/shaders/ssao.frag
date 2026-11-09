#version 450

const uint k_sampleCount = 12u;
const float k_goldenAngle = 2.39996323;

layout(push_constant) uniform SsaoConstants
{
    vec4 projection;
    vec4 params;
} constants;

layout(set = 2, binding = 0) uniform sampler2D sceneDepth;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outOcclusion;

float viewDepth(float depth)
{
    return -constants.projection.w / (depth + constants.projection.z);
}

vec3 viewPosition(vec2 uv, float depth)
{
    float z = viewDepth(depth);
    vec2 ndc = uv * 2.0 - 1.0;

    return vec3(ndc.x * -z / constants.projection.x, ndc.y * -z / constants.projection.y, z);
}

vec2 projectToUv(vec3 position)
{
    vec2 ndc = vec2(position.x * constants.projection.x, position.y * constants.projection.y) / -position.z;
    return ndc * 0.5 + 0.5;
}

float interleavedGradientNoise(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

vec3 viewPositionAtTexel(ivec2 texel, vec2 texelSize)
{
    return viewPosition((vec2(texel) + 0.5) * texelSize, texelFetch(sceneDepth, texel, 0).r);
}

void main()
{
    ivec2 depthSize = textureSize(sceneDepth, 0);
    vec2 texel = 1.0 / vec2(depthSize);
    ivec2 fullPixel = min(ivec2(gl_FragCoord.xy) * 2, depthSize - 1);
    float depth = texelFetch(sceneDepth, fullPixel, 0).r;

    if (depth >= 1.0)
    {
        outOcclusion = vec4(1.0);
        return;
    }

    vec3 center = viewPosition((vec2(fullPixel) + 0.5) * texel, depth);
    vec3 left = viewPositionAtTexel(max(fullPixel - ivec2(1, 0), ivec2(0)), texel);
    vec3 right = viewPositionAtTexel(min(fullPixel + ivec2(1, 0), depthSize - 1), texel);
    vec3 up = viewPositionAtTexel(max(fullPixel - ivec2(0, 1), ivec2(0)), texel);
    vec3 down = viewPositionAtTexel(min(fullPixel + ivec2(0, 1), depthSize - 1), texel);
    vec3 dx = abs(right.z - center.z) < abs(center.z - left.z) ? right - center : center - left;
    vec3 dy = abs(down.z - center.z) < abs(center.z - up.z) ? down - center : center - up;
    vec3 normal = normalize(cross(dy, dx));

    if (dot(normal, center) > 0.0)
    {
        normal = -normal;
    }

    float radius = constants.params.x;
    float noise = interleavedGradientNoise(gl_FragCoord.xy) * 6.2831853;
    vec3 helper = abs(normal.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(helper, normal));
    vec3 bitangent = cross(normal, tangent);
    float occlusion = 0.0;

    for (uint index = 0u; index < k_sampleCount; ++index)
    {
        float fraction = (float(index) + 0.5) / float(k_sampleCount);
        float angle = float(index) * k_goldenAngle + noise;
        float planar = sqrt(fraction);
        float height = sqrt(1.0 - fraction);
        float length = mix(0.15, 1.0, fraction * fraction) * radius;
        vec3 offset = (tangent * cos(angle) + bitangent * sin(angle)) * planar + normal * height;
        vec3 samplePosition = center + offset * length;
        vec2 sampleUv = projectToUv(samplePosition);

        if (any(lessThan(sampleUv, vec2(0.0))) || any(greaterThan(sampleUv, vec2(1.0))))
        {
            continue;
        }

        float sceneZ = viewDepth(textureLod(sceneDepth, sampleUv, 0.0).r);
        float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(center.z - sceneZ), 1e-4));
        occlusion += (sceneZ >= samplePosition.z + 0.02 * radius - 0.002 * center.z ? 1.0 : 0.0) * rangeCheck;
    }

    float visibility = 1.0 - occlusion / float(k_sampleCount) * constants.params.y;
    outOcclusion = vec4(pow(clamp(visibility, 0.0, 1.0), constants.params.z));
}
