// The PBR vertex shader, shared by pbr.vert (static meshes) and pbr_skinned.vert (SKINNED
// defined: skinned meshes, draw.flags.w is the joint palette base).

#define FRAME_LIGHTING_BINDING 5
#include "frame_lighting.glsl"
#include "wind.glsl"

// draw.flags.z: 0 the camera view, 1 the planar reflection view, 2 the camera view of the
// rendered planar reflector's surface, 3 the camera view of a viewmodel draw, 4 the scope view,
// 5 the camera view of the scope's eyepiece lens (a viewmodel draw), 6 the camera view of a
// holographic sight's window (a viewmodel draw).
const float k_reflectionView = 1.0;
const float k_viewModelView = 3.0;
const float k_scopeView = 4.0;
const float k_scopeLensView = 5.0;
const float k_sightWindowView = 6.0;

layout(push_constant) uniform DrawConstants
{
    mat4 model;
    mat4 previousModel;
    vec4 baseColorFactor;
    vec4 emissiveNormalScale;
    vec4 surface;
    vec4 flags;
    vec4 detail;
    vec4 shading;
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

#ifdef SKINNED
#include "skinning.glsl"

layout(location = 4) in vec4 inJoints;
layout(location = 5) in vec4 inWeights;
#endif

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec2 outUv;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec4 outTangent;
layout(location = 4) out vec4 outCurrentClip;
layout(location = 5) out vec4 outPreviousClip;
layout(location = 6) out vec3 outObjectPosition;

void main()
{
#ifdef SKINNED
    uint jointBase = uint(draw.flags.w);
    mat4 model = draw.model * skinMatrix(jointBase, inJoints, inWeights, 0u);
    mat4 previousModel = draw.previousModel * skinMatrix(jointBase, inJoints, inWeights, 1u);
#else
    mat4 model = draw.model;
    mat4 previousModel = draw.previousModel;
#endif

    vec4 worldPosition = model * vec4(inPosition, 1.0);
    mat3 linear = mat3(model);

    outWorldPosition = worldPosition.xyz;
    outObjectPosition = inPosition;
    outUv = inUv;
    outNormal = transpose(inverse(linear)) * inNormal;
    outTangent = vec4(linear * inTangent.xyz, inTangent.w);
    vec4 previousWorldPosition = previousModel * vec4(inPosition, 1.0);

#ifndef SKINNED
    if (draw.surface.w >= 0.0 && frame.wind.z > 0.0 && draw.flags.z != k_viewModelView
        && draw.flags.z != k_scopeLensView && draw.flags.z != k_sightWindowView && !isInsideAmbientZone(model[3].xyz))
    {
        worldPosition.xyz += windOffset(frame.wind, worldPosition.xyz, model[3].xyz, worldPosition.y - model[3].y);
        previousWorldPosition.xyz += windOffset(frame.windPrevious, previousWorldPosition.xyz, previousModel[3].xyz,
            previousWorldPosition.y - previousModel[3].y);
        outWorldPosition = worldPosition.xyz;
    }
#endif

    if (draw.flags.z == k_viewModelView || draw.flags.z == k_scopeLensView || draw.flags.z == k_sightWindowView)
    {
        outCurrentClip = frame.viewModelMotionViewProjection * worldPosition;
        outPreviousClip = frame.previousViewModelMotionViewProjection * previousWorldPosition;
        gl_Position = frame.viewModelViewProjection * worldPosition;
        return;
    }

    outCurrentClip = frame.viewProjection * worldPosition;
    outPreviousClip = frame.previousViewProjection * previousWorldPosition;

    if (draw.flags.z == k_scopeView)
    {
        gl_Position = frame.scopeViewProjection * worldPosition;
        return;
    }

    gl_Position = draw.flags.z == k_reflectionView ? frame.reflectionViewProjection * worldPosition
        : camera.viewProjection * worldPosition;
}
