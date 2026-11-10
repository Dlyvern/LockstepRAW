#version 450

// Auto exposure, step 2: averages blocks of the luminance grid with bilinear taps that each cover
// 2x2 texels.

layout(push_constant) uniform ReduceConstants
{
    vec4 params;
} constants;

layout(set = 2, binding = 0) uniform sampler2D source;

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 sourceTexel = 1.0 / vec2(textureSize(source, 0));
    float block = constants.params.x;
    vec2 origin = floor(gl_FragCoord.xy) * block;
    vec2 sum = vec2(0.0);
    float taps = 0.0;

    for (float y = 1.0; y < block; y += 2.0)
    {
        for (float x = 1.0; x < block; x += 2.0)
        {
            sum += textureLod(source, (origin + vec2(x, y)) * sourceTexel, 0.0).rg;
            taps += 1.0;
        }
    }

    outColor = vec4(sum / taps, 0.0, 1.0);
}
