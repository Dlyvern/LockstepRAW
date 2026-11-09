#ifndef LOCKSTEP_RENDER_TEXT_PASS_HPP
#define LOCKSTEP_RENDER_TEXT_PASS_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/Text.hpp"

#include "FontAtlas.hpp"
#include "RenderPass.hpp"

#include <Vela/Core/Context.hpp>
#include <Vela/Graphics/DynamicBuffer.hpp>
#include <Vela/Graphics/Material.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::render
{
    // One corner of a glyph quad as text.vert reads it.
    // position: framebuffer pixels, (0,0) top-left.
    // uv:       atlas coordinates.
    // color:    sRGB RGBA8 (r in the lowest byte); text.vert converts it to linear.
    // softness: half-width of the edge smoothstep in distance-field units (0..1); 0 means one
    //           screen pixel, for crisp text. Shadows use a wider value.
    struct TextVertex
    {
        float position[2]{};
        float uv[2]{};
        uint32_t color{0};
        float softness{0.0f};
    };

    // Draws the screen text queued with Renderer::submitText() onto the display image, with one
    // indexed draw over every glyph (shadows first, in the same draw). Owns the font atlas, the
    // material and the per-frame vertex and index buffers.
    class TextPass final : public RenderPass
    {
    public:
        // Bakes the Engine font atlas and creates the GPU resources.
        // context: the Vela context that owns them.
        // Returns: the pass, or an error if the font or a shader is missing or a GPU resource failed.
        [[nodiscard]] static Result<std::unique_ptr<TextPass>> create(vela::core::Context& context);

        TextPass(const TextPass&) = delete;
        TextPass& operator=(const TextPass&) = delete;
        TextPass(TextPass&&) = delete;
        TextPass& operator=(TextPass&&) = delete;

        // Queues text for the next frame (see Renderer::submitText()).
        // utf8:     the text; copied.
        // position: normalized screen position of the anchor.
        // style:    size, colour, alignment and shadow.
        void submit(std::string_view utf8, math::Vec2 position, const TextStyle& style);

        // Takes the text queued since the last frame for this frame's record() and empties the
        // queue, also on frames that are not recorded.
        void prepareFrame() override;

        // Lays the frame's text out for the target size, uploads the quads and draws them.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        // One submitText() call; its characters are text[offset, offset + length) of the queue.
        struct TextEntry
        {
            std::size_t offset{0};
            std::size_t length{0};
            math::Vec2 position{};
            TextStyle style{};
        };

        // Queued entries and the characters they point into.
        struct TextQueue
        {
            std::string text;
            std::vector<TextEntry> entries;

            // Forgets every entry, keeping the storage.
            void clear();
        };

        TextPass(FontAtlas&& atlas, vela::graphics::Material&& material, vela::graphics::DynamicBuffer&& vertexBuffer,
            vela::graphics::DynamicBuffer&& indexBuffer);

        // Lays out one entry: its glyph quads go to m_textVertices and, with a shadow, the shadow
        // quads (offset, dark, soft) to m_vertices, so every shadow draws before every text.
        // entry:  the text's position and style.
        // text:   the entry's characters.
        // width:  target width in pixels.
        // height: target height in pixels.
        void layoutEntry(const TextEntry& entry, std::string_view text, float width, float height);

        FontAtlas m_atlas;
        vela::graphics::Material m_material;
        vela::graphics::DynamicBuffer m_vertexBuffer;
        vela::graphics::DynamicBuffer m_indexBuffer;

        TextQueue m_queued;
        TextQueue m_frame;
        std::vector<TextVertex> m_vertices;
        std::vector<TextVertex> m_textVertices;
        std::vector<uint32_t> m_indices;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_TEXT_PASS_HPP
