// The per-frame lighting block (std430). Must match GpuFrameLighting in FrameLighting.hpp.
// Define FRAME_LIGHTING_BINDING (the set-1 binding) before including this file.

const uint k_maxLocalLights = 16u;
const uint k_maxShadowTiles = 20u;
const uint k_maxAmbientZones = 16u;
const float k_spotLight = 1.0;

struct LocalLight
{
    vec4 positionRange;
    vec4 color;
    vec4 directionCosOuter;
    vec4 cosInnerKind;
};

struct AmbientZone
{
    vec4 minimum;
    vec4 maximum;
};

layout(set = 1, binding = FRAME_LIGHTING_BINDING, std430) readonly buffer FrameLighting
{
    mat4 shadowTileViewProjection[k_maxShadowTiles];
    vec4 shadowTileRects[k_maxShadowTiles];
    mat4 viewProjection;
    mat4 previousViewProjection;
    mat4 reflectionViewProjection;
    mat4 viewModelViewProjection;
    mat4 viewModelMotionViewProjection;
    mat4 previousViewModelMotionViewProjection;
    vec4 reflectionCamera;
    vec4 reflectionPlane;
    vec4 reflectionTint;
    vec4 cascadeTexelSize;
    vec4 ambient;
    vec4 sunDirection;
    vec4 sunRadiance;
    vec4 fogColor;
    vec4 fogParams;
    vec4 shadowParams;
    vec4 localShadowParams;
    vec4 iblParams;
    vec4 skyA;
    vec4 skyB;
    vec4 skyC;
    vec4 skyD;
    vec4 skyE;
    vec4 skyZenith;
    vec4 skyGround;
    vec4 skyGlow;
    vec4 skyGlowShape;
    vec4 skyZenithTint;
    vec4 sunDisk;
    vec4 sceneDepthParams;
    vec4 surfaceParallax;
    vec4 surfaceDetail;
    vec4 wetness;
    vec4 wetnessShading;
    uvec4 counts;
    LocalLight lights[k_maxLocalLights];
    vec4 zoneParams;
    vec4 zoneLight;
    vec4 zoneFill;
    AmbientZone zones[k_maxAmbientZones];
} frame;
