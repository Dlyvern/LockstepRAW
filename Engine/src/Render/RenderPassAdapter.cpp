#include "RenderPassAdapter.hpp"

#include <string>

namespace lockstep::render
{
    RenderPassAdapter::RenderPassAdapter(RenderPass& pass)
        : m_pass(&pass)
    {
    }

    vela::graphics::PassDescription RenderPassAdapter::describe() const
    {
        vela::graphics::AttachmentSlot colorSlot{};
        colorSlot.name = std::string(k_sceneColorAttachment);
        colorSlot.format = k_sceneColorFormat;
        colorSlot.load = vela::graphics::LoadOp::Load;
        colorSlot.store = vela::graphics::StoreOp::Store;

        vela::graphics::PassDescription description{};
        description.colorOutputs.push_back(colorSlot);

        return description;
    }

    void RenderPassAdapter::record(vela::graphics::PassRecorder& recorder)
    {
        if (m_pass != nullptr)
        {
            m_pass->record(recorder);
        }
    }

    bool RenderPassAdapter::isAttachedTo(const RenderPass& pass) const
    {
        return m_pass == &pass;
    }

    void RenderPassAdapter::detach()
    {
        m_pass = nullptr;
    }
} //namespace lockstep::render
