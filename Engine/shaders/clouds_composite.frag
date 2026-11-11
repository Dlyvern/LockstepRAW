#version 450

// Composites the reduced-resolution volumetric clouds over the sky pixels of the lit scene's colour,
// alpha-blended (src * a + dst * (1 - a)): the clouds' light over the sky they let through.
// Geometry pixels are left alone.

const float k_skyDepth = 1.0;
const float k_minCoverage = 1e-3;

layout(set = 2, binding = 0) uniform sampler2D clouds;
layout(set = 2, binding = 1) uniform sampler2D sceneDepth;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main()
{
    if (texelFetch(sceneDepth, ivec2(gl_FragCoord.xy), 0).r < k_skyDepth)
    {
        discard;
    }

    vec4 cloud = texture(clouds, inUv);
    float coverage = 1.0 - cloud.a;

    if (coverage < k_minCoverage)
    {
        discard;
    }

    outColor = vec4(cloud.rgb / coverage, coverage);
}
