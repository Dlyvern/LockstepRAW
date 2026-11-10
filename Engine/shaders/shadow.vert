#version 450

// Per instance (gl_InstanceIndex), the caster's model matrix premultiplied by its shadow tile's
// atlas view-projection. Binding: k_shadowInstanceBinding in ShadowMaterials.hpp.
layout(set = 1, binding = 1, std430) readonly buffer ShadowInstances
{
    mat4 lightModels[];
} instances;

layout(location = 0) in vec3 inPosition;

void main()
{
    gl_Position = instances.lightModels[gl_InstanceIndex] * vec4(inPosition, 1.0);
}
