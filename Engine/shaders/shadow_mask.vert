#version 450

layout(push_constant) uniform ShadowConstants
{
    mat4 lightModel;
    vec4 mask;
} constants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec2 outUv;

void main()
{
    outUv = inUv;
    gl_Position = constants.lightModel * vec4(inPosition, 1.0);
}
