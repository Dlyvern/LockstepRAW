#version 450

layout(push_constant) uniform DrawConstants
{
    mat4 model;
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

void main()
{
    vec4 worldPosition = draw.model * vec4(inPosition, 1.0);
    mat3 linear = mat3(draw.model);

    outWorldPosition = worldPosition.xyz;
    outUv = inUv;
    outNormal = transpose(inverse(linear)) * inNormal;
    outTangent = vec4(linear * inTangent.xyz, inTangent.w);

    gl_Position = camera.viewProjection * worldPosition;
}
