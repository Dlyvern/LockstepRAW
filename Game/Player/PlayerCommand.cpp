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
        constexpr std::byte k_sprintBit{0x02};
        constexpr std::byte k_aimBit{0x04};
        constexpr std::byte k_lowerBit{0x08};
        constexpr std::byte k_weaponUpBit{0x10};
        constexpr std::byte k_crouchBit{0x20};
        constexpr std::byte k_jumpBit{0x40};
        constexpr std::byte k_leanLeftBit{0x01};
        constexpr std::byte k_leanRightBit{0x02};
        constexpr std::byte k_interactHeldBit{0x04};
        constexpr float k_moveQuantization{127.0f};
        constexpr std::size_t k_flagsOffset{0};
        constexpr std::size_t k_moveXOffset{1};
        constexpr std::size_t k_moveYOffset{2};
        constexpr std::size_t k_shotCountOffset{3};
        constexpr std::size_t k_yawOffset{4};
        constexpr std::size_t k_pitchOffset{8};
        constexpr std::size_t k_interactCountOffset{12};
        constexpr std::size_t k_moreFlagsOffset{13};
        constexpr std::size_t k_reloadCountOffset{14};

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
        const platform::Input& input, bool isCursorCaptured, const WeaponControl& weapon)
    {
        const bool canRaise = weapon.zone != WeaponZone::Safe && !weapon.isHoldingToy;
        const bool wasLowered = weapon.isArmed && isWeaponLowered(controls.wantsLowered, weapon.zone, weapon.isHoldingToy);
        bool isAiming = false;

        if (isCursorCaptured)
        {
            const bool isAimHeld = weapon.isArmed && canRaise && input.isMouseButtonDown(platform::MouseButton::Right);
            const float sensitivity = lookSensitivity(settings, isAimHeld);
            const math::Vec2 delta = input.mouseDelta();
            controls.yaw = std::remainder(controls.yaw - delta.x * sensitivity, 2.0f * math::k_pi);
            controls.pitch = math::clamp(controls.pitch - delta.y * sensitivity, -settings.maxPitch, settings.maxPitch);

            if (weapon.isArmed && weapon.zone == WeaponZone::Open && !weapon.isHoldingToy
                && input.wasKeyPressed(k_lowerWeaponKey))
            {
                controls.wantsLowered = !controls.wantsLowered;
            }

            if (input.wasMouseButtonPressed(platform::MouseButton::Left))
            {
                if (wasLowered && !weapon.isHoldingToy)
                {
                    controls.wantsLowered = controls.wantsLowered && !canRaise;
                }
                else if (weapon.isArmed && !weapon.isHoldingToy && !weapon.hasRounds)
                {
                    ++controls.reloadCount;
                }
                else if (weapon.isReady || weapon.isHoldingToy || !weapon.isArmed)
                {
                    ++controls.shotCount;
                }
            }

            isAiming = weapon.isArmed && canRaise && input.isMouseButtonDown(platform::MouseButton::Right);

            if (isAiming)
            {
                controls.wantsLowered = false;
            }

            if (input.wasKeyPressed(platform::Key::F))
            {
                ++controls.interactCount;
            }

            if (input.wasKeyPressed(platform::Key::R))
            {
                ++controls.reloadCount;
            }
        }

        const bool isSprintHeld = input.isKeyDown(platform::Key::LeftShift) || input.isKeyDown(platform::Key::RightShift);
        const bool isSprintPressed = input.wasKeyPressed(platform::Key::LeftShift)
            || input.wasKeyPressed(platform::Key::RightShift);

        if (input.wasKeyPressed(platform::Key::C))
        {
            controls.isCrouchToggled = !controls.isCrouchToggled;
        }
        else if (isSprintPressed || input.wasKeyPressed(platform::Key::Space))
        {
            controls.isCrouchToggled = false;
        }

        const bool isCrouchHeld = input.isKeyDown(platform::Key::LeftControl)
            || input.isKeyDown(platform::Key::RightControl);

        return PlayerCommand{
            .move = {axis(input, platform::Key::D, platform::Key::A), axis(input, platform::Key::W, platform::Key::S)},
            .yaw = controls.yaw,
            .pitch = controls.pitch,
            .shotCount = controls.shotCount,
            .bulletTime = input.isKeyDown(platform::Key::LeftAlt)
                || (isCursorCaptured && input.isMouseButtonDown(platform::MouseButton::Middle)),
            .interactCount = controls.interactCount,
            .sprint = isSprintHeld,
            .aim = isAiming,
            .lower = controls.wantsLowered,
            .isWeaponUp = weapon.isArmed && !isWeaponLowered(controls.wantsLowered, weapon.zone, weapon.isHoldingToy),
            .crouch = controls.isCrouchToggled || isCrouchHeld,
            .jump = input.isKeyDown(platform::Key::Space),
            .lean = static_cast<int8_t>(axis(input, platform::Key::E, platform::Key::Q)),
            .reloadCount = controls.reloadCount,
            .isInteractHeld = isCursorCaptured && input.isKeyDown(platform::Key::F)};
    }

    void restrictDownedCommand(PlayerCommand& command, float crawlScale)
    {
        const float moveLength = math::length(command.move);

        if (moveLength > crawlScale && moveLength > 0.0f)
        {
            command.move = command.move * (crawlScale / moveLength);
        }

        command.sprint = false;
        command.aim = false;
        command.isWeaponUp = false;
        command.crouch = true;
        command.jump = false;
        command.lean = 0;
        command.bulletTime = false;
    }

    float lookSensitivity(const ControlSettings& settings, bool isAiming)
    {
        return settings.mouseSensitivity * (isAiming ? settings.aimSensitivityScale : 1.0f);
    }

    std::array<std::byte, k_playerCommandBytes> encodeCommand(const PlayerCommand& command)
    {
        std::array<std::byte, k_playerCommandBytes> bytes{};
        bytes[k_flagsOffset] = (command.bulletTime ? k_bulletTimeBit : std::byte{0})
            | (command.sprint ? k_sprintBit : std::byte{0}) | (command.aim ? k_aimBit : std::byte{0})
            | (command.lower ? k_lowerBit : std::byte{0}) | (command.isWeaponUp ? k_weaponUpBit : std::byte{0})
            | (command.crouch ? k_crouchBit : std::byte{0}) | (command.jump ? k_jumpBit : std::byte{0});
        bytes[k_moreFlagsOffset] = (command.lean < 0 ? k_leanLeftBit : std::byte{0})
            | (command.lean > 0 ? k_leanRightBit : std::byte{0})
            | (command.isInteractHeld ? k_interactHeldBit : std::byte{0});
        bytes[k_reloadCountOffset] = static_cast<std::byte>(command.reloadCount);
        bytes[k_moveXOffset] = quantizeAxis(command.move.x);
        bytes[k_moveYOffset] = quantizeAxis(command.move.y);
        bytes[k_shotCountOffset] = static_cast<std::byte>(command.shotCount);
        writeFloat(bytes, k_yawOffset, command.yaw);
        writeFloat(bytes, k_pitchOffset, command.pitch);
        bytes[k_interactCountOffset] = static_cast<std::byte>(command.interactCount);

        return bytes;
    }

    std::optional<PlayerCommand> decodeCommand(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_playerCommandBytes)
        {
            return std::nullopt;
        }

        const std::byte leanFlags = bytes[k_moreFlagsOffset];
        const bool isLeaningLeft = (leanFlags & k_leanLeftBit) != std::byte{0};
        const bool isLeaningRight = (leanFlags & k_leanRightBit) != std::byte{0};
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
            .bulletTime = (bytes[k_flagsOffset] & k_bulletTimeBit) != std::byte{0},
            .interactCount = static_cast<uint8_t>(bytes[k_interactCountOffset]),
            .sprint = (bytes[k_flagsOffset] & k_sprintBit) != std::byte{0},
            .aim = (bytes[k_flagsOffset] & k_aimBit) != std::byte{0},
            .lower = (bytes[k_flagsOffset] & k_lowerBit) != std::byte{0},
            .isWeaponUp = (bytes[k_flagsOffset] & k_weaponUpBit) != std::byte{0},
            .crouch = (bytes[k_flagsOffset] & k_crouchBit) != std::byte{0},
            .jump = (bytes[k_flagsOffset] & k_jumpBit) != std::byte{0},
            .lean = static_cast<int8_t>((isLeaningRight ? 1 : 0) - (isLeaningLeft ? 1 : 0)),
            .reloadCount = static_cast<uint8_t>(bytes[k_reloadCountOffset]),
            .isInteractHeld = (leanFlags & k_interactHeldBit) != std::byte{0}};
    }
} //namespace game
