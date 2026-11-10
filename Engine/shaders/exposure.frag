#version 450

// Auto exposure, step 3: the adapted exposure in stops (EV). Averages the reduced luminance grid,
// maps it to the target luminance within the limits and moves last frame's value towards it.
// Writes (adapted EV, target EV, 0, 1); alpha 0 in the history marks it unset.

layout(push_constant) uniform ExposureConstants
{
    vec4 target;
    vec4 adaptation;
} constants;

layout(set = 2, binding = 0) uniform sampler2D reduced;
layout(set = 2, binding = 1) uniform sampler2D history;

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(reduced, 0));
    vec2 sum = vec2(0.0);

    for (float y = 1.0; y < float(textureSize(reduced, 0).y); y += 2.0)
    {
        for (float x = 1.0; x < float(textureSize(reduced, 0).x); x += 2.0)
        {
            sum += textureLod(reduced, vec2(x, y) * texel, 0.0).rg;
        }
    }

    float averageLog = sum.x / max(sum.y, 1e-4);
    float targetEv = clamp(constants.target.x - averageLog, constants.target.y, constants.target.z);
    vec4 previous = texelFetch(history, ivec2(0), 0);

    if (previous.a < 0.5 || constants.adaptation.w > 0.5)
    {
        outColor = vec4(targetEv, targetEv, 0.0, 1.0);
        return;
    }

    float rate = targetEv > previous.r ? constants.adaptation.x : constants.adaptation.y;
    float blend = 1.0 - exp(-constants.adaptation.z * rate);

    outColor = vec4(mix(previous.r, targetEv, blend), targetEv, 0.0, 1.0);
}
