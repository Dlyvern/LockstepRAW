// Joint palettes of skinned draws (Engine/src/Render/Skinning.cpp). Per joint of a palette, the
// current skinning matrix, then the previous frame's.
layout(std430, set = 1, binding = 6) readonly buffer JointPalettes
{
    mat4 matrices[];
} jointPalettes;

// base:    index of the palette's first matrix.
// joints:  the vertex's four joint indices.
// weights: the vertex's four joint weights.
// frame:   0 for the current matrices, 1 for the previous frame's.
mat4 skinMatrix(uint base, vec4 joints, vec4 weights, uint frame)
{
    return weights.x * jointPalettes.matrices[base + uint(joints.x) * 2u + frame]
        + weights.y * jointPalettes.matrices[base + uint(joints.y) * 2u + frame]
        + weights.z * jointPalettes.matrices[base + uint(joints.z) * 2u + frame]
        + weights.w * jointPalettes.matrices[base + uint(joints.w) * 2u + frame];
}
