// Preetham analytic daylight sky. Requires frame_lighting.glsl. The coefficients are computed on
// the CPU (SkyModel.cpp) from the sun elevation and turbidity.

vec3 perez(float cosTheta, float gamma, float cosGamma)
{
    vec3 a = frame.skyA.xyz;
    vec3 b = frame.skyB.xyz;
    vec3 c = frame.skyC.xyz;
    vec3 d = frame.skyD.xyz;
    vec3 e = frame.skyE.xyz;

    return (1.0 + a * exp(b / max(cosTheta, 0.01))) * (1.0 + c * exp(d * gamma) + e * cosGamma * cosGamma);
}

vec3 xyYToLinearRgb(vec3 yxy)
{
    float luminance = yxy.x;
    float x = yxy.y;
    float y = max(yxy.z, 1e-4);
    vec3 xyz = vec3(x / y * luminance, luminance, (1.0 - x - y) / y * luminance);

    return mat3(3.2406, -0.9689, 0.0557,
                -1.5372, 1.8758, -0.2040,
                -0.4986, 0.0415, 1.0570) * xyz;
}

// Sunset glow: a warm band along the horizon, brightest towards the sun's azimuth.
vec3 sunsetGlow(vec3 direction, vec3 toSun)
{
    float band = exp(-max(direction.y, 0.0) / frame.skyGlow.w);
    vec2 horizontal = direction.xz;
    vec2 sunHorizontal = toSun.xz;
    float lengths = dot(horizontal, horizontal) * dot(sunHorizontal, sunHorizontal);
    float facing = dot(horizontal, sunHorizontal) * inversesqrt(max(lengths, 1e-8));
    float towardSun = pow(clamp(0.5 + 0.5 * facing, 1e-4, 1.0), frame.skyGlowShape.x);

    return frame.skyGlow.rgb * band * mix(frame.skyGlowShape.y, 1.0, towardSun);
}

// Radiance arriving from `direction` (unit, pointing away from the viewer), without the sun disk.
vec3 skyRadiance(vec3 direction)
{
    vec3 toSun = -frame.sunDirection.xyz;
    float cosTheta = max(direction.y, 0.0);
    float cosGamma = clamp(dot(direction, toSun), -1.0, 1.0);
    float gamma = acos(cosGamma);
    vec3 yxy = frame.skyZenith.xyz * perez(cosTheta, gamma, cosGamma);
    vec3 overhead = mix(vec3(1.0), frame.skyZenithTint.rgb, smoothstep(0.0, 0.8, cosTheta));
    vec3 sky = max(xyYToLinearRgb(yxy), vec3(0.0)) * frame.skyZenith.w * overhead + sunsetGlow(direction, toSun);

    return mix(sky, frame.skyGround.rgb, smoothstep(0.0, 0.04, -direction.y));
}
