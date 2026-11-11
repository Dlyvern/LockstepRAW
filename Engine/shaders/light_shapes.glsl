// Shapes of local lights beyond a point: emission profiles (LightProfile, an atlas of angular
// profiles around the light's axis, rows by (1 - cos(angle from the axis)) / 2) and tube lights (line lights: diffuse integrated over the
// line, specular from the point on the line nearest the reflection ray). Include after shadeLight().

layout(set = 1, binding = 16) uniform sampler2D lightProfileAtlas;

const float k_lightProfileCount = 4.0;

// How much of its peak intensity a light sends towards `travel` (unit, from the light outwards).
float lightProfileFactor(LocalLight light, vec3 travel)
{
    if (light.profile.w < 0.5)
    {
        return 1.0;
    }

    vec3 axis = light.directionCosOuter.xyz;
    vec3 side = light.profile.xyz;
    vec3 across = cross(axis, side);
    float cosTheta = clamp(dot(travel, axis), -1.0, 1.0);
    float phi = atan(dot(travel, across), dot(travel, side));
    vec2 uv = vec2(phi * (0.5 / k_pi) + 0.5, 0.5 - 0.5 * cosTheta);
    float row = light.profile.w - 1.0;
    uv.y = (row + clamp(uv.y, 0.5 / 64.0, 1.0 - 0.5 / 64.0)) / k_lightProfileCount;

    return textureLod(lightProfileAtlas, uv, 0.0).r;
}

// Adds a tube light's diffuse and specular light. `scale` is everything but the line's own
// geometry (shadow, profile).
void shadeTubeLight(Surface surface, LocalLight light, vec3 position, float scale, inout vec3 diffuse,
    inout vec3 specular)
{
    vec3 centre = light.positionRange.xyz - position;
    vec3 halfTube = light.tube.xyz;
    vec3 toStart = centre - halfTube;
    vec3 toEnd = centre + halfTube;
    float startLength = length(toStart);
    float endLength = length(toEnd);
    float range = light.positionRange.w;

    vec3 segment = toEnd - toStart;
    float along = clamp(dot(-toStart, segment) / max(dot(segment, segment), 1e-6), 0.0, 1.0);
    float nearest = length(toStart + segment * along);
    float window = clamp(1.0 - pow(nearest / range, 4.0), 0.0, 1.0);
    window *= window;

    if (window <= 0.0)
    {
        return;
    }

    vec3 color = light.color.rgb * scale * window;
    float nDotStart = dot(surface.normal, toStart) / (2.0 * startLength);
    float nDotEnd = dot(surface.normal, toEnd) / (2.0 * endLength);
    float irradiance = 2.0 * max(nDotStart + nDotEnd, 0.0) / (startLength * endLength + dot(toStart, toEnd) + 0.02);

    if (irradiance > 0.0)
    {
        if (g_shadingModel == 0.0 && !g_isReliefDecal)
        {
            diffuse += surface.diffuseColor * color * irradiance;
        }
        else
        {
            vec3 diffuseDirection = normalize(toStart / startLength + toEnd / endLength + surface.normal * 1e-3);
            float nDotL = max(dot(surface.normal, diffuseDirection), 0.05);
            vec3 unusedSpecular = vec3(0.0);
            shadeLight(surface, diffuseDirection, color * (irradiance / nDotL), diffuse, unusedSpecular);
        }
    }

    if (frame.counts.y == 0u)
    {
        return;
    }

    vec3 reflected = reflect(-surface.toCamera, surface.normal);
    float reflectedSegment = dot(reflected, segment);
    float t = (dot(reflected, toStart) * reflectedSegment - dot(toStart, segment))
        / max(dot(segment, segment) - reflectedSegment * reflectedSegment, 1e-6);
    vec3 closest = toStart + segment * clamp(t, 0.0, 1.0);
    float closestLength = length(closest);
    vec3 toLight = closest / max(closestLength, 1e-4);
    float alpha = surface.roughness * surface.roughness;
    float widened = clamp(alpha + light.tube.w / (2.0 * max(closestLength, 0.05)), alpha, 1.0);
    float energy = alpha / widened;
    vec3 unusedDiffuse = vec3(0.0);
    shadeLight(surface, toLight, color * (energy / max(closestLength * closestLength, 0.01)), unusedDiffuse,
        specular);
}
