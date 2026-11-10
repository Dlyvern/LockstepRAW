// View-space reconstruction from the scene depth buffer for half/full resolution screen passes.
// `projection` is (P[0][0], P[1][1], P[2][2], P[3][2]) of the camera projection (sub-pixel
// jitter ignored). View space looks down -z; uv (0, 0) is the top-left corner.

float viewDepthFromBuffer(float depth, vec4 projection)
{
    return -projection.w / (depth + projection.z);
}

vec3 viewPositionFromBuffer(vec2 uv, float depth, vec4 projection)
{
    float z = viewDepthFromBuffer(depth, projection);
    vec2 ndc = uv * 2.0 - 1.0;

    return vec3(ndc.x * -z / projection.x, ndc.y * -z / projection.y, z);
}

vec2 projectViewToUv(vec3 position, vec4 projection)
{
    vec2 ndc = vec2(position.x * projection.x, position.y * projection.y) / -position.z;
    return ndc * 0.5 + 0.5;
}

float interleavedGradientNoise(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}
