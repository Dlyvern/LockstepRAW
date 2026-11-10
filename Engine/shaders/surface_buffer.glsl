// The lit pass's G-buffer (FrameAttachments.hpp): surfaceNormal and surfaceSpecular.
//
// surfaceNormal:   xy octahedral view-space normal, z roughness, w one minus the share of the
//                  colour target that is direct sunlight.
// surfaceSpecular: rgb the environment specular light the lit pass put into ambientLit, w one
//                  minus the signed specular weight (the split-sum specular colour's largest
//                  channel, times specular occlusion; negative for metals).
//
// Alpha holds "one minus" values so alpha blending stays consistent: blending black with alpha a
// over these targets scales every stored quantity, alpha ones included, by (1 - a), which is what
// glass and fog do to the light the values describe. Writing alpha 1 resets them to "nothing".

vec2 octahedralWrap(vec2 value)
{
    return (1.0 - abs(value.yx)) * vec2(value.x >= 0.0 ? 1.0 : -1.0, value.y >= 0.0 ? 1.0 : -1.0);
}

vec2 encodeOctahedral(vec3 normal)
{
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    return normal.z >= 0.0 ? normal.xy : octahedralWrap(normal.xy);
}

vec3 decodeOctahedral(vec2 encoded)
{
    vec3 normal = vec3(encoded, 1.0 - abs(encoded.x) - abs(encoded.y));
    float wrap = clamp(-normal.z, 0.0, 1.0);
    normal.x += normal.x >= 0.0 ? -wrap : wrap;
    normal.y += normal.y >= 0.0 ? -wrap : wrap;
    return normalize(normal);
}

vec4 encodeSurfaceNormal(vec3 viewNormal, float roughness, float sunShare)
{
    return vec4(encodeOctahedral(viewNormal), roughness, 1.0 - clamp(sunShare, 0.0, 1.0));
}

vec4 encodeSurfaceSpecular(vec3 environmentSpecular, float specularWeight, bool isMetal)
{
    return vec4(environmentSpecular, 1.0 - (isMetal ? -specularWeight : specularWeight));
}

// Signed specular weight from surfaceSpecular.w: negative for metals.
float decodeSpecularWeight(float stored)
{
    return 1.0 - stored;
}

const vec4 k_neutralSurfaceNormal = vec4(0.0, 0.0, 1.0, 1.0);
const vec4 k_neutralSurfaceSpecular = vec4(0.0, 0.0, 0.0, 1.0);
