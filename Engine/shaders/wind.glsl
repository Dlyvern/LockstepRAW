// Wind sway of alpha-masked foliage in the vertex shader (WindSettings). Requires
// frame_lighting.glsl: frame.wind / frame.windPrevious hold xy the unit direction the wind blows
// towards (x, z), z the speed times the sway multiplier (0: off), w the animation seconds.

const float k_windBendPerMetre = 0.0012;
const float k_windFlutter = 0.0045;
const float k_windMaxBend = 0.15;

bool isInsideAmbientZone(vec3 position)
{
    uint zoneCount = uint(frame.zoneParams.x);

    for (uint index = 0u; index < zoneCount; ++index)
    {
        if (all(greaterThanEqual(position, frame.zones[index].minimum.xyz))
            && all(lessThanEqual(position, frame.zones[index].maximum.xyz)))
        {
            return true;
        }
    }

    return false;
}

// Offset of a foliage vertex at `worldPosition`, `height` metres above its mesh's origin at
// `origin`, for the wind block `wind`.
vec3 windOffset(vec4 wind, vec3 worldPosition, vec3 origin, float height)
{
    if (wind.z <= 0.0 || height <= 0.0)
    {
        return vec3(0.0);
    }

    float speed = wind.z;
    float time = wind.w;
    vec3 direction = vec3(wind.x, 0.0, wind.y);
    float phase = dot(origin.xz, vec2(0.37, 0.23));
    float gust = 0.65 + 0.25 * sin(time * 0.9 + phase) + 0.1 * sin(time * 2.3 + phase * 1.7);
    float bend = min(k_windBendPerMetre * speed * height * height * gust, k_windMaxBend);
    vec3 sway = direction * bend;
    sway.y -= 0.25 * bend * bend / max(height, 0.25);

    float leafPhase = dot(worldPosition, vec3(2.1, 1.7, 2.9));
    float flutter = k_windFlutter * speed * min(height, 3.0) * sin(time * (7.0 + 0.04 * speed) + leafPhase);
    vec3 side = vec3(-direction.z, 0.6, direction.x);

    return sway + side * flutter;
}
