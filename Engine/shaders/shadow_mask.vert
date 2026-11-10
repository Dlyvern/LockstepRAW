#version 450

// Per instance (gl_InstanceIndex), the caster's model matrix premultiplied by its shadow tile's
// atlas view-projection. Binding: k_shadowInstanceBinding in ShadowMaterials.hpp. The push
// constants (alpha mask) are read by shadow_mask.frag only.
layout(set = 1, binding = 1, std430) readonly buffer ShadowInstances
{
    mat4 lightModels[];
} instances;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec2 outUv;

void main()
{
    outUv = inUv;
    gl_Position = instances.lightModels[gl_InstanceIndex] * vec4(inPosition, 1.0);
}
