#include "Player/PlayerCommand.hpp"

#include "Player/WireBytes.hpp"

#include <bit>
#include <cmath>

namespace game
{
    namespace
    {
        namespace platform = lockstep::platform;

        constexpr std::byte k_bulletTimeBit{0x01};
        constexpr float k_moveQuantization{127.0f};
        constexpr std::size_t k_flagsOffset{0};
        constexpr std::size_t k_moveXOffset{1};
        constexpr std::size_t k_moveYOffset{2};
        constexpr std::size_t k_shotCountOffset{3};
        constexpr std::size_t k_yawOffset{4};
        constexpr std::size_t k_pitchOffset{8};

        float axis(const platform::Input& input, platform::Key positive, platform::Key negative)
        {
            return (input.isKeyDown(positive) ? 1.0f : 0.0f) - (input.isKeyDown(negative) ? 1.0f : 0.0f);
        }

        std::byte quantizeAxis(float value)
        {
            const auto quantized = static_cast<int8_t>(std::lround(math::clamp(value, -1.0f, 1.0f) * k_moveQuantization));
            return std::bit_cast<std::byte>(quantized);
        }

        float dequantizeAxis(std::byte value)
        {
            return math::clamp(static_cast<float>(std::bit_cast<int8_t>(value)) / k_moveQuantization, -1.0f, 1.0f);
        }
    } //namespace

    PlayerCommand sampleLocalCommand(LocalControls& controls, const ControlSettings& settings,
        const platform::Input& input, bool isCursorCaptured)
    {
        if (isCursorCaptured)
        {
            const math::Vec2 delta = input.mouseDelta();
            controls.yaw = std::remainder(controls.yaw - delta.x * settings.mouseSensitivity, 2.0f * math::k_pi);
            controls.pitch = math::clamp(controls.pitch - delta.y * settings.mouseSensitivity, -settings.maxPitch,
                settings.maxPitch);

            if (input.wasMouseButtonPressed(platform::MouseButton::Left))
            {
                ++controls.shotCount;
            }
        }

        return PlayerCommand{
            .move = {axis(input, platform::Key::D, platform::Key::A), axis(input, platform::Key::W, platform::Key::S)},
            .yaw = controls.yaw,
            .pitch = controls.pitch,
            .shotCount = controls.shotCount,
            .bulletTime = input.isKeyDown(platform::Key::Space)};
    }

    std::array<std::byte, k_playerCommandBytes> encodeCommand(const PlayerCommand& command)
    {
        std::array<std::byte, k_playerCommandBytes> bytes{};
        bytes[k_flagsOffset] = command.bulletTime ? k_bulletTimeBit : std::byte{0};
        bytes[k_moveXOffset] = quantizeAxis(command.move.x);
        bytes[k_moveYOffset] = quantizeAxis(command.move.y);
        bytes[k_shotCountOffset] = static_cast<std::byte>(command.shotCount);
        writeFloat(bytes, k_yawOffset, command.yaw);
        writeFloat(bytes, k_pitchOffset, command.pitch);

        return bytes;
    }

    std::optional<PlayerCommand> decodeCommand(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_playerCommandBytes)
        {
            return std::nullopt;
        }

        const float yaw = readFloat(bytes, k_yawOffset);
        const float pitch = readFloat(bytes, k_pitchOffset);

        if (!std::isfinite(yaw) || !std::isfinite(pitch))
        {
            return std::nullopt;
        }

        return PlayerCommand{
            .move = {dequantizeAxis(bytes[k_moveXOffset]), dequantizeAxis(bytes[k_moveYOffset])},
            .yaw = yaw,
            .pitch = pitch,
            .shotCount = static_cast<uint8_t>(bytes[k_shotCountOffset]),
            .bulletTime = (bytes[k_flagsOffset] & k_bulletTimeBit) != std::byte{0}};
    }
} //namespace game
