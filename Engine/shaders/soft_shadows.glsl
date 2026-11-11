// Contact-hardening soft shadows (percentage-closer soft shadows) for the shadow atlas tiles: a
// blocker search over raw depths (shadowDepth, set 2 binding 2) estimates how far the casters
// are, the penumbra widens with that distance, and a rotated disk of hardware-compared taps
// filters it. Requires pbr.frag's shadowAtlas, shadowDepth and filteredShadow().

const float k_goldenAngle = 2.39996323;

vec2 vogelDisk(int index, int count, float rotation)
{
    float radius = sqrt((float(index) + 0.5) / float(count));
    float angle = float(index) * k_goldenAngle + rotation;

    return radius * vec2(cos(angle), sin(angle));
}

float shadowNoise()
{
    return fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
}

// Metres between the receiver and the average blocker, from atlas depths of a tile.
float blockerDistance(uint tile, float receiverDepth, float blockerDepth)
{
    vec4 depth = frame.shadowTileDepth[tile];

    if (depth.x > 0.5)
    {
        return (receiverDepth - blockerDepth) * depth.y;
    }

    float near = depth.y;
    float far = depth.z;
    float receiver = near * far / (far - receiverDepth * (far - near));
    float blocker = near * far / (far - blockerDepth * (far - near));

    return receiver - blocker;
}

// Lit fraction of a world position in one tile with a contact-hardening filter, or -1 when it falls
// outside the tile. `lightSize` is the sun's penumbra metres per metre (orthographic tiles) or the
// light's radius in metres (perspective tiles).
float tileShadowSoft(uint tile, vec3 position, float lightSize)
{
    vec4 clip = frame.shadowTileViewProjection[tile] * vec4(position, 1.0);

    if (clip.w <= 0.0)
    {
        return -1.0;
    }

    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    vec4 rect = frame.shadowTileRects[tile];
    vec2 local = (uv - rect.xy) / rect.zw;
    vec2 atlasSize = vec2(textureSize(shadowAtlas, 0));
    float tileTexels = rect.z * atlasSize.x;
    vec2 edges = min(local, 1.0 - local) * tileTexels;
    float room = min(edges.x, edges.y) - 2.0;

    if (room < 1.0 || ndc.z >= 1.0)
    {
        return -1.0;
    }

    vec4 depth = frame.shadowTileDepth[tile];
    float maxTexels = min(frame.localShadowSoftness.y, room);
    float searchTexels = maxTexels;
    vec2 texel = 1.0 / atlasSize;
    float rotation = shadowNoise() * 6.2831853;
    int blockerTaps = int(frame.shadowSoftness.y);
    float blockerSum = 0.0;
    float blockers = 0.0;

    for (int index = 0; index < blockerTaps; ++index)
    {
        vec2 offset = vogelDisk(index, blockerTaps, rotation) * searchTexels * texel;
        float sampled = textureLod(shadowDepth, uv + offset, 0.0).r;

        if (sampled < ndc.z)
        {
            blockerSum += sampled;
            blockers += 1.0;
        }
    }

    if (blockers < 0.5)
    {
        return 1.0;
    }

    if (blockers > float(blockerTaps) - 0.5)
    {
        return 0.0;
    }

    float distanceToBlocker = max(blockerDistance(tile, ndc.z, blockerSum / blockers), 0.0);
    float penumbraTexels;

    if (depth.x > 0.5)
    {
        penumbraTexels = distanceToBlocker * lightSize * depth.w;
    }
    else
    {
        float receiver = depth.y * depth.z / (depth.z - ndc.z * (depth.z - depth.y));
        float blocker = max(receiver - distanceToBlocker, depth.y);
        penumbraTexels = lightSize * (distanceToBlocker / blocker) * depth.w / receiver;
    }

    penumbraTexels = clamp(penumbraTexels, 1.0, maxTexels);

    if (penumbraTexels <= 1.5)
    {
        return filteredShadow(uv, ndc.z, 1.0);
    }

    int filterTaps = int(frame.shadowSoftness.z);
    float lit = 0.0;

    for (int index = 0; index < filterTaps; ++index)
    {
        vec2 offset = vogelDisk(index, filterTaps, rotation) * penumbraTexels * texel;
        lit += texture(shadowAtlas, vec3(uv + offset, ndc.z));
    }

    return lit / float(filterTaps);
}
