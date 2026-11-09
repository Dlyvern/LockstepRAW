#version 450
#extension GL_GOOGLE_include_directive : require

#include "skinning.glsl"

// mask.xy: as in shadow_mask.frag; mask.z: the joint palette base.
layout(push_constant) uniform ShadowConstants
{
    mat4 lightModel;
    vec4 mask;
} constants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inJoints;
layout(location = 3) in vec4 inWeights;

layout(location = 0) out vec2 outUv;

void main()
{
    outUv = inUv;
    mat4 skin = skinMatrix(uint(constants.mask.z), inJoints, inWeights, 0u);
    gl_Position = constants.lightModel * (skin * vec4(inPosition, 1.0));
}
