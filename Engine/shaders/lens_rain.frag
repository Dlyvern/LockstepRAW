#version 450

// A rain drop on the lens over the finished image (LensRainSettings): a small inverted, blurred
// view of the image around it (a drop is a tiny lens), a dark rim where it bends light away, and a
// highlight. Alpha-blended; faded with the screen fade like the image below.

const float k_edge = 0.82;
const float k_rimStart = 0.5;
const float k_lensReach = 0.9;
const vec3 k_highlightDirection = vec3(-0.35, -0.55, 0.76);

layout(push_constant) uniform LensRainConstants
{
    vec4 params;
    vec4 fade;
    vec4 options;
} constants;

layout(set = 2, binding = 0) uniform sampler2D source;
layout(set = 2, binding = 1) uniform sampler2D temporal;

layout(location = 0) in vec2 inLocal;
layout(location = 1) in vec4 inDrop;
layout(location = 2) in float inOpacity;

layout(location = 0) out vec4 outColor;

vec3 image(vec2 uv)
{
    uv = clamp(uv, vec2(0.001), vec2(0.999));
    return constants.options.x > 0.5 ? texture(temporal, uv).rgb : texture(source, uv).rgb;
}

void main()
{
    float aspectRatio = constants.params.x;
    float refraction = constants.params.y;
    float blur = constants.params.z;
    float stretch = inDrop.w;
    vec2 local = inLocal;
    local.x /= 1.0 + 0.3 * (stretch - 1.0) * clamp(local.y, -1.0, 1.0);
    float distance = length(local);

    if (distance >= 1.0)
    {
        discard;
    }

    float height = sqrt(max(1.0 - distance * distance, 0.0));
    vec3 normal = normalize(vec3(local, height * 1.3));
    vec2 radiusUv = vec2(inDrop.z / aspectRatio, inDrop.z);
    vec2 centre = inDrop.xy - local * radiusUv * k_lensReach * refraction;
    vec2 spread = radiusUv * (0.15 + 0.6 * blur);
    vec3 color = (image(centre) * 2.0 + image(centre + vec2(spread.x, 0.0)) + image(centre - vec2(spread.x, 0.0))
        + image(centre + vec2(0.0, spread.y)) + image(centre - vec2(0.0, spread.y))) / 6.0;

    float rim = smoothstep(k_rimStart, 1.0, distance);
    color *= mix(1.08, 0.3, rim);
    float highlight = pow(max(dot(normal, normalize(k_highlightDirection)), 0.0), 60.0);
    color += vec3(highlight * 0.5);

    float alpha = (1.0 - smoothstep(k_edge, 1.0, distance)) * inOpacity * constants.params.w;
    color = mix(color, constants.fade.rgb, constants.fade.w);
    outColor = vec4(color, alpha);
}
