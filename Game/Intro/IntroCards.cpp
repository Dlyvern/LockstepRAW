#include "Intro/IntroCards.hpp"

#include "Gore/GoreRandom.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/UI/UiContext.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <iostream>
#include <string_view>
#include <vector>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;
        namespace render = lockstep::render;
        namespace ui = lockstep::ui;

        constexpr uint32_t k_noiseSize{256};
        constexpr uint32_t k_dotSize{64};
        constexpr math::Vec4 k_offWhite{0.85f, 0.84f, 0.80f, 1.0f};
        constexpr math::Vec4 k_dim{0.52f, 0.52f, 0.50f, 1.0f};
        constexpr math::Vec4 k_black{0.0f, 0.0f, 0.0f, 1.0f};
        constexpr math::Vec4 k_recRed{0.86f, 0.08f, 0.06f, 1.0f};
        constexpr float k_staticSeconds{0.38f};
        constexpr float k_flickerInStart{0.3f};
        constexpr float k_flickerInEnd{1.1f};
        constexpr float k_hudMargin{56.0f};
        constexpr std::string_view k_fmodLogo{"intro/fmod_logo.rgba"};
        constexpr std::string_view k_physxLogo{"intro/physx_logo.rgba"};
        constexpr std::size_t k_logoHeaderBytes{12};
        constexpr float k_fmodLogoSize{340.0f};
        constexpr float k_physxLogoWidth{520.0f};

        std::vector<std::byte> noisePixels()
        {
            GoreRandom random = goreRandom(91);
            std::vector<std::byte> pixels;
            pixels.reserve(static_cast<std::size_t>(k_noiseSize) * k_noiseSize * 4);

            for (uint32_t y = 0; y < k_noiseSize; ++y)
            {
                const float band = random.next() < 0.08f ? 1.35f : 1.0f;

                for (uint32_t x = 0; x < k_noiseSize; ++x)
                {
                    const float value = std::clamp(random.next() * random.next() * 1.6f * band, 0.0f, 1.0f);
                    const auto grey = std::byte{static_cast<uint8_t>(value * 255.0f)};
                    pixels.insert(pixels.end(), {grey, grey, grey, std::byte{255}});
                }
            }

            return pixels;
        }

        std::vector<std::byte> dotPixels()
        {
            std::vector<std::byte> pixels;
            pixels.reserve(static_cast<std::size_t>(k_dotSize) * k_dotSize * 4);
            const float half = static_cast<float>(k_dotSize) * 0.5f;

            for (uint32_t y = 0; y < k_dotSize; ++y)
            {
                for (uint32_t x = 0; x < k_dotSize; ++x)
                {
                    const float dx = static_cast<float>(x) + 0.5f - half;
                    const float dy = static_cast<float>(y) + 0.5f - half;
                    const float coverage = std::clamp(half - 1.0f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
                    pixels.insert(pixels.end(), {std::byte{255}, std::byte{255}, std::byte{255},
                                                    std::byte{static_cast<uint8_t>(coverage * 255.0f)}});
                }
            }

            return pixels;
        }

        uint32_t readLittleEndian(std::span<const std::byte> bytes)
        {
            uint32_t value = 0;

            for (std::size_t index = 0; index < 4; ++index)
            {
                value |= static_cast<uint32_t>(bytes[index]) << (8 * index);
            }

            return value;
        }

        IntroLogo loadLogo(render::Renderer& renderer, const lockstep::assets::FileSystem& files, std::string_view path)
        {
            auto bytes = files.read(path);

            if (!bytes)
            {
                std::clog << "Game: intro logo '" << path << "' not loaded: " << bytes.error().message << '\n';
                return {};
            }

            const std::span<const std::byte> data(bytes.value());

            if (data.size() < k_logoHeaderBytes || std::memcmp(data.data(), "RGBA", 4) != 0)
            {
                std::clog << "Game: intro logo '" << path << "' is not a raw RGBA image\n";
                return {};
            }

            const uint32_t width = readLittleEndian(data.subspan(4));
            const uint32_t height = readLittleEndian(data.subspan(8));

            if (width == 0 || height == 0
                || data.size() != k_logoHeaderBytes + static_cast<std::size_t>(width) * height * 4)
            {
                std::clog << "Game: intro logo '" << path << "' has a bad size\n";
                return {};
            }

            auto texture = renderer.createTexture({.pixels = data.subspan(k_logoHeaderBytes),
                .width = width,
                .height = height,
                .generateMips = true});

            if (!texture)
            {
                std::clog << "Game: intro logo '" << path << "' not created: " << texture.error().message << '\n';
                return {};
            }

            return IntroLogo{.texture = texture.value(), .width = width, .height = height};
        }

        void drawLogo(ui::UiContext& context, const IntroLogo& logo, math::Vec2 centre, float width)
        {
            const float height = width * static_cast<float>(logo.height) / static_cast<float>(logo.width);
            const math::Vec2 half{width * 0.5f, height * 0.5f};
            context.image(logo.texture, ui::Rect{.min = centre - half, .max = centre + half});
        }

        float hashUnit(uint64_t value)
        {
            GoreRandom random = goreRandom(static_cast<uint32_t>(value * 2654435761u));
            return random.next();
        }

        math::Vec4 withAlpha(math::Vec4 colour, float alpha)
        {
            colour.w *= alpha;
            return colour;
        }

        void drawStatic(ui::UiContext& context, const IntroCardTextures& textures, uint64_t frame, float opacity)
        {
            const math::Vec2 canvas = context.canvasSize();
            const math::Vec2 offset{hashUnit(frame) * 0.5f, hashUnit(frame + 7) * 0.5f};
            context.image(textures.noise, ui::Rect{.min = {0.0f, 0.0f}, .max = canvas},
                render::ImageStyle{.tint = {1.0f, 1.0f, 1.0f, opacity},
                    .uvMin = offset,
                    .uvMax = offset + math::Vec2{0.5f, 0.28f}});
        }

        float titleVisibility(float seconds, uint64_t frame)
        {
            if (seconds < k_flickerInStart)
            {
                return 0.0f;
            }

            if (seconds < k_flickerInEnd)
            {
                const float progress = (seconds - k_flickerInStart) / (k_flickerInEnd - k_flickerInStart);
                return hashUnit(frame * 3 + 1) < progress ? 1.0f : 0.15f * hashUnit(frame);
            }

            return hashUnit(frame * 5 + 2) < 0.04f ? 0.55f : 1.0f;
        }

        void drawCameraReadouts(ui::UiContext& context, const IntroCardTextures& textures, float seconds)
        {
            const math::Vec2 canvas = context.canvasSize();
            const bool isRecLit = std::fmod(seconds, 1.0f) < 0.6f;
            const float dotSize = 26.0f;
            const math::Vec2 dotCorner{k_hudMargin, k_hudMargin + 4.0f};

            if (isRecLit)
            {
                context.image(textures.dot, ui::Rect{.min = dotCorner, .max = dotCorner + math::Vec2{dotSize, dotSize}},
                    render::ImageStyle{.tint = k_recRed});
            }

            context.text("REC", {k_hudMargin + dotSize + 14.0f, k_hudMargin}, 32.0f, k_offWhite, render::TextAlign::Left,
                render::TextFont::Mono);

            const int clock = 47 * 60 + 13 + static_cast<int>(seconds);
            char stamp[48];
            std::snprintf(stamp, sizeof(stamp), "2026-11-14  02:%02d:%02d", clock / 60 % 60, clock % 60);
            context.text(stamp, {canvas.x - k_hudMargin, k_hudMargin}, 30.0f, k_offWhite, render::TextAlign::Right,
                render::TextFont::Mono);
            context.text("UNIT 07  CAM 2", {k_hudMargin, canvas.y - k_hudMargin - 30.0f}, 26.0f, k_dim,
                render::TextAlign::Left, render::TextFont::Mono);
        }
    } //namespace

    lockstep::Result<IntroCardTextures> createIntroCardTextures(render::Renderer& renderer,
        const lockstep::assets::FileSystem& files)
    {
        const std::vector<std::byte> noise = noisePixels();
        auto noiseTexture = renderer.createTexture({.pixels = noise,
            .width = k_noiseSize,
            .height = k_noiseSize,
            .filter = render::TextureFilter::Nearest});

        if (!noiseTexture)
        {
            return noiseTexture.error();
        }

        const std::vector<std::byte> dot = dotPixels();
        auto dotTexture = renderer.createTexture({.pixels = dot, .width = k_dotSize, .height = k_dotSize});

        if (!dotTexture)
        {
            renderer.destroyTexture(noiseTexture.value());
            return dotTexture.error();
        }

        return IntroCardTextures{.noise = noiseTexture.value(),
            .dot = dotTexture.value(),
            .fmod = loadLogo(renderer, files, k_fmodLogo),
            .physx = loadLogo(renderer, files, k_physxLogo)};
    }

    void destroyIntroCardTextures(IntroCardTextures& textures, render::Renderer& renderer)
    {
        renderer.destroyTexture(textures.noise);
        renderer.destroyTexture(textures.dot);
        renderer.destroyTexture(textures.fmod.texture);
        renderer.destroyTexture(textures.physx.texture);
        textures = {};
    }

    void drawIntroBackdrop(ui::UiContext& context)
    {
        context.fillRect(ui::Rect{.min = {0.0f, 0.0f}, .max = context.canvasSize()}, k_black);
    }

    void drawWarningCard(ui::UiContext& context)
    {
        const math::Vec2 canvas = context.canvasSize();
        const float centre = canvas.x * 0.5f;
        context.text("This game contains graphic violence, gore and flashing lights\n"
                     "that may affect players with photosensitive epilepsy.",
            {centre, canvas.y * 0.41f}, 40.0f, k_offWhite, render::TextAlign::Center, render::TextFont::Sans);
        context.text("Headphones recommended.", {centre, canvas.y * 0.56f}, 30.0f, k_dim, render::TextAlign::Center,
            render::TextFont::Sans);
    }

    void drawVelaCaption(ui::UiContext& context)
    {
        const math::Vec2 canvas = context.canvasSize();
        context.text("made with", {canvas.x * 0.5f, canvas.y * 0.27f}, 30.0f, k_offWhite, render::TextAlign::Center,
            render::TextFont::Sans);
    }

    void drawCreditsCard(ui::UiContext& context, const IntroCardTextures& textures)
    {
        const math::Vec2 canvas = context.canvasSize();
        const float centreY = canvas.y * 0.44f;
        const math::Vec2 fmodCentre{canvas.x * 0.5f - 250.0f, centreY};
        const math::Vec2 physxCentre{canvas.x * 0.5f + 310.0f, centreY};

        if (textures.fmod.texture.isValid())
        {
            drawLogo(context, textures.fmod, fmodCentre, k_fmodLogoSize);
        }
        else
        {
            context.text("FMOD", fmodCentre - math::Vec2{0.0f, 60.0f}, 104.0f, k_offWhite, render::TextAlign::Center,
                render::TextFont::Sans);
        }

        if (textures.physx.texture.isValid())
        {
            drawLogo(context, textures.physx, physxCentre, k_physxLogoWidth);
        }
        else
        {
            context.text("PhysX", physxCentre - math::Vec2{0.0f, 60.0f}, 104.0f, k_offWhite, render::TextAlign::Center,
                render::TextFont::Sans);
        }

        context.fillRect(ui::Rect{.min = {canvas.x * 0.5f - 1.0f, centreY - 80.0f},
                             .max = {canvas.x * 0.5f + 1.0f, centreY + 80.0f}},
            withAlpha(k_dim, 0.6f));
        context.text("Made with FMOD Studio by Firelight Technologies Pty Ltd.", {canvas.x * 0.5f, canvas.y * 0.72f},
            30.0f, k_dim, render::TextAlign::Center, render::TextFont::Sans);
    }

    void drawTitleCard(ui::UiContext& context, const IntroCardTextures& textures, float seconds, uint64_t frame)
    {
        drawIntroBackdrop(context);

        if (seconds >= k_titleCutSeconds)
        {
            return;
        }

        const math::Vec2 canvas = context.canvasSize();

        if (seconds < k_staticSeconds)
        {
            drawStatic(context, textures, frame, 0.85f - 0.5f * seconds / k_staticSeconds);
        }
        else if (hashUnit(frame * 11 + 3) < 0.05f)
        {
            drawStatic(context, textures, frame, 0.12f);
        }

        const float visibility = titleVisibility(seconds, frame);

        if (visibility > 0.0f)
        {
            const float jitter = visibility < 1.0f ? (hashUnit(frame + 13) - 0.5f) * 14.0f : 0.0f;
            context.text("L O C K S T E P", {canvas.x * 0.5f + jitter, canvas.y * 0.44f}, 116.0f,
                withAlpha(k_offWhite, visibility), render::TextAlign::Center, render::TextFont::Sans);
        }

        if (seconds >= k_staticSeconds)
        {
            drawCameraReadouts(context, textures, seconds);
        }
    }
} //namespace game
