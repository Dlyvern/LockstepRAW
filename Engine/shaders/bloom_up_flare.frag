#version 450
#extension GL_GOOGLE_include_directive : require

// The last bloom upsample with the lens flares (lens_flare.frag) added to its colour.
#define BLOOM_UP_LENS_FLARE
#include "bloom_up.glsl"
