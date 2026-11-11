#version 450

// One rain drop on the lens per instance (GpuLensDroplet in LensRainPass.cpp): a screen-space quad
// around the drop, long enough for a running drop's stretch.

const vec2 k_corners[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0),
    vec2(1.0, 1.0), vec2(-1.0, 1.0));
const float k_margin = 1.15;

layout(push_constant) uniform LensRainConstants
{
    vec4 params;
    vec4 fade;
    vec4 options;
} constants;

layout(location = 0) in vec4 inShape;
layout(location = 1) in vec4 inParams;

layout(location = 0) out vec2 outLocal;
layout(location = 1) out vec4 outDrop;
layout(location = 2) out float outOpacity;

void main()
{
    vec2 corner = k_corners[gl_VertexIndex];
    float aspectRatio = constants.params.x;
    float radius = inShape.z;
    float stretch = inShape.w;
    vec2 halfSize = vec2(radius / aspectRatio, radius * stretch) * k_margin;
    vec2 uv = inShape.xy + corner * halfSize;

    outLocal = corner * k_margin;
    outDrop = vec4(inShape.xy, radius, stretch);
    outOpacity = inParams.x;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
