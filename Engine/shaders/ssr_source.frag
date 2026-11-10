#version 450
#extension GL_GOOGLE_include_directive : require

// The finished lit HDR scene (colour plus occluded ambient) at half resolution, kept for the next
// frame's screen-space reflections. Each pixel's bilinear tap averages the 2x2 full-resolution
// pixels under it.

#include "scene_hdr.glsl"

const float k_maxRadiance = 1024.0;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 color = sceneHdr(inUv);
    outColor = vec4(clamp(color, vec3(0.0), vec3(k_maxRadiance)), 1.0);
}
