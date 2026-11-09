#include "RendererState.hpp"

#include "VelaConversions.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <string_view>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_enginePassPrefix{"engine-pass-"};
    } //namespace

    RendererState::RendererState(vela::core::Window& window, vela::core::Context&& context, MaterialShaders&& shaders,
        vela::graphics::DynamicBuffer&& lightBuffer, vela::graphics::DynamicBuffer&& jointBuffer,
        EnvironmentResources&& environment)
        : window(window),
          context(std::move(context)),
          shaders(std::move(shaders)),
          environment(std::move(environment)),
          lightBuffer(std::move(lightBuffer)),
          jointBuffer(std::move(jointBuffer))
    {
    }

    RendererState::~RendererState()
    {
        context.waitIdle();
    }

    Status RendererState::addPass(std::unique_ptr<RenderPass> pass)
    {
        assert(pass != nullptr && "addPass: pass must not be null");

        if (pass == nullptr)
        {
            return Error{ErrorCode::InvalidArgument, "addPass: pass must not be null"};
        }

        std::string name = std::string(k_enginePassPrefix) + std::to_string(passNameCounter++);

        if (auto added = graph->addPass(name, std::make_unique<RenderPassAdapter>(*pass)); !added)
        {
            return toError(added.error());
        }

        passes.push_back({.pass = std::move(pass), .graphName = std::move(name)});

        return {};
    }

    Status RendererState::removePass(const RenderPass& pass)
    {
        const auto registered = std::find_if(passes.begin(), passes.end(), [&pass](const RegisteredPass& entry)
        {
            return entry.pass.get() == &pass;
        });

        if (registered == passes.end())
        {
            return {};
        }

        if (auto removed = graph->removePass(registered->graphName); !removed)
        {
            return toError(removed.error());
        }

        context.waitIdle();
        passes.erase(registered);

        return {};
    }

    Status RendererState::setPresentMode(PresentMode mode)
    {
        if (auto changed = context.setVSync(toVelaVSync(mode, context.getSupportedVSync())); !changed)
        {
            return toError(changed.error());
        }

        presentMode = mode;

        return {};
    }
} //namespace lockstep::render
