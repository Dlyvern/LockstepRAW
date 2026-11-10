#version 450

layout(set = 1, binding = 0) uniform sampler2D albedo;

layout(location = 0) in vec2 inUv;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAmbient;
layout(location = 3) out vec4 outSurfaceNormal;
layout(location = 4) out vec4 outSurfaceSpecular;

void main()
{
    outColor = texture(albedo, inUv);
    outAmbient = vec4(0.0);
    outSurfaceNormal = vec4(0.0, 0.0, 1.0, 1.0);
    outSurfaceSpecular = vec4(0.0, 0.0, 0.0, 1.0);
}
