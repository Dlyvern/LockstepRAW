#include "RenderScale.hpp"

#include "Lockstep/Core/Log.hpp"

#include "FrameAttachments.hpp"
#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/RenderGraph.hpp>

#include <string>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_shadowPassName{"shadow"};

        class ScaledPass final : public vela::graphics::Pass
        {
        public:
            ScaledPass(std::unique_ptr<vela::graphics::Pass> pass, const float& scale)
                : m_pass(std::move(pass)),
                  m_scale(&scale)
            {
            }

            vela::graphics::PassDescription describe() const override
            {
                vela::graphics::PassDescription description = m_pass->describe();

                for (vela::graphics::AttachmentSlot& slot : description.colorOutputs)
                {
                    scale(slot);
                }

                if (description.depthOutput)
                {
                    scale(*description.depthOutput);
                }

                return description;
            }

            void record(vela::graphics::PassRecorder& recorder) override
            {
                m_pass->record(recorder);
            }

        private:
            void scale(vela::graphics::AttachmentSlot& slot) const
            {
                const bool isFixed = slot.size.width > 0 || slot.size.height > 0;

                if (!isFixed && slot.name != k_displayAttachment)
                {
                    slot.scale *= *m_scale;
                }
            }

            std::unique_ptr<vela::graphics::Pass> m_pass;
            const float* m_scale{nullptr};
        };

        Status redescribeScaledPasses(RendererState& state)
        {
            for (const std::string& name : state.scaledPassNames)
            {
                if (auto redescribed = state.graph->redescribePass(name); !redescribed)
                {
                    return toError(redescribed.error());
                }
            }

            return {};
        }
    } //namespace

    Status addScaledPass(vela::graphics::RenderGraph& graph, RendererState& state, std::string_view name,
        std::unique_ptr<vela::graphics::Pass> pass)
    {
        std::string passName(name);

        if (auto added = graph.addPass(passName, std::make_unique<ScaledPass>(std::move(pass), state.renderScale));
            !added)
        {
            return toError(added.error());
        }

        state.scaledPassNames.push_back(std::move(passName));

        return {};
    }

    Status updateRenderScale(RendererState& state)
    {
        const float requested = state.quality.renderScale;

        if (requested == state.renderScale)
        {
            return {};
        }

        const float previous = state.renderScale;
        state.renderScale = requested;

        if (auto redescribed = redescribeScaledPasses(state); !redescribed)
        {
            state.renderScale = previous;
            state.quality.renderScale = previous;

            if (auto restored = redescribeScaledPasses(state); !restored)
            {
                log(LogLevel::Error, "Render", "restoring the render scale failed: " + restored.error().message);
            }

            return redescribed;
        }

        return {};
    }

    Status updateShadowAtlas(RendererState& state)
    {
        const uint32_t requested = state.quality.shadowAtlasSize;

        if (requested == state.shadowAtlasWidth)
        {
            return {};
        }

        const uint32_t previous = state.shadowAtlasWidth;
        state.shadowAtlasWidth = requested;

        if (auto redescribed = state.graph->redescribePass(std::string(k_shadowPassName)); !redescribed)
        {
            state.shadowAtlasWidth = previous;
            state.quality.shadowAtlasSize = previous;
            return toError(redescribed.error());
        }

        return {};
    }
} //namespace lockstep::render
