// The lit scene as one HDR colour: direct light (and everything already composited into the
// colour target) plus ambient light darkened by SSAO. Slots 0-2 of set 2.

layout(set = 2, binding = 0) uniform sampler2D sceneColor;
layout(set = 2, binding = 1) uniform sampler2D sceneAmbient;
layout(set = 2, binding = 2) uniform sampler2D ambientOcclusion;

vec3 sceneHdr(vec2 uv)
{
    vec3 color = textureLod(sceneColor, uv, 0.0).rgb;
    vec3 ambient = textureLod(sceneAmbient, uv, 0.0).rgb;

    return color + ambient * textureLod(ambientOcclusion, uv, 0.0).r;
}
