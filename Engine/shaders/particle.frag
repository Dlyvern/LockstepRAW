#version 450
#extension GL_GOOGLE_include_directive : require

#define FRAME_LIGHTING_BINDING 0
#include "frame_lighting.glsl"

// Particle sprite: atlas frame (blended with the next for flipbooks), faded where it nears the
// opaque surface behind it (soft particles) and hidden behind it (there is no depth attachment).
// Writes colour and alpha to the colour target and only alpha to the ambient target, so alpha
// blending also covers the ambient light behind and additive blending leaves it alone.

const float k_minAlpha = 1.0 / 255.0;

layout(set = 2, binding = 0) uniform sampler2D sceneDepth;
layout(set = 2, binding = 3) uniform sampler2D sprite;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec4 inUvs;
layout(location = 2) in vec2 inFrameBlendSoftness;
layout(location = 3) in float inViewDepth;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAmbient;

void main()
{
    float depth = texelFetch(sceneDepth, ivec2(gl_FragCoord.xy), 0).r;
    float surfaceDepth = abs(frame.sceneDepthParams.y / (depth + frame.sceneDepthParams.x));
    float gap = surfaceDepth - inViewDepth;

    if (gap <= 0.0)
    {
        discard;
    }

    float softness = inFrameBlendSoftness.y;
    float fade = softness > 0.0 ? clamp(gap / softness, 0.0, 1.0) : 1.0;
    vec4 texel = mix(texture(sprite, inUvs.xy), texture(sprite, inUvs.zw), inFrameBlendSoftness.x);
    float alpha = inColor.a * texel.a * fade;

    if (alpha < k_minAlpha)
    {
        discard;
    }

    outColor = vec4(inColor.rgb * texel.rgb, alpha);
    outAmbient = vec4(0.0, 0.0, 0.0, alpha);
}
