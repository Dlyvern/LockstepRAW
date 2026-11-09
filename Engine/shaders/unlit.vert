#version 450

layout(push_constant) uniform ModelConstants
{
    mat4 model;
} modelConstants;

layout(set = 0, binding = 0) uniform Camera
{
    mat4 view;
    mat4 projection;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec2 outUv;

void main()
{
    outUv = inUv;
    gl_Position = camera.projection * camera.view * modelConstants.model * vec4(inPosition, 1.0);
}
