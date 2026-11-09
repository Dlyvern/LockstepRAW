#version 450
#extension GL_GOOGLE_include_directive : require

#define FRAME_LIGHTING_BINDING 0
#include "frame_lighting.glsl"
#include "sky_model.glsl"

layout(set = 0, binding = 0) uniform Camera
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 position;
} camera;

layout(location = 0) in vec4 inFarPoint;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAmbient;

void main()
{
    vec3 direction = normalize(inFarPoint.xyz / inFarPoint.w - camera.position.xyz);
    vec3 radiance = skyRadiance(direction);
    float cosSun = dot(direction, -frame.sunDirection.xyz);

    if (frame.sunDisk.w > 0.0 && direction.y > 0.0)
    {
        float edge = 1.0 - frame.sunDisk.w;
        radiance += frame.sunDisk.rgb * smoothstep(frame.sunDisk.w - edge * 0.3, frame.sunDisk.w + edge * 0.3, cosSun);
    }

    outColor = vec4(radiance, 1.0);
    outAmbient = vec4(0.0);
}
