#version 450
#extension GL_GOOGLE_include_directive : require

#define FRAME_LIGHTING_BINDING 5
#include "frame_lighting.glsl"

// draw.flags.z: 0 the camera view, 1 the planar reflection view, 2 the camera view of the
// rendered planar reflector's surface.
const float k_reflectionView = 1.0;

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

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec2 outUv;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec4 outTangent;
layout(location = 4) out vec4 outCurrentClip;
layout(location = 5) out vec4 outPreviousClip;

void main()
{
    vec4 worldPosition = draw.model * vec4(inPosition, 1.0);
    mat3 linear = mat3(draw.model);

    outWorldPosition = worldPosition.xyz;
    outUv = inUv;
    outNormal = transpose(inverse(linear)) * inNormal;
    outTangent = vec4(linear * inTangent.xyz, inTangent.w);
    outCurrentClip = frame.viewProjection * worldPosition;
    outPreviousClip = frame.previousViewProjection * (draw.previousModel * vec4(inPosition, 1.0));

    gl_Position = draw.flags.z == k_reflectionView ? frame.reflectionViewProjection * worldPosition
        : camera.viewProjection * worldPosition;
}
