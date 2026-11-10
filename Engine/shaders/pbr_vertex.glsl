// The PBR vertex shader, shared by pbr.vert (static meshes) and pbr_skinned.vert (SKINNED
// defined: skinned meshes, draw.flags.w is the joint palette base).

#define FRAME_LIGHTING_BINDING 5
#include "frame_lighting.glsl"

// draw.flags.z: 0 the camera view, 1 the planar reflection view, 2 the camera view of the
// rendered planar reflector's surface, 3 the camera view of a viewmodel draw.
const float k_reflectionView = 1.0;
const float k_viewModelView = 3.0;

layout(push_constant) uniform DrawConstants
{
    mat4 model;
    mat4 previousModel;
    vec4 baseColorFactor;
    vec4 emissiveNormalScale;
    vec4 surface;
    vec4 flags;
    vec4 detail;
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
    outUv = inUv;
    outNormal = transpose(inverse(linear)) * inNormal;
    outTangent = vec4(linear * inTangent.xyz, inTangent.w);
    vec4 previousWorldPosition = previousModel * vec4(inPosition, 1.0);

    if (draw.flags.z == k_viewModelView)
    {
        outCurrentClip = frame.viewModelMotionViewProjection * worldPosition;
        outPreviousClip = frame.previousViewModelMotionViewProjection * previousWorldPosition;
        gl_Position = frame.viewModelViewProjection * worldPosition;
        return;
    }

    outCurrentClip = frame.viewProjection * worldPosition;
    outPreviousClip = frame.previousViewProjection * previousWorldPosition;

    gl_Position = draw.flags.z == k_reflectionView ? frame.reflectionViewProjection * worldPosition
        : camera.viewProjection * worldPosition;
}
