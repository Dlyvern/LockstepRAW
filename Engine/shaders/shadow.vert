#version 450

layout(push_constant) uniform ShadowConstants
{
    mat4 lightModel;
} constants;

layout(location = 0) in vec3 inPosition;

void main()
{
    gl_Position = constants.lightModel * vec4(inPosition, 1.0);
}
