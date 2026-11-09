#include <Vela/Builtins/ForwardGraph.hpp>
#include <Vela/Core/Context.hpp>
#include <Vela/Core/Window.hpp>
#include <Vela/Graphics/RenderScene.hpp>

#include <iostream>

namespace
{
    int fail(const vela::Error& error)
    {
        std::cerr << "Vela error " << static_cast<uint32_t>(error.code) << ": " << error.message << '\n';
        return 1;
    }
} //namespace

int main()
{
    auto windowResult = vela::core::Window::create({.title = "Lockstep", .width = 1280, .height = 720});

    if (!windowResult)
        return fail(windowResult.error());

    vela::core::Window window = std::move(windowResult).value();

    auto contextResult = vela::core::Context::create(window);

    if (!contextResult)
        return fail(contextResult.error());

    vela::core::Context context = std::move(contextResult).value();

    if (auto attached = context.attach(window); !attached)
        return fail(attached.error());

    vela::graphics::RenderScene scene;

    auto graphResult = vela::builtins::forwardGraph(context, scene);

    if (!graphResult)
        return fail(graphResult.error());

    vela::graphics::RenderGraph graph = std::move(graphResult).value();

    vela::core::Event event{};

    while (window.isOpen())
    {
        window.pollEvents();

        while (window.popEvent(event))
            if (event.type == vela::core::EventType::KeyPressed && event.key == vela::core::Key::Escape)
                window.close();

        if (auto frame = graph.execute(); !frame)
            return fail(frame.error());
    }

    context.waitIdle();

    return 0;
}
