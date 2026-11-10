#include "ScreenPass.hpp"

#include "FramePasses.hpp"
#include "RenderScale.hpp"
#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/RenderGraph.hpp>
#include <Vela/Graphics/VertexLayout.hpp>

#include <string>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_fullscreenVertexShader{"fullscreen.vert.spv"};
        constexpr uint32_t k_fullscreenTriangleVertices{3};
    } //namespace

    ScreenPass::ScreenPass(std::vector<vela::graphics::AttachmentSlot> outputs,
        std::vector<vela::graphics::PassInput> inputs, vela::graphics::Material&& material, const float* scale,
        Recorder recorder)
        : m_outputs(std::move(outputs)),
          m_inputs(std::move(inputs)),
          m_material(std::move(material)),
          m_scale(scale),
          m_recorder(std::move(recorder))
    {
    }

    vela::graphics::PassDescription ScreenPass::describe() const
    {
        vela::graphics::PassDescription description{};
        description.colorOutputs = m_outputs;
        description.inputs = m_inputs;

        if (m_scale != nullptr)
        {
            for (vela::graphics::AttachmentSlot& slot : description.colorOutputs)
            {
                slot.scale = *m_scale;
            }
        }

        return description;
    }

    void ScreenPass::record(vela::graphics::PassRecorder& recorder)
    {
        recorder.bind(m_material);

        for (uint32_t slot = 0; slot < m_inputs.size(); ++slot)
        {
            recorder.bindAttachment(slot, m_inputs[slot].name);
        }

        if (m_recorder(recorder))
        {
            recorder.drawInstanced(k_fullscreenTriangleVertices, 1);
        }
    }

    Status addScreenPass(vela::graphics::RenderGraph& graph, RendererState& state, ScreenPassDescription description,
        ScreenPass::Recorder recorder)
    {
        vela::graphics::RenderState renderState{};
        renderState.depthTest = false;
        renderState.depthWrite = false;
        renderState.blend = description.blend;

        auto material = createBuiltinMaterial(state.context, k_fullscreenVertexShader, description.fragmentShader,
            vela::graphics::VertexLayout{}, renderState);

        if (!material)
        {
            return material.error();
        }

        auto pass = std::make_unique<ScreenPass>(std::move(description.outputs), std::move(description.inputs),
            std::move(material).value(), description.scale, std::move(recorder));

        return addScaledPass(graph, state, description.name, std::move(pass));
    }

    vela::graphics::AttachmentSlot clearedScreenSlot(std::string_view name, vela::graphics::TextureFormat format,
        float scale, const vela::graphics::ClearValue& clear)
    {
        vela::graphics::AttachmentSlot slot{};
        slot.name = std::string(name);
        slot.format = format;
        slot.load = vela::graphics::LoadOp::Clear;
        slot.store = vela::graphics::StoreOp::Store;
        slot.clear = clear;
        slot.scale = scale;

        return slot;
    }

    vela::graphics::AttachmentSlot loadedScreenSlot(std::string_view name, vela::graphics::TextureFormat format)
    {
        vela::graphics::AttachmentSlot slot{};
        slot.name = std::string(name);
        slot.format = format;
        slot.load = vela::graphics::LoadOp::Load;
        slot.store = vela::graphics::StoreOp::Store;

        return slot;
    }
} //namespace lockstep::render
