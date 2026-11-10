// Camera lens effects of the output pass (fxaa.frag), applied to the anti-aliased image before
// grain: depth of field, per-pixel motion blur along the TAA motion vectors, and chromatic
// aberration. Include after the output constants, the samplers and interleavedGradientNoise().
//
// constants.motionBlur:   x shutter share (0 off), y longest smear in pixels, z taps, w viewmodel scale.
// constants.depthOfField: x strength (0 off), y focus distance in metres (0 the screen centre),
//                         z sharp band as a share of the focus distance, w blur growth.
// constants.lens:         x largest blur radius in pixels, y disc taps, z red/blue separation at the
//                         corners in pixels (0 off).
// constants.depthParams:  x, y the projection's [2][2] and [3][2]; z the depth value below which a
//                         pixel is the viewmodel.

const float k_goldenAngle = 2.39996323;
const float k_maxDiscTaps = 48.0;
const float k_maxBlurTaps = 16.0;

vec3 lensSource(vec2 uv)
{
    if (constants.filterMode == k_filterTemporalSharpen)
    {
        return textureLod(temporal, uv, 0.0).rgb;
    }

    return textureLod(source, uv, 0.0).rgb;
}

float lensViewDepth(float depth)
{
    return abs(constants.depthParams.y / (depth + constants.depthParams.x));
}

bool isViewModelDepth(float depth)
{
    return depth < constants.depthParams.z;
}

float focusDistance()
{
    if (constants.depthOfField.y > 0.0)
    {
        return constants.depthOfField.y;
    }

    const vec2 taps[5] = vec2[](vec2(0.5, 0.5), vec2(0.47, 0.5), vec2(0.53, 0.5), vec2(0.5, 0.47), vec2(0.5, 0.53));
    float sum = 0.0;
    float count = 0.0;

    for (int index = 0; index < 5; ++index)
    {
        float depth = textureLod(depthTexture, taps[index], 0.0).r;

        if (!isViewModelDepth(depth) && depth < 1.0)
        {
            sum += lensViewDepth(depth);
            count += 1.0;
        }
    }

    return count > 0.0 ? sum / count : 1000.0;
}

// Blur radius in pixels of a surface at a view depth: grows with the relative distance from the
// focal plane once outside the sharp band.
float circleOfConfusion(float depth, float focus)
{
    if (isViewModelDepth(depth))
    {
        return 0.0;
    }

    float viewDepth = depth >= 1.0 ? 1e6 : lensViewDepth(depth);
    float away = abs(1.0 - focus / max(viewDepth, 1e-3));
    float blur = clamp((away - constants.depthOfField.z) * constants.depthOfField.w, 0.0, 1.0);

    return blur * constants.lens.x * constants.depthOfField.x;
}

// Disc gather: a tap counts when its own blur reaches this pixel; taps behind the pixel are limited
// to its blur so sharp foreground does not smear onto blurry background. Sharp pixels skip the
// gather (blurry foreground does not spill onto them; cheaper).
vec3 depthOfField(vec3 center, vec2 uv)
{
    float focus = focusDistance();
    float centerDepth = textureLod(depthTexture, uv, 0.0).r;
    float centerBlur = circleOfConfusion(centerDepth, focus);

    if (centerBlur < 0.5)
    {
        return center;
    }

    float radius = constants.lens.x * constants.depthOfField.x;
    float taps = min(constants.lens.y, k_maxDiscTaps);
    vec3 sum = center;
    float weight = 1.0;
    float rotation = interleavedGradientNoise(gl_FragCoord.xy) * 6.2831853;

    for (float index = 0.0; index < k_maxDiscTaps; index += 1.0)
    {
        if (index >= taps)
        {
            break;
        }

        float distanceFromCenter = sqrt((index + 0.5) / taps) * radius;
        float angle = index * k_goldenAngle + rotation;
        vec2 tapUv = uv + vec2(cos(angle), sin(angle)) * distanceFromCenter * constants.inverseSize;
        float tapDepth = textureLod(depthTexture, tapUv, 0.0).r;
        float tapBlur = circleOfConfusion(tapDepth, focus);

        if (tapDepth > centerDepth)
        {
            tapBlur = min(tapBlur, centerBlur);
        }

        float reach = clamp(tapBlur - distanceFromCenter + 1.0, 0.0, 1.0);
        sum += lensSource(tapUv) * reach;
        weight += reach;
    }

    return sum / weight;
}

// Averages taps along this pixel's motion since last frame (centred on it), skipping viewmodel taps
// for world pixels so the weapon does not smear into the scene.
vec3 motionBlur(vec3 center, vec2 uv)
{
    float centerDepth = textureLod(depthTexture, uv, 0.0).r;
    bool isViewModel = isViewModelDepth(centerDepth);
    float scale = constants.motionBlur.x * (isViewModel ? constants.motionBlur.w : 1.0);
    vec2 smear = textureLod(velocityTexture, uv, 0.0).xy * scale;
    vec2 smearPixels = smear / constants.inverseSize;
    float smearLength = length(smearPixels);

    if (smearLength < 0.5)
    {
        return center;
    }

    smear *= min(constants.motionBlur.y / smearLength, 1.0);
    float taps = min(constants.motionBlur.z, k_maxBlurTaps);
    float jitter = interleavedGradientNoise(gl_FragCoord.xy + float(constants.frameIndex % 8u) * 7.0) - 0.5;
    vec3 sum = center;
    float weight = 1.0;

    for (float index = 0.0; index < k_maxBlurTaps; index += 1.0)
    {
        if (index >= taps)
        {
            break;
        }

        vec2 tapUv = uv + smear * ((index + 0.5 + jitter) / taps - 0.5);

        if (!isViewModel && isViewModelDepth(textureLod(depthTexture, tapUv, 0.0).r))
        {
            continue;
        }

        sum += lensSource(tapUv);
        weight += 1.0;
    }

    return sum / weight;
}

// Moves red inwards and blue outwards by an amount growing with the square of the distance from the
// screen centre, keeping the filtered green.
vec3 chromaticAberration(vec3 color, vec2 uv)
{
    vec2 fromCentre = uv - 0.5;
    float reach = dot(fromCentre, fromCentre) * 2.0;
    float pixels = constants.lens.z * reach;

    if (pixels < 0.05)
    {
        return color;
    }

    vec2 offset = normalize(fromCentre) * pixels * constants.inverseSize;
    vec3 raw = lensSource(uv);
    color.r = max(color.r + lensSource(uv - offset).r - raw.r, 0.0);
    color.b = max(color.b + lensSource(uv + offset).b - raw.b, 0.0);

    return color;
}

vec3 lensEffects(vec3 color, vec2 uv)
{
    if (constants.depthOfField.x > 0.0)
    {
        color = depthOfField(color, uv);
    }

    if (constants.motionBlur.x > 0.0)
    {
        color = motionBlur(color, uv);
    }

    if (constants.lens.z > 0.0)
    {
        color = chromaticAberration(color, uv);
    }

    return color;
}
