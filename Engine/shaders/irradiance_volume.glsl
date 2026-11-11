// Baked bounce light: trilinear lookup into the irradiance volume (Renderer::setIrradianceVolume()).
// Each texture holds order-1 spherical harmonics convolved with the cosine lobe ({a, b} per texel,
// irradiance / pi = a + dot(b, n)) of one channel: red, green, blue bounce light, and the sky
// visibility. Probe slices along z are stacked vertically (height = probes y * probes z), so
// filtering along z takes two bilinear lookups. Requires frame_lighting.glsl.

layout(set = 1, binding = 17) uniform sampler2D irradianceRed;
layout(set = 1, binding = 18) uniform sampler2D irradianceGreen;
layout(set = 1, binding = 19) uniform sampler2D irradianceBlue;
layout(set = 1, binding = 20) uniform sampler2D irradianceSky;

// The volume at one point.
// bounce:     rgb harmonics, one vec4 per channel.
// sky:        the sky visibility harmonic.
// weight:     0 outside the volume, 1 inside past the fade distance.
struct IrradianceSample
{
    vec4 red;
    vec4 green;
    vec4 blue;
    vec4 sky;
    float weight;
};

IrradianceSample sampleIrradianceVolume(vec3 position, vec3 normal)
{
    IrradianceSample result;
    result.red = vec4(0.0);
    result.green = vec4(0.0);
    result.blue = vec4(0.0);
    result.sky = vec4(1.0, 0.0, 0.0, 0.0);
    result.weight = 0.0;

    if (frame.irradianceParams.x < 0.5)
    {
        return result;
    }

    vec3 counts = frame.irradianceCounts.xyz;
    vec3 probe = (position + normal * frame.irradianceParams.z - frame.irradianceOrigin.xyz) * frame.irradianceScale.xyz;
    vec3 inside = min(probe, counts - 1.0 - probe) / frame.irradianceScale.xyz;
    float edge = min(inside.x, min(inside.y, inside.z));
    result.weight = clamp(edge / max(frame.irradianceOrigin.w, 1e-3) + 0.5, 0.0, 1.0);

    if (result.weight <= 0.0)
    {
        return result;
    }

    probe = clamp(probe, vec3(0.0), counts - 1.0);
    float slice = min(floor(probe.z), counts.z - 2.0);
    float blend = probe.z - slice;
    float height = counts.y * counts.z;
    vec2 uv = vec2((probe.x + 0.5) / counts.x, (probe.y + 0.5 + slice * counts.y) / height);
    vec2 nextUv = uv + vec2(0.0, counts.y / height);

    result.red = mix(textureLod(irradianceRed, uv, 0.0), textureLod(irradianceRed, nextUv, 0.0), blend);
    result.green = mix(textureLod(irradianceGreen, uv, 0.0), textureLod(irradianceGreen, nextUv, 0.0), blend);
    result.blue = mix(textureLod(irradianceBlue, uv, 0.0), textureLod(irradianceBlue, nextUv, 0.0), blend);
    result.sky = mix(textureLod(irradianceSky, uv, 0.0), textureLod(irradianceSky, nextUv, 0.0), blend);

    return result;
}

// Bounce light (irradiance / pi, times the volume's intensity) arriving at a surface facing `normal`.
vec3 irradianceBounce(IrradianceSample volume, vec3 normal)
{
    vec4 direction = vec4(1.0, normal);

    return max(vec3(dot(volume.red, direction), dot(volume.green, direction), dot(volume.blue, direction)),
        vec3(0.0)) * frame.irradianceParams.y;
}

// Multiplier on the sky's diffuse light at a surface facing `normal`: the share of the sky it
// sees, blended in by the volume's weight and the visibility strength.
float irradianceSkyFactor(IrradianceSample volume, vec3 normal)
{
    float visible = clamp(dot(volume.sky, vec4(1.0, normal)), 0.0, 1.0);

    return mix(1.0, visible, frame.irradianceCounts.w);
}
