// Octahedral mapping of unit directions (+Y up) to [0,1]^2 and back, and lookups into the
// environment atlas: k_environmentTiles square tiles side by side, tiles 0..k_specularLevels-1 hold
// the sky prefiltered for increasing roughness, the last tile holds diffuse irradiance / pi.
// Must match FrameAttachments.hpp.

const float k_environmentTileSize = 64.0;
const float k_environmentTiles = 7.0;
const float k_specularLevels = 6.0;
const float k_irradianceTile = 6.0;

vec2 signNotZero(vec2 value)
{
    return vec2(value.x >= 0.0 ? 1.0 : -1.0, value.y >= 0.0 ? 1.0 : -1.0);
}

vec2 octahedralEncode(vec3 direction)
{
    vec3 n = direction / (abs(direction.x) + abs(direction.y) + abs(direction.z));
    vec2 folded = n.y >= 0.0 ? n.xz : (1.0 - abs(n.zx)) * signNotZero(n.xz);

    return folded * 0.5 + 0.5;
}

vec3 octahedralDecode(vec2 uv)
{
    vec2 folded = uv * 2.0 - 1.0;
    vec3 n = vec3(folded.x, 1.0 - abs(folded.x) - abs(folded.y), folded.y);
    float t = clamp(-n.y, 0.0, 1.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.z += n.z >= 0.0 ? -t : t;

    return normalize(n);
}

vec2 environmentAtlasUv(float tile, vec3 direction)
{
    float inset = 0.5 / k_environmentTileSize;
    vec2 local = clamp(octahedralEncode(direction), vec2(inset), vec2(1.0 - inset));

    return vec2((tile + local.x) / k_environmentTiles, local.y);
}
