#version 450

layout(push_constant) uniform DrawConstants
{
    mat4 model;
    vec4 surface;
} draw;

layout(set = 0, binding = 0) uniform Camera
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 position;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec2 outUv;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outToCamera;

void main()
{
    vec4 worldPosition = draw.model * vec4(inPosition, 1.0);

    outWorldPosition = worldPosition.xyz;
    outUv = inUv;
    outNormal = transpose(inverse(mat3(draw.model))) * inNormal;
    outToCamera = camera.position.xyz - worldPosition.xyz;

    gl_Position = camera.viewProjection * worldPosition;
}
