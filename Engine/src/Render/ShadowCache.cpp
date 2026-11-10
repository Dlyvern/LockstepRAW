#include "ShadowCache.hpp"

#include "RendererState.hpp"

#include <cmath>

namespace lockstep::render
{
    namespace
    {
        constexpr uint32_t k_firstHeldCascade{2};
        constexpr uint32_t k_heldCascadeInterval{2};
        constexpr float k_maxHeldTurnCos{0.9986f};
        constexpr float k_maxHeldMoveFraction{0.05f};
        constexpr float k_minSameSunCos{0.99999f};

        bool canHold(const HeldCascade& held, const ShadowTile& fitted, const math::Vec3& position,
            const math::Vec3& forward, const math::Vec3& sunTravel, uint32_t tileTexels)
        {
            if (!held.isValid || held.tile.texelSize != fitted.texelSize)
            {
                return false;
            }

            const float radius = fitted.texelSize * static_cast<float>(tileTexels) * 0.5f;

            return math::length(position - held.cameraPosition) < radius * k_maxHeldMoveFraction
                && math::dot(forward, held.cameraForward) > k_maxHeldTurnCos
                && math::dot(sunTravel, held.sunTravel) > k_minSameSunCos;
        }
    } //namespace

    void ShadowCache::invalidate()
    {
        tileValid.fill(false);
        cascades.fill(HeldCascade{});
        isInvalidated = true;
    }

    void holdFarCascades(RendererState& state)
    {
        PreparedLighting& prepared = state.prepared;
        const math::Vec3 position = state.camera.position;
        const math::Vec3 forward = math::normalize(state.camera.target - state.camera.position);

        for (uint32_t cascade = 0; cascade < k_sunShadowTiles; ++cascade)
        {
            HeldCascade& held = state.shadowCache.cascades[cascade];

            if (cascade < k_firstHeldCascade || cascade >= prepared.cascadeCount)
            {
                held.isValid = false;
                continue;
            }

            const ShadowTile& fitted = prepared.tiles[cascade];
            const bool isRefreshFrame = (state.frameIndex + cascade) % k_heldCascadeInterval == 0;

            if (!isRefreshFrame && canHold(held, fitted, position, forward, prepared.sunTravel,
                shadowTileTexels(cascade, state.shadowAtlasWidth)))
            {
                prepared.tiles[cascade] = held.tile;
                prepared.lighting.shadowTileViewProjection[cascade] = held.tile.atlasViewProjection;
                continue;
            }

            held = HeldCascade{.tile = fitted, .cameraPosition = position, .cameraForward = forward,
                .sunTravel = prepared.sunTravel, .isValid = true};
        }
    }
} //namespace lockstep::render
