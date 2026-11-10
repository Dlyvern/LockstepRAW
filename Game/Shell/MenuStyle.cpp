#include "Shell/MenuStyle.hpp"

#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <span>
#include <vector>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;
        namespace ui = lockstep::ui;

        constexpr uint32_t k_grainSize{256};
        constexpr uint32_t k_vignetteSize{128};
        constexpr uint32_t k_dotSize{32};
        constexpr float k_grainTile{256.0f};
        constexpr float k_grainOpacity{0.035f};
        constexpr float k_scanlineOpacity{0.11f};
        constexpr uint32_t k_scanlinePeriod{3};
        constexpr std::size_t k_gradientBands{40};
        constexpr float k_bracketLength{46.0f};
        constexpr float k_bracketThickness{2.0f};
        constexpr float k_frameMargin{34.0f};
        constexpr float k_frameTextSize{20.0f};
        constexpr float k_titleSize{58.0f};
        constexpr float k_subtitleSize{20.0f};
        constexpr uint32_t k_startSecondOfDay{20 * 3600 + 47 * 60 + 13};

        uint32_t nextRandom(uint32_t& state)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        render::TextureHandle createTexture(render::Renderer& renderer, const std::vector<std::byte>& pixels,
            uint32_t size, render::TextureFilter filter, const char* name)
        {
            auto texture = renderer.createTexture(render::TextureDescription{.pixels = pixels, .width = size,
                .height = size, .format = render::TextureFormat::Rgba8Srgb, .filter = filter, .levels = 1,
                .generateMips = false});

            if (!texture)
            {
                std::clog << "Game: menu " << name << " texture not created: " << texture.error().message << '\n';
                return {};
            }

            return texture.value();
        }

        std::vector<std::byte> grainPixels()
        {
            std::vector<std::byte> pixels(static_cast<std::size_t>(k_grainSize) * k_grainSize * 4);
            uint32_t random = 0x9E3779B9u;

            for (std::size_t index = 0; index < pixels.size(); index += 4)
            {
                const auto grey = static_cast<std::byte>(nextRandom(random) & 0xFFu);
                pixels[index] = grey;
                pixels[index + 1] = grey;
                pixels[index + 2] = grey;
                pixels[index + 3] = std::byte{0xFF};
            }

            return pixels;
        }

        std::vector<std::byte> radialPixels(uint32_t size, float inner, float outer, bool isDot)
        {
            std::vector<std::byte> pixels(static_cast<std::size_t>(size) * size * 4);
            const float half = static_cast<float>(size) * 0.5f;

            for (uint32_t y = 0; y < size; ++y)
            {
                for (uint32_t x = 0; x < size; ++x)
                {
                    const float dx = (static_cast<float>(x) + 0.5f - half) / half;
                    const float dy = (static_cast<float>(y) + 0.5f - half) / half;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    const float edge = std::clamp((distance - inner) / (outer - inner), 0.0f, 1.0f);
                    const float alpha = isDot ? 1.0f - edge : edge * edge;
                    const std::size_t index = (static_cast<std::size_t>(y) * size + x) * 4;
                    const auto colour = static_cast<std::byte>(isDot ? 0xFF : 0x00);
                    pixels[index] = colour;
                    pixels[index + 1] = colour;
                    pixels[index + 2] = colour;
                    pixels[index + 3] = static_cast<std::byte>(std::lround(alpha * 255.0f));
                }
            }

            return pixels;
        }

        void drawBracket(ui::UiContext& context, math::Vec2 corner, float xDirection, float yDirection)
        {
            const float horizontalX = xDirection > 0.0f ? corner.x : corner.x - k_bracketLength;
            const float verticalY = yDirection > 0.0f ? corner.y : corner.y - k_bracketLength;
            const float lineY = yDirection > 0.0f ? corner.y : corner.y - k_bracketThickness;
            const float lineX = xDirection > 0.0f ? corner.x : corner.x - k_bracketThickness;
            context.fillRect(ui::rectAt({horizontalX, lineY}, {k_bracketLength, k_bracketThickness}), k_menuLine);
            context.fillRect(ui::rectAt({lineX, verticalY}, {k_bracketThickness, k_bracketLength}), k_menuLine);
        }

        std::string clockText(float seconds)
        {
            const auto total = static_cast<uint32_t>(seconds) + k_startSecondOfDay;
            char text[16]{};
            std::snprintf(text, sizeof(text), "%02u:%02u:%02u", (total / 3600) % 24, (total / 60) % 60, total % 60);

            return text;
        }
    } //namespace

    MenuStyle createMenuStyle(render::Renderer& renderer)
    {
        MenuStyle style{};
        style.grain = createTexture(renderer, grainPixels(), k_grainSize, render::TextureFilter::Nearest, "grain");
        style.vignette = createTexture(renderer, radialPixels(k_vignetteSize, 0.55f, 1.45f, false), k_vignetteSize,
            render::TextureFilter::Linear, "vignette");
        style.dot = createTexture(renderer, radialPixels(k_dotSize, 0.75f, 0.95f, true), k_dotSize,
            render::TextureFilter::Linear, "dot");

        return style;
    }

    void applyMenuTheme(ui::UiContext& context)
    {
        ui::Theme& theme = context.theme();
        theme.font = render::TextFont::Mono;
        theme.textSize = 25.0f;
        theme.text = k_menuText;
        theme.textDim = k_menuDim;
        theme.accent = k_menuAccent;
        theme.rowFill = math::Vec4{0.0f, 0.0f, 0.0f, 0.0f};
        theme.rowFocusFill = math::Vec4{0.86f, 0.85f, 0.80f, 0.09f};
        theme.frame = k_menuLine;
        theme.textShadow = true;
    }

    void drawMenuGradient(ui::UiContext& context)
    {
        const math::Vec2 canvas = context.canvasSize();
        const float band = canvas.y / static_cast<float>(k_gradientBands);

        for (std::size_t index = 0; index < k_gradientBands; ++index)
        {
            const float share = static_cast<float>(index) / static_cast<float>(k_gradientBands - 1);
            const float shade = 0.075f * (1.0f - share) + 0.012f;
            context.fillRect(ui::rectAt({0.0f, band * static_cast<float>(index)}, {canvas.x, band + 1.0f}),
                math::Vec4{shade * 0.9f, shade * 0.95f, shade * 1.1f, 1.0f});
        }
    }

    void drawMenuFrame(MenuStyle& style, ui::UiContext& context, float frameSeconds, std::string_view label)
    {
        style.seconds += frameSeconds;
        const math::Vec2 canvas = context.canvasSize();
        context.image(style.vignette, ui::rectAt({0.0f, 0.0f}, canvas), render::ImageStyle{.tint = {1.0f, 1.0f, 1.0f, 0.85f}});

        const float left = k_frameMargin;
        const float top = k_frameMargin;
        const float right = canvas.x - k_frameMargin;
        const float bottom = canvas.y - k_frameMargin;
        drawBracket(context, {left, top}, 1.0f, 1.0f);
        drawBracket(context, {right, top}, -1.0f, 1.0f);
        drawBracket(context, {left, bottom}, 1.0f, -1.0f);
        drawBracket(context, {right, bottom}, -1.0f, -1.0f);

        const float textTop = top + 16.0f;
        const bool isLit = std::fmod(style.seconds, 1.2f) < 0.8f;

        if (isLit)
        {
            context.image(style.dot, ui::rectAt({right - 130.0f, textTop + 2.0f}, {16.0f, 16.0f}),
                render::ImageStyle{.tint = k_menuAccent});
        }

        context.text("REC", {right - 106.0f, textTop}, k_frameTextSize, k_menuAccent);
        context.text(clockText(style.seconds), {right - 20.0f, textTop + 26.0f}, k_frameTextSize, k_menuDim,
            render::TextAlign::Right);
        context.text(label, {left + 20.0f, textTop}, k_frameTextSize, k_menuDim);
        context.text("BAT 71%   ISO 3200   1/50", {left + 20.0f, bottom - 38.0f}, k_frameTextSize, k_menuDim);
    }

    void drawMenuNoise(MenuStyle& style, ui::UiContext& context, const render::Renderer& renderer)
    {
        const math::Vec2 canvas = context.canvasSize();
        const math::Vec2 screen = renderer.screenSize();

        if (screen.y > 0.0f)
        {
            const float pixel = canvas.y / screen.y;
            const auto lines = static_cast<uint32_t>(screen.y) / k_scanlinePeriod;

            for (uint32_t line = 0; line < lines; ++line)
            {
                const float y = static_cast<float>(line * k_scanlinePeriod) * pixel;
                context.fillRect(ui::rectAt({0.0f, y}, {canvas.x, pixel}),
                    math::Vec4{0.0f, 0.0f, 0.0f, k_scanlineOpacity});
            }
        }

        if (!style.grain.isValid())
        {
            return;
        }

        const float offsetX = static_cast<float>(nextRandom(style.random) % 256u);
        const float offsetY = static_cast<float>(nextRandom(style.random) % 256u);

        for (float y = -offsetY; y < canvas.y; y += k_grainTile)
        {
            for (float x = -offsetX; x < canvas.x; x += k_grainTile)
            {
                context.image(style.grain, ui::rectAt({x, y}, {k_grainTile, k_grainTile}),
                    render::ImageStyle{.tint = {1.0f, 1.0f, 1.0f, k_grainOpacity}});
            }
        }
    }

    void drawMenuTitle(ui::UiContext& context, math::Vec2 position, std::string_view title, std::string_view subtitle)
    {
        context.text(title, position, k_titleSize, k_menuText);
        const float ruleY = position.y + k_titleSize + 14.0f;
        context.fillRect(ui::rectAt({position.x, ruleY}, {44.0f, 3.0f}), k_menuAccent);
        context.fillRect(ui::rectAt({position.x + 52.0f, ruleY + 1.0f}, {420.0f, 1.0f}), k_menuLine);

        if (!subtitle.empty())
        {
            context.text(subtitle, {position.x, ruleY + 14.0f}, k_subtitleSize, k_menuDim);
        }
    }

    void drawMenuPanel(ui::UiContext& context, const ui::Rect& rect)
    {
        context.fillRect(rect, k_menuPanel);
        context.strokeRect(rect, math::Vec4{k_menuLine.x, k_menuLine.y, k_menuLine.z, 0.18f}, 1.0f);
    }
} //namespace game
