#version 450

layout(set = 0, binding = 0) uniform Camera
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 position;
} camera;

layout(location = 0) out vec4 outFarPoint;

void main()
{
    vec2 ndc = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;

    outFarPoint = inverse(camera.viewProjection) * vec4(ndc, 1.0, 1.0);
    gl_Position = vec4(ndc, 1.0, 1.0);
}
