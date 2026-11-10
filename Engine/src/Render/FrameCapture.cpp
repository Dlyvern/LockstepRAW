#include "Lockstep/Render/Renderer.hpp"

#include "Lockstep/Core/Log.hpp"
#include "PngWriter.hpp"
#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <string>
#include <utility>

namespace lockstep::render
{
    Status Renderer::captureNextFrame(std::string_view path)
    {
        RendererState& state = *m_state;

        if (path.empty() || !state.graph)
        {
            return Error{ErrorCode::InvalidArgument, "captureNextFrame: no path or no render graph"};
        }

        auto requested = state.graph->readbackFrame([file = std::string(path)](const vela::graphics::ImageData& image)
        {
            if (auto written = writePng(file, image.pixels, image.width, image.height); !written)
            {
                log(LogLevel::Error, "Render", written.error().message);
                return;
            }

            log(LogLevel::Info, "Render", "captured frame to '" + file + "'");
        });

        if (!requested)
        {
            return toError(requested.error());
        }

        return {};
    }
} //namespace lockstep::render
