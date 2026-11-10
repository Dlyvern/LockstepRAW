#ifndef LOCKSTEP_RENDER_OVERLAY_PASS_HPP
#define LOCKSTEP_RENDER_OVERLAY_PASS_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/Resources.hpp"
#include "Lockstep/Render/Text.hpp"

#include "FontAtlas.hpp"
#include "RenderPass.hpp"
#include "TextLayout.hpp"

#include <Vela/Core/Context.hpp>
#include <Vela/Graphics/DynamicBuffer.hpp>
#include <Vela/Graphics/Material.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::render
{
    struct RendererState;

    // Draws the screen overlay queued with Renderer::submitText(), submitRect() and submitImage()
    // onto the display image, in submission order: all quads go into one vertex buffer and are
    // drawn in as few indexed draws as the textures and clip rectangles allow. Owns the two Engine
    // font atlases, the material and the per-frame vertex and index buffers.
    class OverlayPass final : public RenderPass
    {
    public:
        // Bakes the Engine font atlases and creates the GPU resources.
        // context: the Vela context that owns them.
        // state:   the renderer state whose texture pool images are looked up in; must outlive the
        //          pass.
        // Returns: the pass, or an error if a font or a shader is missing or a GPU resource failed.
        [[nodiscard]] static Result<std::unique_ptr<OverlayPass>> create(vela::core::Context& context,
            const RendererState& state);

        OverlayPass(const OverlayPass&) = delete;
        OverlayPass& operator=(const OverlayPass&) = delete;
        OverlayPass(OverlayPass&&) = delete;
        OverlayPass& operator=(OverlayPass&&) = delete;

        // Queues text for the next frame (see Renderer::submitText()).
        // utf8:     the text; copied.
        // position: normalized screen position of the anchor.
        // style:    size, colour, alignment, shadow and font.
        void submitText(std::string_view utf8, math::Vec2 position, const TextStyle& style);

        // Queues a filled rectangle (see Renderer::submitRect()).
        // rect:  normalized rectangle.
        // color: sRGB colour.
        void submitRect(const ScreenRect& rect, const math::Vec4& color);

        // Queues an image (see Renderer::submitImage()).
        // texture: the texture, looked up when the frame is recorded.
        // rect:    normalized rectangle.
        // style:   tint and texture coordinates.
        void submitImage(TextureHandle texture, const ScreenRect& rect, const ImageStyle& style);

        // Sets the clip rectangle of later submissions (see Renderer::setScreenClip()).
        // clip: normalized rectangle, or nothing for the whole screen.
        void setClip(const std::optional<ScreenRect>& clip);

        // Measures text (see Renderer::measureText()).
        // utf8:    the text.
        // style:   size and font.
        // Returns: size in pixels at 1080p.
        [[nodiscard]] math::Vec2 measureText(std::string_view utf8, const TextStyle& style) const;

        // Takes the overlay queued since the last frame for this frame's record() and empties the
        // queue, also on frames that are not recorded.
        void prepareFrame() override;

        // Lays the frame's overlay out for the target size, uploads the quads and draws them.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        // What one queued item draws.
        enum class ItemKind : uint8_t
        {
            Text,
            Rect,
            Image
        };

        // One queued item. Text items point into the queue's characters; clip indexes the queue's
        // clip list (0 is the whole screen).
        struct OverlayItem
        {
            ItemKind kind{ItemKind::Rect};
            std::size_t textOffset{0};
            std::size_t textLength{0};
            math::Vec2 position{};
            TextStyle style{};
            ScreenRect rect{};
            math::Vec4 color{};
            TextureHandle texture{};
            ImageStyle image{};
            uint32_t clip{0};
        };

        // Queued items, the characters text items point into, and the clip rectangles.
        struct OverlayQueue
        {
            std::string text;
            std::vector<OverlayItem> items;
            std::vector<ScreenRect> clips;
            uint32_t currentClip{0};

            // Forgets everything, keeping the storage; the whole-screen clip is entry 0.
            void clear();
        };

        // A run of quads drawn with one texture and one clip rectangle.
        struct DrawBatch
        {
            const vela::graphics::Texture* texture{nullptr};
            uint32_t clip{0};
            uint32_t firstQuad{0};
            uint32_t quadCount{0};
        };

        OverlayPass(const RendererState& state, FontAtlas&& sans, FontAtlas&& mono,
            vela::graphics::Material&& material, vela::graphics::DynamicBuffer&& vertexBuffer,
            vela::graphics::DynamicBuffer&& indexBuffer);

        // Returns: the atlas of `font`.
        [[nodiscard]] const FontAtlas& atlas(TextFont font) const;

        // Appends one item's quads to m_vertices.
        // item:    the item.
        // width:   target width in pixels.
        // height:  target height in pixels.
        // Returns: the texture it needs (nullptr: any), or nothing if it is skipped.
        [[nodiscard]] std::optional<const vela::graphics::Texture*> appendItem(const OverlayItem& item, float width,
            float height);

        // Adds the quads appended since `firstQuad` to the batch list.
        // texture:   the texture they need (nullptr: any).
        // clip:      their clip index.
        // firstQuad: index of their first quad.
        void addToBatches(const vela::graphics::Texture* texture, uint32_t clip, uint32_t firstQuad);

        const RendererState& m_state;
        FontAtlas m_sans;
        FontAtlas m_mono;
        vela::graphics::Material m_material;
        vela::graphics::DynamicBuffer m_vertexBuffer;
        vela::graphics::DynamicBuffer m_indexBuffer;

        OverlayQueue m_queued;
        OverlayQueue m_frame;
        std::vector<OverlayVertex> m_vertices;
        std::vector<uint32_t> m_indices;
        std::vector<DrawBatch> m_batches;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_OVERLAY_PASS_HPP
