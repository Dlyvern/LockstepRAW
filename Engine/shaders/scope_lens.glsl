// The scope view (draw.flags.z 4) and the scope's eyepiece lens (5). Included by pbr.frag after
// its lighting helpers; reads the scope fields of the frame lighting block (ScopeView.cpp) and,
// for the lens, the scope view colour bound in the planar reflection slot.
//
// The lens is a picture in picture: each lens pixel looks along the ray from the eye through it;
// the ray's angle to the scope axis, divided by the magnification, is the direction of the world
// that pixel shows, looked up in the scope view (rendered along the scope's axis). The reticle sits
// on the axis in the second focal plane. The scope shadow comes from the field stop (the edge of
// the magnified field) and the exit pupil: the ray, followed back to the eye relief behind the
// lens, must pass through the exit pupil.

const float k_scopeLensTransmission = 0.92;
const float k_scopeEdgeDarkening = 0.25;
const float k_scopeHistoryReject = 1e6;
const float k_scopeMaxExponent = 30.0;
const float k_scopeFieldMargin = 1.06;
const float k_reticleMinPixels = 0.6;
const float k_reticleMinDotEnergy = 0.5;

// The scope view's fog over `along` metres from the scope view's position along a ray whose
// direction has height `rayY`: the scene colour faded towards the fog's ambient glow.
vec3 applyScopeFog(vec3 color, float rayY, float along)
{
    float maxDistance = frame.scopeFog.w;

    if (maxDistance <= 0.0)
    {
        return color;
    }

    float distance = min(along, maxDistance);
    float rate = frame.scopeFog.y * rayY;
    float depth = abs(rate) < 1e-4 ? frame.scopeFog.x * distance
        : frame.scopeFog.x * (1.0 - exp(clamp(-rate * distance, -k_scopeMaxExponent, k_scopeMaxExponent))) / rate;
    float transmittance = exp(-depth);

    return color * transmittance + frame.scopeFogColor.rgb * (1.0 - transmittance);
}

// True for a scope view fragment outside the circular field the lens can show (the corners of
// the square view), which is never sampled, so it is not shaded.
bool outsideScopeField(vec3 position)
{
    vec4 clip = frame.scopeViewProjection * vec4(position, 1.0);
    vec2 field = (clip.xy / clip.w + 1.0) / max(frame.scopeAxisUp.w, 1e-4) - 1.0;

    return dot(field, field) > k_scopeFieldMargin;
}

// Coverage of a band of half-width `halfWidth` around 0 at `value`, antialiased by `pixel`, the
// screen-space size of one pixel in the same units.
float scopeBand(float value, float halfWidth, float pixel)
{
    return 1.0 - smoothstep(halfWidth - pixel * 0.5, halfWidth + pixel * 0.5, abs(value));
}

// How much of a pixel the reticle covers at `field` (the field stop radius is 1).
float reticleCoverage(vec2 field, float pixel)
{
    float lineHalf = frame.scopeReticle.w * 0.5;
    float postHalf = frame.scopeReticleShape.x * 0.5;
    float gap = frame.scopeReticleShape.y;
    float spacing = frame.scopeReticleShape.z;
    float tickHalf = frame.scopeReticleShape.w;
    vec2 distanceFromCentre = abs(field);

    float fine = max(scopeBand(field.x, lineHalf, pixel) * step(distanceFromCentre.y, gap + pixel),
        scopeBand(field.y, lineHalf, pixel) * step(distanceFromCentre.x, gap + pixel));
    float posts = 0.0;

    if (postHalf > 0.0)
    {
        float verticalPost = scopeBand(field.x, postHalf, pixel) * smoothstep(gap - pixel, gap, distanceFromCentre.y);
        float horizontalPost = scopeBand(field.y, postHalf, pixel) * smoothstep(gap - pixel, gap, distanceFromCentre.x);
        posts = max(verticalPost, horizontalPost);
    }

    float ticks = 0.0;

    if (spacing > 0.0)
    {
        vec2 nearest = abs(field - round(field / spacing) * spacing);
        float onVertical = scopeBand(field.x, tickHalf, pixel) * scopeBand(nearest.y, lineHalf, pixel)
            * step(spacing * 0.5, distanceFromCentre.y) * step(distanceFromCentre.y, gap);
        float onHorizontal = scopeBand(field.y, tickHalf, pixel) * scopeBand(nearest.x, lineHalf, pixel)
            * step(spacing * 0.5, distanceFromCentre.x) * step(distanceFromCentre.x, gap);
        ticks = max(onVertical, onHorizontal);
    }

    return max(max(fine, posts), ticks);
}

// The lens surface point the unjittered pixel centre sees: TAA jitters the lens draw by a fraction
// of a pixel each frame, which would make the magnified image and reticle shimmer, so the point
// is moved back along the screen derivatives by the jitter.
vec3 unjitteredLensPoint()
{
    vec4 jitteredClip = frame.viewModelViewProjection * vec4(inWorldPosition, 1.0);
    vec2 jitteredNdc = jitteredClip.xy / jitteredClip.w;
    vec2 jitter = jitteredNdc - inCurrentClip.xy / inCurrentClip.w;
    vec2 ndcPerPixel = vec2(dFdx(jitteredNdc.x), dFdy(jitteredNdc.y));
    ndcPerPixel = sign(ndcPerPixel) * max(abs(ndcPerPixel), vec2(1e-6));

    return inWorldPosition + dFdx(inWorldPosition) * (jitter.x / ndcPerPixel.x)
        + dFdy(inWorldPosition) * (jitter.y / ndcPerPixel.y);
}

// Visibility of the image through the exit pupil for the ray from `eye` through lens point
// `lensPoint` (1 inside, 0 in the scope shadow).
float exitPupilVisibility(vec3 eye, vec3 lensPoint, vec3 axis, vec3 right, vec3 up)
{
    float pupil = frame.scopeAxis.w;

    if (pupil <= 0.0)
    {
        return 1.0;
    }

    vec3 centre = frame.scopeEyepiece.xyz;
    vec2 eyeOffset = vec2(dot(eye - centre, right), dot(eye - centre, up));
    vec2 lensOffset = vec2(dot(lensPoint - centre, right), dot(lensPoint - centre, up));
    float eyeDistance = max(dot(centre - eye, axis), 1e-3);
    vec2 atPupil = lensOffset + (eyeOffset - lensOffset) * (frame.scopeEyepiece.w / eyeDistance);

    return 1.0 - smoothstep(pupil * 0.6, pupil * 1.4, length(atPupil));
}

// The holographic sight's reticle seen through this window pixel: the ray from the eye through
// the pixel, compared with the sight line at infinity (frame.sightDirection). Ring and dot are
// widened to at least about a pixel, dimmed so their total light stays the same.
vec3 holographicReticle()
{
    if (frame.sightDirection.w <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 ray = normalize(unjitteredLensPoint() - camera.position.xyz);
    vec3 axis = frame.sightDirection.xyz;
    float along = dot(ray, axis);

    if (along <= 0.0)
    {
        return vec3(0.0);
    }

    float angle = atan(length(cross(ray, axis)), along);
    float pixel = max(length(dFdx(ray)), length(dFdy(ray)));
    float ringHalf = frame.sightShape.x;
    float ringShown = max(ringHalf, pixel * k_reticleMinPixels);
    float ring = scopeBand(angle - frame.sightReticle.w, ringShown, pixel) * (ringHalf / ringShown);
    float dotRadius = frame.sightShape.y;
    float dotShown = max(dotRadius, pixel * k_reticleMinPixels);
    float centre = (1.0 - smoothstep(dotShown - pixel * 0.5, dotShown + pixel * 0.5, angle))
        * max((dotRadius * dotRadius) / (dotShown * dotShown), k_reticleMinDotEnergy);

    return frame.sightReticle.rgb * max(ring, centre);
}

void shadeScopeLens()
{
    vec3 eye = camera.position.xyz;
    vec3 lensPoint = unjitteredLensPoint();
    vec3 ray = normalize(lensPoint - eye);
    vec3 axis = normalize(frame.scopeAxis.xyz);
    vec3 up = normalize(frame.scopeAxisUp.xyz - axis * dot(frame.scopeAxisUp.xyz, axis));
    vec3 right = cross(axis, up);
    float along = dot(ray, axis);

    outAmbient = vec4(0.0, 0.0, 0.0, 1.0);
    outVelocity = vec4(motionVector(true).xy, k_scopeHistoryReject, 0.0);
    outSurfaceNormal = k_neutralSurfaceNormal;
    outSurfaceSpecular = k_neutralSurfaceSpecular;

    if (along <= 0.05)
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float magnification = frame.scopeCameraUp.w;
    float tanHalfField = frame.scopeCameraForward.w;
    vec2 apparent = vec2(dot(ray, right), dot(ray, up)) / along;
    vec2 objectTangent = apparent / magnification;
    vec2 field = objectTangent / tanHalfField;
    float fieldRadius = length(field);
    float pixel = max(fwidth(field.x), fwidth(field.y));

    vec3 scopeForward = frame.scopeCameraForward.xyz;
    vec3 scopeUp = frame.scopeCameraUp.xyz;
    vec3 scopeRight = cross(scopeForward, scopeUp);
    vec3 objectDirection = normalize(scopeForward + scopeRight * objectTangent.x + scopeUp * objectTangent.y);
    vec4 clip = frame.scopeViewProjection * vec4(objectDirection, 0.0);
    vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
    vec4 seen = textureLod(planarReflection, uv, 0.0);
    vec3 image = seen.rgb;

    if (frame.skyGround.w > 0.0 && seen.a < 1.0)
    {
        vec3 sky = applyScopeFog(skyRadiance(objectDirection), objectDirection.y, frame.scopeFog.w);
        image += sky * (1.0 - seen.a);
    }

    float visibility = k_scopeLensTransmission * (1.0 - k_scopeEdgeDarkening * fieldRadius * fieldRadius);
    visibility *= 1.0 - smoothstep(1.0 - pixel, 1.0 + pixel, fieldRadius);
    visibility *= exitPupilVisibility(eye, lensPoint, axis, right, up);

    float etched = reticleCoverage(field, pixel);
    float centreDot = 1.0 - smoothstep(frame.scopeIllumination.w - pixel, frame.scopeIllumination.w + pixel,
        fieldRadius);
    vec3 color = mix(image, frame.scopeReticle.rgb, max(etched, centreDot)) * visibility;
    color += frame.scopeIllumination.rgb * centreDot * visibility;

    vec3 normal = normalize(inNormal);
    float facing = clamp(dot(normal, -ray), 0.0, 1.0);
    float fresnel = 0.02 + 0.98 * pow(1.0 - facing, 5.0);
    color += environmentSpecular(reflect(ray, normal), 0.08) * fresnel * frame.iblParams.y;

    outColor = vec4(color, 1.0);
}
