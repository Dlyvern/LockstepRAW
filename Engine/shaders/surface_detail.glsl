// Close-up surface detail for pbr.frag: parallax occlusion mapping from a material's height map
// and the Renderer's tiling micro-detail layers. Include after the draw constants, the frame
// lighting block and the material's set-1 textures are declared.
//
// draw.detail:          x parallax height scale in UV units (0 off), y SurfaceDetail layer (0 none),
//                       z detail repeats per UV unit.
// frame.surfaceParallax: x fade-out distance (0 off), y fewest and z most steps, w 1 for self-shadow.
// frame.surfaceDetail:   x fade-out distance (0 off), y strength.
//
// surfacePrepare() sets the globals below once per pixel; surfaceTexture() then samples a material
// texture at the parallax-shifted coordinate with the unshifted derivatives (no mip seams along the
// relief's steps).

layout(set = 1, binding = 7) uniform sampler2D heightTexture;
layout(set = 1, binding = 8) uniform sampler2D detailMineral;
layout(set = 1, binding = 9) uniform sampler2D detailPaint;
layout(set = 1, binding = 10) uniform sampler2D detailMetal;
layout(set = 1, binding = 11) uniform sampler2D detailFibrous;

const float k_parallaxShadowSteps = 6.0;
const float k_minParallaxPixels = 0.75;
const float k_stepsPerPixel = 0.5;
const float k_parallaxShadowSoftness = 4.0;
const float k_detailRoughnessRange = 0.6;

vec2 g_surfaceUv;
vec2 g_surfaceUvDx;
vec2 g_surfaceUvDy;
mat3 g_parallaxFrame;
float g_parallaxScale = 0.0;
float g_parallaxHeight = 1.0;
vec2 g_detailNormal = vec2(0.0);
float g_detailRoughness = 0.0;
float g_detailAlbedo = 1.0;

vec4 surfaceTexture(sampler2D materialTexture)
{
    return textureGrad(materialTexture, g_surfaceUv, g_surfaceUvDx, g_surfaceUvDy);
}

float surfaceHeight(vec2 uv)
{
    return textureGrad(heightTexture, uv, g_surfaceUvDx, g_surfaceUvDy).r;
}

// Rows are the directions in which u and v grow along the surface, scaled together so the larger is
// unit length, and the normal: world offsets into UV offsets per unit of texture repeat. Built from
// screen derivatives, so it is correct whatever the mesh's tangents. False where UVs are degenerate.
bool uvGradientFrame(vec3 normal, vec3 dpdx, vec3 dpdy, out mat3 basis)
{
    vec3 dpdyPerp = cross(dpdy, normal);
    vec3 dpdxPerp = cross(normal, dpdx);
    vec3 gradientU = dpdyPerp * g_surfaceUvDx.x + dpdxPerp * g_surfaceUvDy.x;
    vec3 gradientV = dpdyPerp * g_surfaceUvDx.y + dpdxPerp * g_surfaceUvDy.y;
    float largest = max(dot(gradientU, gradientU), dot(gradientV, gradientV));

    if (largest < 1e-20)
    {
        basis = mat3(1.0);
        return false;
    }

    float scale = inversesqrt(largest);
    basis = transpose(mat3(gradientU * scale, gradientV * scale, normal));
    return true;
}

// Steps along the view ray below the surface until it meets the height field, then refines between
// the last two steps. Returns the UV where the ray hits and stores the height there.
// steps: samples along the ray; maxSteps: the loop bound (frame setting).
vec2 parallaxOcclusion(vec3 viewTangent, float steps, float maxSteps)
{
    float layer = 1.0 / steps;
    vec2 shift = -viewTangent.xy / max(viewTangent.z, 0.05) * g_parallaxScale * layer;
    vec2 uv = g_surfaceUv;
    float rayDepth = 0.0;
    float surfaceDepth = 1.0 - surfaceHeight(uv);
    vec2 previousUv = uv;
    float previousDifference = surfaceDepth;

    for (float index = 0.0; index < maxSteps && rayDepth < surfaceDepth; index += 1.0)
    {
        if (index >= steps)
        {
            break;
        }

        previousUv = uv;
        previousDifference = surfaceDepth - rayDepth;
        uv += shift;
        rayDepth += layer;
        surfaceDepth = 1.0 - surfaceHeight(uv);
    }

    float after = rayDepth - surfaceDepth;
    float blend = previousDifference / max(previousDifference + after, 1e-5);
    vec2 hit = mix(previousUv, uv, clamp(blend, 0.0, 1.0));
    g_parallaxHeight = surfaceHeight(hit);

    return hit;
}

vec4 detailTexel(float kind, vec2 uv, vec2 dx, vec2 dy)
{
    if (kind < 1.5)
    {
        return textureGrad(detailMineral, uv, dx, dy);
    }

    if (kind < 2.5)
    {
        return textureGrad(detailPaint, uv, dx, dy);
    }

    if (kind < 3.5)
    {
        return textureGrad(detailMetal, uv, dx, dy);
    }

    return textureGrad(detailFibrous, uv, dx, dy);
}

// Sets the parallax-shifted texture coordinate, its derivatives and the detail layer's offsets for
// this pixel.
// normal:     the interpolated geometric normal, facing the camera side that is drawn.
// position:   world position of the pixel.
// toCamera:   unit vector from the pixel to the camera.
// viewDistance: metres from the camera.
// allowParallax: false for views that skip it (mirrors).
void surfacePrepare(vec3 normal, vec3 position, vec3 toCamera, float viewDistance, bool allowParallax)
{
    g_surfaceUv = inUv;
    g_surfaceUvDx = dFdx(inUv);
    g_surfaceUvDy = dFdy(inUv);
    vec3 positionDx = dFdx(position);
    vec3 positionDy = dFdy(position);

    float parallaxDistance = frame.surfaceParallax.x;
    float parallaxFade = parallaxDistance > 0.0 ? 1.0 - smoothstep(parallaxDistance * 0.5, parallaxDistance, viewDistance)
        : 0.0;
    g_parallaxScale = allowParallax ? draw.detail.x * parallaxFade : 0.0;

    if (g_parallaxScale > 0.0 && uvGradientFrame(normal, positionDx, positionDy, g_parallaxFrame))
    {
        vec3 viewTangent = g_parallaxFrame * toCamera;

        float uvPerPixel = max(max(length(g_surfaceUvDx), length(g_surfaceUvDy)), 1e-7);
        float shiftPixels = g_parallaxScale * length(viewTangent.xy) / max(viewTangent.z, 0.05) / uvPerPixel;

        if (viewTangent.z > 0.0 && shiftPixels >= k_minParallaxPixels)
        {
            float steps = clamp(ceil(shiftPixels * k_stepsPerPixel), frame.surfaceParallax.y, frame.surfaceParallax.z);
            g_surfaceUv = parallaxOcclusion(viewTangent, steps, frame.surfaceParallax.z);
        }
        else
        {
            g_parallaxScale = 0.0;
        }
    }
    else
    {
        g_parallaxScale = 0.0;
    }

    float detailDistance = frame.surfaceDetail.x;

    if (draw.detail.y < 0.5 || detailDistance <= 0.0)
    {
        return;
    }

    float weight = frame.surfaceDetail.y * (1.0 - smoothstep(detailDistance * 0.5, detailDistance, viewDistance));

    if (weight <= 0.0)
    {
        return;
    }

    float scale = draw.detail.z;
    vec4 texel = detailTexel(draw.detail.y, g_surfaceUv * scale, g_surfaceUvDx * scale, g_surfaceUvDy * scale);
    g_detailNormal = (texel.xy * 2.0 - 1.0) * weight;
    g_detailRoughness = (texel.z - 0.5) * k_detailRoughnessRange * weight;
    g_detailAlbedo = 1.0 + (texel.w - 0.5) * weight;
}

// How much of a light reaches the parallax hit point past the relief between it and the light:
// soft, 1 without parallax or with self-shadowing off.
// toLight: unit vector from the pixel towards the light.
float parallaxSelfShadow(vec3 toLight)
{
    if (g_parallaxScale <= 0.0 || frame.surfaceParallax.w < 0.5)
    {
        return 1.0;
    }

    vec3 lightTangent = g_parallaxFrame * toLight;

    if (lightTangent.z <= 0.0)
    {
        return 1.0;
    }

    float above = 1.0 - g_parallaxHeight;

    if (above <= 0.0)
    {
        return 1.0;
    }

    vec2 reach = lightTangent.xy / max(lightTangent.z, 0.05) * g_parallaxScale * above;
    float blocked = 0.0;

    for (float index = 1.0; index <= k_parallaxShadowSteps; index += 1.0)
    {
        float t = index / k_parallaxShadowSteps;
        float rayHeight = g_parallaxHeight + above * t;
        float excess = surfaceHeight(g_surfaceUv + reach * t) - rayHeight;
        blocked = max(blocked, excess * k_parallaxShadowSoftness * (1.0 - t));
    }

    return 1.0 - clamp(blocked, 0.0, 1.0);
}
