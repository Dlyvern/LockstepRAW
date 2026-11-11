// Shading models and per-character surface layers for pbr.frag: subsurface-scattered skin, fabric
// sheen, blood/dirt/rain on characters, and decal relief. Include after the BRDF helpers
// (distributionGgx, visibilitySmithGgxCorrelated, fresnelSchlick), the Surface struct, the outputs
// and surface_buffer.glsl.
//
// draw.shading: x the shading model (0 standard, 1 skin, 2 fabric), y its strength (skin: scatter
//               amount, fabric: sheen intensity; 0 when switched off), z skin: unused, fabric: sheen
//               roughness, decal draws: relief height in UV units (negative dents), w the draw's
//               character surface (bit pattern: 0 none, else first vec4 in characterSurfaces + 1).
//
// A character surface (MaterialShading.cpp, CharacterSurface.hpp): vec4 header (x rain soak,
// y overall grime, z stain count, w noise seed), then per stain two vec4: (bind-pose position,
// radius) and (blood, freshness, dirt, seed). Positions are in the skinned mesh's bind pose, which
// the vertex shader passes as inObjectPosition, so stains stick to the moving body.

layout(std430, set = 1, binding = 6) readonly buffer CharacterSurfaces
{
    vec4 values[];
} characterSurfaces;

const float k_skinShading = 1.0;
const float k_fabricShading = 2.0;
const vec3 k_skinScatterWrap = vec3(0.4, 0.14, 0.08);
const vec3 k_skinNormalBlur = vec3(0.6, 0.3, 0.15);
const vec3 k_skinBackScatter = vec3(0.2, 0.04, 0.02);
const float k_skinCurvatureScale = 0.012;
const vec3 k_freshBlood = vec3(0.075, 0.004, 0.003);
const vec3 k_driedBlood = vec3(0.028, 0.009, 0.006);
const vec3 k_dirtTint = vec3(0.32, 0.27, 0.21);
const float k_freshBloodRoughness = 0.16;
const float k_driedBloodRoughness = 0.62;
const float k_dirtRoughness = 0.92;
const float k_soakDarkening = 0.55;
const float k_soakGloss = 0.6;
const float k_stainNoiseScale = 14.0;
const float k_dripStretch = 0.45;
const float k_maxRelief = 4.0;

float g_shadingModel = 0.0;
float g_skinWeight = 0.0;
vec3 g_skinGeometryNormal = vec3(0.0, 1.0, 0.0);
vec3 g_skinWrap = vec3(0.0);
vec3 g_sheenColor = vec3(0.0);
float g_sheenRoughness = 0.5;
bool g_isReliefDecal = false;
vec3 g_reliefGeometryNormal = vec3(0.0, 1.0, 0.0);
vec3 g_reliefBumped = vec3(0.0);
vec3 g_reliefFlat = vec3(0.0);

float stainHash(vec3 point)
{
    point = fract(point * 0.3183099 + 0.1);
    point *= 17.0;
    return fract(point.x * point.y * point.z * (point.x + point.y + point.z));
}

float stainNoise(vec3 point)
{
    vec3 cell = floor(point);
    vec3 f = fract(point);
    vec3 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(mix(stainHash(cell), stainHash(cell + vec3(1.0, 0.0, 0.0)), u.x),
                   mix(stainHash(cell + vec3(0.0, 1.0, 0.0)), stainHash(cell + vec3(1.0, 1.0, 0.0)), u.x), u.y),
               mix(mix(stainHash(cell + vec3(0.0, 0.0, 1.0)), stainHash(cell + vec3(1.0, 0.0, 1.0)), u.x),
                   mix(stainHash(cell + vec3(0.0, 1.0, 1.0)), stainHash(cell + vec3(1.0, 1.0, 1.0)), u.x), u.y), u.z);
}

// How likely a base colour is human skin: warm, fairly bright, clearly redder than green. Keeps the
// skin model off the gear, hair and eyes that share a face texture atlas.
float skinLikelihood(vec3 linearColor)
{
    vec3 display = sqrt(max(linearColor, vec3(0.0)));
    float red = max(display.r, 1e-4);
    float redOverGreen = (display.r - display.g) / red;
    float redOverBlue = (display.r - display.b) / red;

    return smoothstep(0.19, 0.23, redOverGreen) * (1.0 - smoothstep(0.32, 0.4, redOverGreen))
        * smoothstep(0.26, 0.32, redOverBlue) * smoothstep(0.4, 0.55, display.r);
}

// Sets up this pixel's shading model. Call once the material's base colour and the geometry and
// shading normals are known.
// baseColor:      linear base colour (after any surface layers).
// metallic:       metalness (metal parts of a fabric material get no sheen).
// geometryNormal: interpolated normal facing the drawn side.
// position:       world position (skin curvature).
void prepareMaterialShading(vec3 baseColor, float metallic, vec3 geometryNormal, vec3 position)
{
    g_shadingModel = draw.shading.y > 0.0 ? draw.shading.x : 0.0;

    if (g_shadingModel == k_skinShading)
    {
        g_skinWeight = skinLikelihood(baseColor) * clamp(draw.shading.y, 0.0, 2.0);

        if (g_skinWeight <= 0.0)
        {
            g_shadingModel = 0.0;
            return;
        }

        float curvature = clamp(length(fwidth(geometryNormal)) / max(length(fwidth(position)), 1e-5)
            * k_skinCurvatureScale, 0.0, 1.0);
        g_skinGeometryNormal = geometryNormal;
        g_skinWrap = k_skinScatterWrap * g_skinWeight * mix(0.6, 1.0, curvature);
        return;
    }

    if (g_shadingModel == k_fabricShading)
    {
        vec3 tint = baseColor / max(max(baseColor.r, max(baseColor.g, baseColor.b)), 1e-3);
        g_sheenColor = mix(vec3(1.0), tint, 0.75) * 0.2 * draw.shading.y * (1.0 - metallic);
        g_sheenRoughness = clamp(draw.shading.z, 0.2, 1.0);
    }
}

float charlieDistribution(float nDotH, float roughness)
{
    float inverseAlpha = 1.0 / (roughness * roughness);
    float sin2 = max(1.0 - nDotH * nDotH, 0.0078125);

    return (2.0 + inverseAlpha) * pow(sin2, inverseAlpha * 0.5) / (2.0 * k_pi);
}

float neubeltVisibility(float nDotV, float nDotL)
{
    return clamp(1.0 / (4.0 * (nDotL + nDotV - nDotL * nDotV)), 0.0, 1.0);
}

// One light on a non-standard shading model, added to `diffuse` and `specular`. Returns true when
// it replaced the standard shading (skin), false when the standard shading still runs (fabric adds
// its sheen on top; relief decals note the light for their relief).
bool shadeMaterialLight(Surface surface, vec3 toLight, vec3 lightColor, float nDotL, inout vec3 diffuse,
    inout vec3 specular)
{
    if (g_isReliefDecal)
    {
        g_reliefBumped += lightColor * max(nDotL, 0.0);
        g_reliefFlat += lightColor * max(dot(g_reliefGeometryNormal, toLight), 0.0);
    }

    if (g_shadingModel == k_fabricShading && nDotL > 0.0)
    {
        vec3 halfVector = normalize(toLight + surface.toCamera);
        float nDotH = max(dot(surface.normal, halfVector), 0.0);
        specular += k_pi * charlieDistribution(nDotH, g_sheenRoughness) * neubeltVisibility(surface.nDotV, nDotL)
            * g_sheenColor * lightColor * nDotL;
        return false;
    }

    if (g_shadingModel != k_skinShading)
    {
        return false;
    }

    vec3 blurredNormalDot = vec3(dot(normalize(mix(surface.normal, g_skinGeometryNormal, k_skinNormalBlur.r)), toLight),
        dot(normalize(mix(surface.normal, g_skinGeometryNormal, k_skinNormalBlur.g)), toLight),
        dot(normalize(mix(surface.normal, g_skinGeometryNormal, k_skinNormalBlur.b)), toLight));
    vec3 wrapped = clamp((blurredNormalDot + g_skinWrap) / (1.0 + g_skinWrap), 0.0, 1.0);
    vec3 scattered = mix(vec3(max(nDotL, 0.0)), wrapped, min(g_skinWeight, 1.0));
    float behind = clamp(-dot(surface.toCamera, toLight), 0.0, 1.0);
    scattered += k_skinBackScatter * g_skinWeight * behind * behind * clamp(1.0 - abs(nDotL), 0.0, 1.0);

    vec3 diffuseTerm = surface.diffuseColor;

    if (nDotL > 0.0 && frame.counts.y != 0u)
    {
        vec3 halfVector = normalize(toLight + surface.toCamera);
        float nDotH = max(dot(surface.normal, halfVector), 0.0);
        float vDotH = max(dot(surface.toCamera, halfVector), 0.0);
        float alpha = surface.roughness * surface.roughness;
        vec3 fresnel = fresnelSchlick(surface.f0, vDotH);
        specular += k_pi * distributionGgx(nDotH, alpha) * visibilitySmithGgxCorrelated(surface.nDotV, nDotL, alpha)
            * fresnel * lightColor * nDotL;
        diffuseTerm *= 1.0 - fresnel;
    }

    diffuse += diffuseTerm * lightColor * scattered;
    return true;
}

// Ambient sheen of fabric: the sheen lobe lit by the ambient irradiance (none for other models).
// irradiance: the diffuse ambient light reaching the surface (before albedo), occlusion included.
vec3 materialAmbient(Surface surface, vec3 irradiance)
{
    if (g_shadingModel != k_fabricShading)
    {
        return vec3(0.0);
    }

    float grazing = 1.0 - surface.nDotV;
    return g_sheenColor * irradiance * (0.35 + 0.65 * grazing * grazing) * (1.0 - 0.5 * g_sheenRoughness);
}

// Blood, dirt and rain on a character's body (a draw with a character surface). Changes the
// material in place. Returns the rain soak used (the world wetness is skipped for such draws).
// objectPosition: bind-pose position of the pixel.
// outdoor:        1 outdoors, 0 inside an interior zone.
float applyCharacterSurface(vec3 objectPosition, float outdoor, inout vec3 albedo, inout float roughness,
    inout float metallic)
{
    uint base = floatBitsToUint(draw.shading.w) - 1u;
    vec4 header = characterSurfaces.values[base];
    float soak = clamp(max(header.x, frame.wetness.x * outdoor), 0.0, 1.0);
    vec3 noisePoint = objectPosition * k_stainNoiseScale + header.w;
    float noise = stainNoise(noisePoint) * 0.65 + stainNoise(noisePoint * 2.7 + 5.3) * 0.35;

    float blood = 0.0;
    float freshness = 0.0;
    float dirt = header.y * smoothstep(0.35, 0.75, noise);
    uint count = uint(header.z);

    for (uint index = 0u; index < count; ++index)
    {
        vec4 shape = characterSurfaces.values[base + 1u + index * 2u];
        vec4 look = characterSurfaces.values[base + 2u + index * 2u];
        vec3 offset = objectPosition - shape.xyz;
        offset.y *= offset.y < 0.0 ? k_dripStretch : 1.0;
        float reach = length(offset) / max(shape.w, 1e-3) + (noise - 0.5) * 0.7;
        float core = 1.0 - smoothstep(0.55, 1.0, reach);
        float halo = 1.0 - smoothstep(0.8, 1.6, reach);
        float amount = core * look.x;

        if (amount > blood)
        {
            freshness = look.y;
        }

        blood = max(blood, amount);
        dirt = max(dirt, halo * look.z);
    }

    float porosity = (1.0 - metallic) * smoothstep(0.25, 0.7, roughness);
    albedo *= mix(1.0, k_soakDarkening, soak * porosity);
    roughness = mix(roughness, roughness * (1.0 - k_soakGloss), soak);

    albedo = mix(albedo, albedo * k_dirtTint * 2.2, clamp(dirt, 0.0, 1.0) * 0.8);
    roughness = mix(roughness, k_dirtRoughness, clamp(dirt, 0.0, 1.0) * (1.0 - soak) * 0.7);

    vec3 bloodColor = mix(k_driedBlood, k_freshBlood, freshness) * mix(1.0, 0.6, porosity);
    float bloodRoughness = mix(k_driedBloodRoughness, k_freshBloodRoughness, max(freshness, soak * 0.7));
    bloodRoughness = mix(bloodRoughness, max(bloodRoughness, 0.38), porosity * 0.6);
    albedo = mix(albedo, bloodColor, blood);
    roughness = mix(roughness, bloodRoughness, blood);
    metallic *= 1.0 - blood;

    return soak;
}

// Tilts a relief decal's shading normal by the slope of its coverage (base colour alpha): raised
// blood rims, dented bullet holes. Sets up the relief lighting delta (decalOutputs()).
// geometryNormal: the decal's interpolated normal.
// shadingNormal:  its normal-mapped normal, changed in place.
// position:       world position.
void prepareDecalRelief(vec3 geometryNormal, inout vec3 shadingNormal, vec3 position)
{
    float relief = draw.shading.z;

    if (relief == 0.0 || draw.flags.y < k_decalBlend - 0.5)
    {
        return;
    }

    vec2 texel = 1.0 / vec2(textureSize(baseColorTexture, 0));
    vec2 offset = max(texel, max(abs(g_surfaceUvDx), abs(g_surfaceUvDy)) * 0.5);
    float centre = surfaceTexture(baseColorTexture).a;
    float alongU = textureGrad(baseColorTexture, g_surfaceUv + vec2(offset.x, 0.0), g_surfaceUvDx, g_surfaceUvDy).a;
    float alongV = textureGrad(baseColorTexture, g_surfaceUv + vec2(0.0, offset.y), g_surfaceUvDx, g_surfaceUvDy).a;
    vec2 slope = vec2(alongU - centre, alongV - centre) / offset * relief;
    slope = clamp(slope, vec2(-k_maxRelief), vec2(k_maxRelief));

    mat3 basis;

    if (!uvGradientFrame(geometryNormal, dFdx(position), dFdy(position), basis))
    {
        return;
    }

    vec3 tangentU = normalize(basis[0] - geometryNormal * dot(basis[0], geometryNormal));
    vec3 tangentV = normalize(cross(geometryNormal, tangentU));
    tangentV *= sign(dot(tangentV, basis[1]) + 1e-6);
    vec3 tilted = shadingNormal - tangentU * slope.x - tangentV * slope.y;
    shadingNormal = normalize(tilted);
    g_isReliefDecal = true;
    g_reliefGeometryNormal = geometryNormal;
}

// Writes a decal draw's outputs. Coverage is the base colour alpha; a relief decal also shades the
// uncovered surface below it (darkens what its relief turns away from the lights) and writes its
// normal and roughness into the G-buffer, so screen-space effects see the dent or the wet blood.
// color, ambient, ambientSpecular: the decal's lit colour, ambient and environment specular.
// coverage:   base colour alpha.
// viewNormal: the decal's shading normal in view space.
// roughness:  its roughness.
void decalOutputs(vec3 color, vec3 ambient, vec3 ambientSpecular, float coverage, vec3 viewNormal, float roughness)
{
    float blend = coverage;

    if (g_isReliefDecal)
    {
        float flatLight = dot(g_reliefFlat, vec3(0.2126, 0.7152, 0.0722));
        float bumped = dot(g_reliefBumped, vec3(0.2126, 0.7152, 0.0722));
        float shade = flatLight > 1e-4 ? clamp(bumped / flatLight, 0.0, 1.0) : 1.0;
        float lightAmount = clamp(flatLight * 4.0, 0.0, 1.0);
        shade = mix(1.0, shade, lightAmount);
        blend = 1.0 - (1.0 - coverage) * shade;
    }

    float scale = blend > 1e-4 ? coverage / blend : 0.0;
    outColor = vec4(color * scale, blend);
    outAmbient = vec4(ambient * scale, blend);
    outSurfaceSpecular = vec4(ambientSpecular * scale, blend);
    outSurfaceNormal = g_isReliefDecal ? vec4(encodeOctahedral(viewNormal), roughness, blend) : vec4(0.0);
}
