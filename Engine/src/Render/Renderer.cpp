#include "Lockstep/Render/Renderer.hpp"

#include "FrameCulling.hpp"
#include "FrameGraph.hpp"
#include "PlanarReflections.hpp"
#include "RenderScale.hpp"
#include "RendererState.hpp"
#include "ScopeView.hpp"
#include "ShaderLoader.hpp"
#include "ShadowMaterials.hpp"
#include "Skinning.hpp"
#include "TextPass.hpp"
#include "VelaConversions.hpp"
#include "ViewModel.hpp"
#include "VolumetricFog.hpp"

#include "Platform/WindowState.hpp"

#include <Vela/Core/ContextPreferences.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_maxFrameSeconds{0.25f};
        constexpr std::string_view k_unlitVertexShader{"unlit.vert.spv"};
        constexpr std::string_view k_unlitFragmentShader{"unlit.frag.spv"};
        constexpr std::string_view k_pbrVertexShader{"pbr.vert.spv"};
        constexpr std::string_view k_pbrFragmentShader{"pbr.frag.spv"};
        constexpr std::string_view k_shadowVertexShader{"shadow.vert.spv"};
        constexpr std::string_view k_shadowFragmentShader{"shadow.frag.spv"};
        constexpr std::string_view k_shadowMaskVertexShader{"shadow_mask.vert.spv"};
        constexpr std::string_view k_shadowMaskFragmentShader{"shadow_mask.frag.spv"};
        constexpr std::string_view k_pbrSkinnedVertexShader{"pbr_skinned.vert.spv"};
        constexpr std::string_view k_shadowSkinnedVertexShader{"shadow_skinned.vert.spv"};
        constexpr std::string_view k_shadowMaskSkinnedVertexShader{"shadow_mask_skinned.vert.spv"};
        constexpr std::size_t k_initialJointBufferBytes{256 * sizeof(math::Mat4)};
        constexpr const char* k_engineName{"Lockstep"};

        Result<vela::core::Context> createPresentingContext(vela::core::Window& window,
            vela::core::ContextPreferences preferences, PresentMode mode)
        {
            preferences.preferredVSync = toVelaVSync(mode, {});
            auto context = vela::core::Context::create(window, preferences);

            if (!context)
            {
                return toError(context.error());
            }

            if (auto attached = context.value().attach(window); !attached)
            {
                return toError(attached.error());
            }

            const vela::core::VSync best = toVelaVSync(mode, context.value().getSupportedVSync());

            if (best != preferences.preferredVSync)
            {
                if (auto changed = context.value().setVSync(best); !changed)
                {
                    return toError(changed.error());
                }
            }

            return std::move(context).value();
        }

        vela::graphics::Extent2D updateAspectRatio(RendererState& state)
        {
            int width = 0;
            int height = 0;
            state.window.getFramebufferSize(width, height);

            if (width > 0 && height > 0)
            {
                state.aspectRatio = static_cast<float>(width) / static_cast<float>(height);
                return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            }

            return {};
        }

        vela::graphics::Extent2D scaledExtent(const vela::graphics::Extent2D& extent, float scale)
        {
            if (extent.width == 0 || extent.height == 0)
            {
                return extent;
            }

            return {std::max(1u, static_cast<uint32_t>(static_cast<float>(extent.width) * scale)),
                std::max(1u, static_cast<uint32_t>(static_cast<float>(extent.height) * scale))};
        }

        void updateFrameSeconds(RendererState& state)
        {
            const auto now = std::chrono::steady_clock::now();
            const bool hasPrevious = state.lastRender != std::chrono::steady_clock::time_point{};
            const float seconds = hasPrevious ? std::chrono::duration<float>(now - state.lastRender).count() : 0.0f;
            state.frameSeconds = std::clamp(seconds, 0.0f, k_maxFrameSeconds);
            state.lastRender = now;
        }

        void writeCameraMotion(RendererState& state)
        {
            GpuFrameLighting& lighting = state.prepared.lighting;
            lighting.viewProjection = state.temporal.viewProjection;
            lighting.previousViewProjection = state.temporal.previousViewProjection;
        }

        Result<MaterialShaders> loadMaterialShaders()
        {
            MaterialShaders shaders;
            const std::pair<std::string_view, std::vector<uint32_t>*> files[]{
                {k_unlitVertexShader, &shaders.unlitVertex},
                {k_unlitFragmentShader, &shaders.unlitFragment},
                {k_pbrVertexShader, &shaders.pbrVertex},
                {k_pbrFragmentShader, &shaders.pbrFragment},
                {k_shadowVertexShader, &shaders.shadowVertex},
                {k_shadowFragmentShader, &shaders.shadowFragment},
                {k_shadowMaskVertexShader, &shaders.shadowMaskVertex},
                {k_shadowMaskFragmentShader, &shaders.shadowMaskFragment},
                {k_pbrSkinnedVertexShader, &shaders.pbrSkinnedVertex},
                {k_shadowSkinnedVertexShader, &shaders.shadowSkinnedVertex},
                {k_shadowMaskSkinnedVertexShader, &shaders.shadowMaskSkinnedVertex}};

            for (const auto& [fileName, target] : files)
            {
                auto loaded = loadShader(fileName);

                if (!loaded)
                {
                    return loaded.error();
                }

                *target = std::move(loaded).value();
            }

            return shaders;
        }

    } //namespace

    RendererState& rendererState(Renderer& renderer)
    {
        return *renderer.m_state;
    }

    Result<Renderer> Renderer::create(platform::Window& window, const RendererDescription& description)
    {
        auto shaders = loadMaterialShaders();

        if (!shaders)
        {
            return shaders.error();
        }

        vela::core::Window& velaWindow = platform::windowState(window).window;

        vela::core::ContextPreferences preferences{};
        preferences.applicationName = description.applicationName;
        preferences.engineName = k_engineName;
        preferences.optionalFeatures.push_back(vela::core::Feature::SamplerAnisotropy);

        auto context = createPresentingContext(velaWindow, preferences, description.presentMode);

        if (!context)
        {
            return context.error();
        }

        auto lightBuffer = vela::graphics::DynamicBuffer::create(context.value(),
            vela::graphics::DynamicBuffer::Usage::Storage, sizeof(GpuFrameLighting));

        if (!lightBuffer)
        {
            return toError(lightBuffer.error());
        }

        auto jointBuffer = vela::graphics::DynamicBuffer::create(context.value(),
            vela::graphics::DynamicBuffer::Usage::Storage, k_initialJointBufferBytes);

        if (!jointBuffer)
        {
            return toError(jointBuffer.error());
        }

        auto environment = EnvironmentResources::create(context.value());

        if (!environment)
        {
            return environment.error();
        }

        auto state = std::make_unique<RendererState>(velaWindow, std::move(context).value(),
            std::move(shaders).value(), std::move(lightBuffer).value(), std::move(jointBuffer).value(),
            std::move(environment).value());

        state->presentMode = description.presentMode;
        state->quality = clampedQuality(description.quality);
        state->renderScale = state->quality.renderScale;
        state->shadowAtlasWidth = state->quality.shadowAtlasSize;
        state->settingsFile = description.settingsFile;
        state->settings = limitedByQuality(state->requestedSettings, state->quality);

        if (auto shadowMaterials = updateShadowMaterials(*state); !shadowMaterials)
        {
            return shadowMaterials.error();
        }

        if (auto built = buildFrameGraph(*state); !built)
        {
            return built.error();
        }

        if (auto text = addTextPass(*state); !text)
        {
            return text.error();
        }

        static_cast<void>(updateAspectRatio(*state));

        return Renderer(std::move(state));
    }

    Renderer::Renderer(std::unique_ptr<RendererState> state)
        : m_state(std::move(state))
    {
    }

    Renderer::~Renderer() = default;

    Renderer::Renderer(Renderer&& other) noexcept = default;

    Renderer& Renderer::operator=(Renderer&& other) noexcept = default;

    void Renderer::setCamera(const Camera& camera)
    {
        m_state->camera = camera;
    }

    void Renderer::setScreenFade(float amount, const math::Vec3& colour)
    {
        const float clamped = std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 0.0f;
        m_state->screenFade = math::Vec4{colour.x, colour.y, colour.z, clamped};
    }

    void Renderer::submit(MeshHandle mesh, MaterialHandle material, const math::Transform& transform)
    {
        submit(mesh, material, transform, k_defaultRenderLayer);
    }

    void Renderer::submit(MeshHandle mesh, MaterialHandle material, const math::Transform& transform,
        RenderLayers layers)
    {
        submit(mesh, material, transform, layers, 1.0f);
    }

    void Renderer::submit(MeshHandle mesh, MaterialHandle material, const math::Transform& transform,
        RenderLayers layers, float emissiveScale)
    {
        if (m_state->meshes.isStale(mesh) || m_state->materials.isStale(material))
        {
            reportStaleHandle(*m_state, m_state->meshes.isStale(mesh) ? "submit: mesh" : "submit: material");
            return;
        }

        const MeshRecord* record = m_state->meshes.find(mesh);
        const MaterialRecord* materialRecord = m_state->materials.find(material);

        assert(record != nullptr && "submit: invalid mesh handle");
        assert(materialRecord != nullptr && "submit: invalid material handle");
        assert((record == nullptr || record->kind == MeshKind::Static) && "submit: skinned mesh needs submitSkinned");
        assert((materialRecord == nullptr || !isViewModelDraw(layers) || materialRecord->shading == Shading::Lit)
            && "submit: viewmodel draws need lit materials");

        if (record == nullptr || materialRecord == nullptr || record->kind != MeshKind::Static
            || materialRecord->kind != MeshKind::Static
            || (isViewModelDraw(layers) && materialRecord->shading != Shading::Lit))
        {
            return;
        }

        const math::Mat4 model = math::toMatrix(transform);
        m_state->draws.push_back({
            .mesh = &record->mesh,
            .material = materialRecord,
            .model = model,
            .previousModel = model,
            .worldBounds = math::transformBounds(record->bounds, transform),
            .cameraDistance = 0.0f,
            .layers = layers,
            .isReflector = false,
            .emissiveScale = std::isfinite(emissiveScale) ? std::max(emissiveScale, 0.0f) : 1.0f});
    }

    void Renderer::submitDecal(MeshHandle mesh, MaterialHandle material, const math::Transform& transform)
    {
        const std::size_t before = m_state->draws.size();
        submit(mesh, material, transform, k_defaultRenderLayer);

        if (m_state->draws.size() > before)
        {
            m_state->draws.back().isDecal = true;
        }
    }

    void Renderer::submitReflector(const PlanarReflector& reflector)
    {
        if (m_state->reflectors.size() < k_maxPlanarReflectors)
        {
            m_state->reflectors.push_back(reflector);
        }
    }

    void Renderer::submitScope(const ScopeView& scope)
    {
        m_state->scopeView = scope;
    }

    void Renderer::submitSight(const HolographicSight& sight)
    {
        m_state->sight = sight;
    }

    void Renderer::submitLight(const DirectionalLight& light)
    {
        m_state->lights.directional = light;
    }

    void Renderer::submitLight(const PointLight& light)
    {
        if (!m_state->lights.hasRoom())
        {
            ++m_state->lights.dropped;
            return;
        }

        m_state->lights.points.push_back(light);
    }

    void Renderer::submitLight(const SpotLight& light)
    {
        if (!m_state->lights.hasRoom())
        {
            ++m_state->lights.dropped;
            return;
        }

        m_state->lights.spots.push_back(light);
    }

    void Renderer::submitText(std::string_view utf8, math::Vec2 position, const TextStyle& style)
    {
        m_state->textPass->submit(utf8, position, style);
    }

    Status Renderer::setPresentMode(PresentMode mode)
    {
        return m_state->setPresentMode(mode);
    }

    PresentMode Renderer::presentMode() const
    {
        return m_state->presentMode;
    }

    void Renderer::setAmbientZones(std::span<const AmbientZone> zones)
    {
        m_state->ambientZones.assign(zones.begin(), zones.end());
    }

    void Renderer::setGraphicsQuality(const GraphicsQuality& quality)
    {
        m_state->quality = clampedQuality(quality);
    }

    const GraphicsQuality& Renderer::graphicsQuality() const
    {
        return m_state->quality;
    }

    RenderSettings& Renderer::settings()
    {
        return m_state->requestedSettings;
    }

    const RenderSettings& Renderer::settings() const
    {
        return m_state->requestedSettings;
    }

    Status Renderer::render()
    {
        RendererState& state = *m_state;
        updateFrameSeconds(state);
        state.settings = limitedByQuality(state.requestedSettings, state.quality);
        Status resized = updateRenderScale(state);

        if (resized)
        {
            resized = updateShadowAtlas(state);
        }

        const vela::graphics::Extent2D extent = scaledExtent(updateAspectRatio(state), state.renderScale);
        state.frameHeight = extent.height;

        for (RegisteredPass& registered : state.passes)
        {
            registered.pass->prepareFrame();
        }

        state.view = toViewMatrix(state.camera);
        state.unjitteredProjection = toProjectionMatrix(state.camera, state.aspectRatio);
        state.projection = state.unjitteredProjection;
        state.projection = beginTemporalFrame(state, extent.width, extent.height);
        state.prepared = prepareLighting(state.lights, state.settings,
            LightingView{.camera = state.camera, .aspectRatio = state.aspectRatio,
                .viewProjection = state.temporal.viewProjection, .zones = state.ambientZones,
                .shadowAtlasWidth = state.shadowAtlasWidth});
        writeCameraMotion(state);
        prepareViewModel(state);
        assignPreviousModels(state);
        buildDrawLists(state, state.temporal.viewProjection, state.temporal.viewModelViewProjection);
        prepareReflection(state);
        prepareScope(state);
        prepareSight(state);
        state.graph->setView(state.view, state.projection);

        Status frame = resized;

        if (frame)
        {
            frame = updateShadowMaterials(state);
        }

        if (frame)
        {
            frame = uploadJointPalettes(state);
        }

        if (frame)
        {
            frame = updateSceneSamples(state);
        }

        if (frame)
        {
            frame = updateReflectionScale(state);
        }

        if (frame)
        {
            frame = updateVolumetricFogScale(state);
        }

        if (frame)
        {
            frame = updateScopeResolution(state);
        }

        if (frame)
        {
            if (auto executed = state.graph->execute(); !executed)
            {
                frame = toError(executed.error());
            }
        }

        uint32_t shadowDraws = 0;

        for (const std::vector<uint32_t>& casters : state.drawLists.shadowCasters)
        {
            shadowDraws += static_cast<uint32_t>(casters.size());
        }

        state.lastFrame = {
            .local = static_cast<uint32_t>(state.lights.points.size() + state.lights.spots.size()),
            .dropped = state.lights.dropped,
            .hasDirectional = state.prepared.hasSun,
            .shadowedLocalLights = state.prepared.shadowedLocalLights,
            .localShadowTiles = state.prepared.localTileCount,
            .cascades = state.prepared.cascadeCount,
            .submitted = static_cast<uint32_t>(state.draws.size()),
            .visible = static_cast<uint32_t>(state.drawLists.opaque.size() + state.drawLists.blended.size()
                + state.drawLists.viewModelOpaque.size() + state.drawLists.viewModelBlended.size()),
            .shadowDraws = shadowDraws};
        endTemporalFrame(state);
        state.draws.clear();
        state.jointData.clear();
        state.drawLists.clear();
        state.lights.clear();
        state.reflectors.clear();
        state.scopeView.reset();
        state.sight.reset();
        releaseRetiredResources(state);
        ++state.frameIndex;

        return frame;
    }
} //namespace lockstep::render
