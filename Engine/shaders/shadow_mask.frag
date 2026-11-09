#version 450

layout(push_constant) uniform ShadowConstants
{
    mat4 lightModel;
    vec4 mask;
} constants;

layout(set = 1, binding = 0) uniform sampler2D baseColorTexture;

layout(location = 0) in vec2 inUv;

void main()
{
    if (texture(baseColorTexture, inUv).a * constants.mask.x < constants.mask.y)
    {
        discard;
    }
}
