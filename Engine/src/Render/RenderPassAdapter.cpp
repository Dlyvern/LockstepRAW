#include "RenderPassAdapter.hpp"

#include <string>

namespace lockstep::render
{
    RenderPassAdapter::RenderPassAdapter(RenderPass& pass)
        : m_pass(pass)
    {
    }

    vela::graphics::PassDescription RenderPassAdapter::describe() const
    {
        vela::graphics::AttachmentSlot colorSlot{};
        colorSlot.name = std::string(k_displayAttachment);
        colorSlot.format = k_displayFormat;
        colorSlot.load = vela::graphics::LoadOp::Load;
        colorSlot.store = vela::graphics::StoreOp::Store;

        vela::graphics::PassDescription description{};
        description.colorOutputs.push_back(colorSlot);

        return description;
    }

    void RenderPassAdapter::record(vela::graphics::PassRecorder& recorder)
    {
        m_pass.record(recorder);
    }
} //namespace lockstep::render
