#include "FontAtlas.hpp"

#include "VelaConversions.hpp"

#include <Vela/Assets/Resources.hpp>
#include <Vela/Graphics/ImageData.hpp>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <thread>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_fontDirectory{"fonts"};
        constexpr char32_t k_fallbackCodepoint{U'?'};
        constexpr int k_glyphPadding{static_cast<int>(FontAtlas::k_fontEdgeValue / FontAtlas::k_fontDistanceScale)};
        constexpr int k_atlasWidth{1024};
        constexpr int k_glyphGap{1};

        struct CodepointRange
        {
            char32_t first;
            char32_t last;
        };

        constexpr CodepointRange k_codepointRanges[]{
            {0x0020, 0x007E},
            {0x00A0, 0x017F},
            {0x0400, 0x045F},
            {0x2010, 0x2027},
            {0x2030, 0x203A},
            {0x20AC, 0x20AC},
            {0x2116, 0x2116},
            {0x2122, 0x2122}};

        constexpr char32_t k_lastCodepoint{0x2122};

        struct BakedGlyph
        {
            char32_t codepoint{0};
            int glyphIndex{0};
            int width{0};
            int height{0};
            int offsetX{0};
            int offsetY{0};
            int atlasX{0};
            int atlasY{0};
            float advance{0.0f};
            std::vector<unsigned char> distances;
        };

        Result<std::vector<unsigned char>> readFontFile(std::string_view fileName)
        {
            const std::filesystem::path directory = vela::assets::resources::executableDirectory();

            if (directory.empty())
            {
                return Error{ErrorCode::PlatformFailure, "Could not resolve the executable directory"};
            }

            const std::filesystem::path path = directory / k_fontDirectory / fileName;
            std::ifstream file(path, std::ios::binary);

            if (!file)
            {
                return Error{ErrorCode::AssetLoadFailed, "Could not open font " + path.string()};
            }

            std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

            if (bytes.empty())
            {
                return Error{ErrorCode::AssetLoadFailed, "Font file is empty: " + path.string()};
            }

            return bytes;
        }

        void bakeGlyph(const stbtt_fontinfo& info, float scale, BakedGlyph& baked)
        {
            const int glyphIndex = baked.glyphIndex;
            int advance = 0;
            int leftSideBearing = 0;
            stbtt_GetGlyphHMetrics(&info, glyphIndex, &advance, &leftSideBearing);
            baked.advance = static_cast<float>(advance) * scale;

            unsigned char* distances = stbtt_GetGlyphSDF(&info, scale, glyphIndex, k_glyphPadding,
                FontAtlas::k_fontEdgeValue, FontAtlas::k_fontDistanceScale, &baked.width, &baked.height,
                &baked.offsetX, &baked.offsetY);

            if (distances == nullptr)
            {
                baked.width = 0;
                baked.height = 0;
                return;
            }

            const std::size_t size = static_cast<std::size_t>(baked.width) * static_cast<std::size_t>(baked.height);
            baked.distances.assign(distances, distances + size);
            stbtt_FreeSDF(distances, nullptr);
        }

        void bakeGlyphs(const stbtt_fontinfo& info, float scale, std::vector<BakedGlyph>& glyphs)
        {
            if (glyphs.empty())
            {
                return;
            }

            const std::size_t threadCount = std::clamp<std::size_t>(std::thread::hardware_concurrency(), 1, glyphs.size());
            std::vector<std::jthread> workers;
            workers.reserve(threadCount - 1);

            const auto bakeEvery = [&info, scale, &glyphs, threadCount](std::size_t first)
            {
                for (std::size_t index = first; index < glyphs.size(); index += threadCount)
                {
                    bakeGlyph(info, scale, glyphs[index]);
                }
            };

            for (std::size_t worker = 1; worker < threadCount; ++worker)
            {
                workers.emplace_back(bakeEvery, worker);
            }

            bakeEvery(0);
        }

        int packGlyphs(std::vector<BakedGlyph>& glyphs)
        {
            std::vector<BakedGlyph*> byHeight;
            byHeight.reserve(glyphs.size());

            for (BakedGlyph& glyph : glyphs)
            {
                byHeight.push_back(&glyph);
            }

            std::sort(byHeight.begin(), byHeight.end(), [](const BakedGlyph* left, const BakedGlyph* right)
            {
                return left->height > right->height;
            });

            int x = 0;
            int y = 0;
            int shelfHeight = 0;

            for (BakedGlyph* glyph : byHeight)
            {
                if (glyph->width == 0)
                {
                    continue;
                }

                if (x + glyph->width > k_atlasWidth)
                {
                    x = 0;
                    y += shelfHeight + k_glyphGap;
                    shelfHeight = 0;
                }

                glyph->atlasX = x;
                glyph->atlasY = y;
                x += glyph->width + k_glyphGap;
                shelfHeight = std::max(shelfHeight, glyph->height);
            }

            int height = 1;

            while (height < y + shelfHeight)
            {
                height *= 2;
            }

            return height;
        }

        AtlasGlyph toAtlasGlyph(const BakedGlyph& baked, int atlasHeight)
        {
            const float width = static_cast<float>(k_atlasWidth);
            const float height = static_cast<float>(atlasHeight);

            AtlasGlyph glyph{};
            glyph.uvMin[0] = static_cast<float>(baked.atlasX) / width;
            glyph.uvMin[1] = static_cast<float>(baked.atlasY) / height;
            glyph.uvMax[0] = static_cast<float>(baked.atlasX + baked.width) / width;
            glyph.uvMax[1] = static_cast<float>(baked.atlasY + baked.height) / height;
            glyph.offsetMin[0] = static_cast<float>(baked.offsetX);
            glyph.offsetMin[1] = static_cast<float>(baked.offsetY);
            glyph.offsetMax[0] = static_cast<float>(baked.offsetX + baked.width);
            glyph.offsetMax[1] = static_cast<float>(baked.offsetY + baked.height);
            glyph.advance = baked.advance;
            glyph.glyphIndex = baked.glyphIndex;

            return glyph;
        }

        vela::graphics::SamplerDescription atlasSampler()
        {
            vela::graphics::SamplerDescription sampler{};
            sampler.addressU = vela::graphics::AddressMode::ClampToEdge;
            sampler.addressV = vela::graphics::AddressMode::ClampToEdge;
            sampler.addressW = vela::graphics::AddressMode::ClampToEdge;
            sampler.maxLod = 0.0f;

            return sampler;
        }
    } //namespace

    struct FontAtlas::FontData
    {
        std::vector<unsigned char> bytes;
        stbtt_fontinfo info{};
        float scale{1.0f};
        float ascent{0.0f};
        float lineHeight{0.0f};
        std::vector<AtlasGlyph> glyphs;
        std::vector<int32_t> glyphByCodepoint;
        std::size_t fallback{0};
    };

    Result<FontAtlas> FontAtlas::create(vela::core::Context& context, std::string_view fileName)
    {
        auto bytes = readFontFile(fileName);

        if (!bytes)
        {
            return bytes.error();
        }

        auto font = std::make_unique<FontData>();
        font->bytes = std::move(bytes).value();

        const int offset = stbtt_GetFontOffsetForIndex(font->bytes.data(), 0);

        if (offset < 0 || stbtt_InitFont(&font->info, font->bytes.data(), offset) == 0)
        {
            return Error{ErrorCode::AssetLoadFailed, "Not a TrueType font: " + std::string(fileName)};
        }

        font->scale = stbtt_ScaleForPixelHeight(&font->info, k_bakeSize);

        int ascent = 0;
        int descent = 0;
        int lineGap = 0;
        stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &lineGap);
        font->ascent = static_cast<float>(ascent) * font->scale;
        font->lineHeight = static_cast<float>(ascent - descent + lineGap) * font->scale;

        std::vector<BakedGlyph> baked;

        for (const CodepointRange& range : k_codepointRanges)
        {
            for (char32_t codepoint = range.first; codepoint <= range.last; ++codepoint)
            {
                const int glyphIndex = stbtt_FindGlyphIndex(&font->info, static_cast<int>(codepoint));

                if (glyphIndex != 0)
                {
                    BakedGlyph glyph{};
                    glyph.codepoint = codepoint;
                    glyph.glyphIndex = glyphIndex;
                    baked.push_back(std::move(glyph));
                }
            }
        }

        bakeGlyphs(font->info, font->scale, baked);

        const int atlasHeight = packGlyphs(baked);
        std::vector<std::byte> pixels(static_cast<std::size_t>(k_atlasWidth) * static_cast<std::size_t>(atlasHeight));
        font->glyphByCodepoint.assign(static_cast<std::size_t>(k_lastCodepoint) + 1, -1);
        font->glyphs.reserve(baked.size());

        for (const BakedGlyph& glyph : baked)
        {
            for (int row = 0; row < glyph.height; ++row)
            {
                const auto* source = reinterpret_cast<const std::byte*>(glyph.distances.data())
                    + static_cast<std::size_t>(row) * static_cast<std::size_t>(glyph.width);
                std::byte* target = pixels.data()
                    + static_cast<std::size_t>(glyph.atlasY + row) * static_cast<std::size_t>(k_atlasWidth)
                    + static_cast<std::size_t>(glyph.atlasX);
                std::copy_n(source, glyph.width, target);
            }

            font->glyphByCodepoint[glyph.codepoint] = static_cast<int32_t>(font->glyphs.size());
            font->glyphs.push_back(toAtlasGlyph(glyph, atlasHeight));
        }

        const int32_t fallback = font->glyphByCodepoint[k_fallbackCodepoint];

        if (fallback < 0)
        {
            return Error{ErrorCode::AssetLoadFailed, "Font has no '?' glyph: " + std::string(fileName)};
        }

        font->fallback = static_cast<std::size_t>(fallback);

        vela::graphics::ImageData image{};
        image.pixels = pixels;
        image.width = static_cast<uint32_t>(k_atlasWidth);
        image.height = static_cast<uint32_t>(atlasHeight);
        image.format = vela::graphics::TextureFormat::R8Unorm;

        auto texture = vela::graphics::Texture::create(context, image, atlasSampler());

        if (!texture)
        {
            return toError(texture.error());
        }

        return FontAtlas(std::move(font), std::move(texture).value());
    }

    FontAtlas::FontAtlas(std::unique_ptr<FontData> font, vela::graphics::Texture&& texture)
        : m_font(std::move(font)),
          m_texture(std::move(texture))
    {
    }

    FontAtlas::~FontAtlas() = default;

    FontAtlas::FontAtlas(FontAtlas&& other) noexcept = default;

    FontAtlas& FontAtlas::operator=(FontAtlas&& other) noexcept = default;

    const AtlasGlyph& FontAtlas::glyph(char32_t codepoint) const
    {
        if (codepoint < m_font->glyphByCodepoint.size())
        {
            const int32_t index = m_font->glyphByCodepoint[codepoint];

            if (index >= 0)
            {
                return m_font->glyphs[static_cast<std::size_t>(index)];
            }
        }

        return m_font->glyphs[m_font->fallback];
    }

    float FontAtlas::kerning(const AtlasGlyph& left, const AtlasGlyph& right) const
    {
        return static_cast<float>(stbtt_GetGlyphKernAdvance(&m_font->info, left.glyphIndex, right.glyphIndex))
            * m_font->scale;
    }

    float FontAtlas::ascent() const
    {
        return m_font->ascent;
    }

    float FontAtlas::lineHeight() const
    {
        return m_font->lineHeight;
    }

    const vela::graphics::Texture& FontAtlas::texture() const
    {
        return m_texture;
    }
} //namespace lockstep::render
