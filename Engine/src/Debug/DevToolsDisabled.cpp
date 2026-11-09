#include "Lockstep/Debug/DevTools.hpp"

#include <utility>

namespace lockstep::debug
{
    struct DevToolsState
    {
    };

    Result<DevTools> DevTools::create(render::Renderer&, platform::Window&)
    {
        return DevTools(nullptr);
    }

    DevTools::DevTools(std::unique_ptr<DevToolsState> state)
        : m_state(std::move(state))
    {
    }

    DevTools::~DevTools() = default;

    DevTools::DevTools(DevTools&& other) noexcept = default;

    DevTools& DevTools::operator=(DevTools&& other) noexcept = default;

    void DevTools::beginFrame(float)
    {
    }

    void DevTools::drawNetPanel(net::Network&, net::Session&, net::Replication&)
    {
    }

    void DevTools::drawPhysicsSection(const physics::PhysicsWorld&)
    {
    }

    void DevTools::drawAudioPanel(audio::AudioSystem&)
    {
    }

    void DevTools::recordFrameTiming(const FrameTiming&)
    {
    }

    bool DevTools::isVisible() const
    {
        return false;
    }

    void DevTools::setVisible(bool)
    {
    }

    bool DevTools::wantsMouse() const
    {
        return false;
    }

    bool DevTools::wantsKeyboard() const
    {
        return false;
    }
} //namespace lockstep::debug
