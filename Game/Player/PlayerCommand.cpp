#include "Player/PlayerCommand.hpp"

#include "Player/WireBytes.hpp"
#include "Weapons/WeaponCatalog.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

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
        constexpr std::byte k_scopeBit{0x80};
        constexpr std::byte k_leanLeftBit{0x01};
        constexpr std::byte k_leanRightBit{0x02};
        constexpr std::byte k_interactHeldBit{0x04};
        constexpr std::byte k_flashlightBit{0x08};
        constexpr std::byte k_kickBit{0x10};
        constexpr std::byte k_opticBit{0x80};
        constexpr uint32_t k_weaponShift{5};
        constexpr std::byte k_weaponBits{0x60};
        constexpr float k_aimOffsetQuantization{8192.0f};
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
        constexpr std::size_t k_aimOffsetYawOffset{15};
        constexpr std::size_t k_aimOffsetPitchOffset{17};

        float axis(const platform::Input& input, platform::Key positive, platform::Key negative)
        {
            return (input.isKeyDown(positive) ? 1.0f : 0.0f) - (input.isKeyDown(negative) ? 1.0f : 0.0f);
        }

        std::byte quantizeAxis(float value)
        {
            const auto quantized = static_cast<int8_t>(std::lround(math::clamp(value, -1.0f, 1.0f) * k_moveQuantization));
            return std::bit_cast<std::byte>(quantized);
        }

        void writeAngle(std::span<std::byte> bytes, std::size_t offset, float radians)
        {
            const auto quantized = static_cast<int16_t>(std::clamp(std::lround(radians * k_aimOffsetQuantization),
                long{std::numeric_limits<int16_t>::min()},
                long{std::numeric_limits<int16_t>::max()}));
            const auto bits = std::bit_cast<uint16_t>(quantized);
            bytes[offset] = static_cast<std::byte>(bits & 0xFFu);
            bytes[offset + 1] = static_cast<std::byte>(bits >> 8);
        }

        float readAngle(std::span<const std::byte> bytes, std::size_t offset)
        {
            const auto bits = static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset])
                | (static_cast<uint16_t>(bytes[offset + 1]) << 8));
            return static_cast<float>(std::bit_cast<int16_t>(bits)) / k_aimOffsetQuantization;
        }

        void pickWeapon(LocalControls& controls, const WeaponStatsTable& weapons, const platform::Input& input)
        {
            WeaponKind wanted = controls.weapon;
            const float wheel = input.scrollDelta().y;

            if (input.wasKeyPressed(platform::Key::Num1))
            {
                wanted = WeaponKind::M4;
            }
            else if (input.wasKeyPressed(platform::Key::Num2))
            {
                wanted = WeaponKind::Glock;
            }
            else if (input.wasKeyPressed(platform::Key::Num3))
            {
                wanted = WeaponKind::Kriss;
            }
            else if (wheel != 0.0f)
            {
                const std::size_t step = wheel < 0.0f ? 1 : k_weaponCount - 1;
                wanted = static_cast<WeaponKind>((weaponIndex(controls.weapon) + step) % k_weaponCount);
            }

            if (wanted == controls.weapon)
            {
                return;
            }

            controls.switchSeconds = weaponSwitchSeconds(weapons, controls.weapon, wanted);
            controls.previousWeapon = controls.weapon;
            controls.weapon = wanted;
            controls.trigger.queued = 0;
            controls.trigger.mode = allowedFireMode(weaponStats(weapons, wanted), controls.trigger.mode);
        }

        float dequantizeAxis(std::byte value)
        {
            return math::clamp(static_cast<float>(std::bit_cast<int8_t>(value)) / k_moveQuantization, -1.0f, 1.0f);
        }
    } //namespace

    PlayerCommand sampleLocalCommand(LocalControls& controls, const ControlSettings& settings,
        const platform::Input& input, bool isCursorCaptured, const WeaponControl& weapon, float deltaSeconds)
    {
        const bool canRaise = weapon.zone != WeaponZone::Safe && !weapon.isHoldingToy;
        const bool wasLowered = weapon.isArmed && isWeaponLowered(controls.wantsLowered, weapon.zone, weapon.isHoldingToy);
        const bool isArmedWeapon = weapon.isArmed && !weapon.isHoldingToy;
        Trigger& trigger = controls.trigger;
        syncTriggerRounds(trigger, weapon.magazine, weapon.isReloading);
        const WeaponStatsTable& weapons = *weapon.weapons;
        controls.switchSeconds = std::max(controls.switchSeconds - deltaSeconds, 0.0f);
        bool isAiming = false;
        bool isFirePressed = false;

        if (isCursorCaptured)
        {
            const bool isAimHeld = weapon.isArmed && canRaise && input.isMouseButtonDown(platform::MouseButton::Right);
            const float sensitivity = lookSensitivity(settings, isAimHeld, controls.opticSensitivityScale);
            const math::Vec2 delta = input.mouseDelta();
            controls.yaw = std::remainder(controls.yaw - delta.x * sensitivity, 2.0f * math::k_pi);
            controls.pitch = math::clamp(controls.pitch - delta.y * sensitivity, -settings.maxPitch, settings.maxPitch);

            if (weapon.isArmed && weapon.zone == WeaponZone::Open && !weapon.isHoldingToy
                && input.wasKeyPressed(k_lowerWeaponKey))
            {
                controls.wantsLowered = !controls.wantsLowered;
            }

            if (weapon.isArmed)
            {
                pickWeapon(controls, weapons, input);
            }

            if (weapon.isArmed && input.wasKeyPressed(k_fireModeKey))
            {
                trigger.mode = nextWeaponFireMode(weaponStats(weapons, controls.weapon), trigger.mode);
                trigger.queued = 0;
            }

            if (input.wasMouseButtonPressed(platform::MouseButton::Left))
            {
                if (wasLowered && !weapon.isHoldingToy)
                {
                    controls.wantsLowered = controls.wantsLowered && !canRaise;
                }
                else if (!isArmedWeapon)
                {
                    ++controls.shotCount;
                }
                else if (trigger.rounds <= 0 && !weapon.isReloading)
                {
                    ++controls.reloadCount;
                }
                else
                {
                    isFirePressed = true;
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

                if (weapon.isArmed && weapon.canToggleFlashlight)
                {
                    controls.isFlashlightOn = !controls.isFlashlightOn;
                }
            }

            if (input.wasKeyPressed(platform::Key::R))
            {
                ++controls.reloadCount;
            }
        }

        bool isFireHeld = isCursorCaptured && input.isMouseButtonDown(platform::MouseButton::Left);

        if (weapon.scriptedFire)
        {
            isAiming = weapon.isArmed && canRaise && weapon.scriptedFire->isAiming;
            isFireHeld = weapon.scriptedFire->isHeld;
            const bool hasRounds = trigger.rounds > 0 || weapon.isReloading;
            isFirePressed = weapon.scriptedFire->isPressed && isArmedWeapon && !wasLowered && hasRounds;
            controls.reloadCount = static_cast<uint8_t>(controls.reloadCount
                + (weapon.scriptedFire->isPressed && isArmedWeapon && !hasRounds ? 1 : 0));
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
        const bool isCrouching = controls.isCrouchToggled || isCrouchHeld;
        const WeaponStats& stats = weaponStats(weapons, controls.weapon);
        trigger.mode = allowedFireMode(stats, trigger.mode);
        const bool canFire = (isCursorCaptured || weapon.scriptedFire) && isArmedWeapon && !wasLowered && weapon.isReady
            && !weapon.isReloading && controls.switchSeconds <= 0.0f;
        const uint8_t shots = stepTrigger(trigger, stats.fire, TriggerInput{.isPressed = isFirePressed,
            .isHeld = isFireHeld,
            .canFire = canFire,
            .deltaSeconds = deltaSeconds});

        for (uint8_t shot = 0; shot < shots; ++shot)
        {
            ++controls.shotCount;
            kickRecoil(controls.recoil, stats.recoil, isAiming, isCrouching);
        }

        const math::Vec2 settled = stepRecoil(controls.recoil, stats.recoil, deltaSeconds);
        controls.yaw = std::remainder(controls.yaw + settled.x, 2.0f * math::k_pi);
        controls.pitch = math::clamp(controls.pitch + settled.y, -settings.maxPitch, settings.maxPitch);
        const math::Vec2 recoil = controls.recoil.offset;

        return PlayerCommand{
            .move = {axis(input, platform::Key::D, platform::Key::A), axis(input, platform::Key::W, platform::Key::S)},
            .yaw = std::remainder(controls.yaw + recoil.x, 2.0f * math::k_pi),
            .pitch = math::clamp(controls.pitch + recoil.y, -settings.maxPitch, settings.maxPitch),
            .shotCount = controls.shotCount,
            .bulletTime = input.isKeyDown(platform::Key::LeftAlt)
                || (isCursorCaptured && input.isMouseButtonDown(platform::MouseButton::Middle)),
            .interactCount = controls.interactCount,
            .sprint = isSprintHeld,
            .aim = isAiming,
            .lower = controls.wantsLowered,
            .isWeaponUp = weapon.isArmed && !isWeaponLowered(controls.wantsLowered, weapon.zone, weapon.isHoldingToy),
            .crouch = isCrouching,
            .jump = input.isKeyDown(platform::Key::Space),
            .lean = static_cast<int8_t>(axis(input, platform::Key::E, platform::Key::Q)),
            .reloadCount = controls.reloadCount,
            .isInteractHeld = isCursorCaptured && input.isKeyDown(platform::Key::F),
            .aimOffset = weapon.isArmed ? weapon.aimOffset : math::Vec2{0.0f, 0.0f},
            .isFlashlightOn = weapon.isArmed && controls.isFlashlightOn,
            .kick = input.isKeyDown(platform::Key::V),
            .weapon = controls.weapon};
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
        command.kick = false;
        command.lean = 0;
        command.bulletTime = false;
    }

    float lookSensitivity(const ControlSettings& settings, bool isAiming, float opticScale)
    {
        return settings.mouseSensitivity * (isAiming ? settings.aimSensitivityScale * opticScale : 1.0f);
    }

    std::array<std::byte, k_playerCommandBytes> encodeCommand(const PlayerCommand& command)
    {
        std::array<std::byte, k_playerCommandBytes> bytes{};
        bytes[k_flagsOffset] = (command.bulletTime ? k_bulletTimeBit : std::byte{0})
            | (command.sprint ? k_sprintBit : std::byte{0}) | (command.aim ? k_aimBit : std::byte{0})
            | (command.lower ? k_lowerBit : std::byte{0}) | (command.isWeaponUp ? k_weaponUpBit : std::byte{0})
            | (command.crouch ? k_crouchBit : std::byte{0}) | (command.jump ? k_jumpBit : std::byte{0})
            | (command.optic == OpticKind::Scope4x ? k_scopeBit : std::byte{0});
        bytes[k_moreFlagsOffset] = (command.lean < 0 ? k_leanLeftBit : std::byte{0})
            | (command.lean > 0 ? k_leanRightBit : std::byte{0})
            | (command.isInteractHeld ? k_interactHeldBit : std::byte{0})
            | (command.isFlashlightOn ? k_flashlightBit : std::byte{0})
            | (command.kick ? k_kickBit : std::byte{0}) | (command.optic != OpticKind::None ? k_opticBit : std::byte{0})
            | static_cast<std::byte>(static_cast<uint32_t>(command.weapon) << k_weaponShift);
        writeAngle(bytes, k_aimOffsetYawOffset, command.aimOffset.x);
        writeAngle(bytes, k_aimOffsetPitchOffset, command.aimOffset.y);
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
            .isInteractHeld = (leanFlags & k_interactHeldBit) != std::byte{0},
            .aimOffset = {readAngle(bytes, k_aimOffsetYawOffset), readAngle(bytes, k_aimOffsetPitchOffset)},
            .isFlashlightOn = (leanFlags & k_flashlightBit) != std::byte{0},
            .kick = (leanFlags & k_kickBit) != std::byte{0},
            .optic = (leanFlags & k_opticBit) == std::byte{0} ? OpticKind::None
                : ((bytes[k_flagsOffset] & k_scopeBit) != std::byte{0} ? OpticKind::Scope4x : OpticKind::Holographic),
            .weapon = weaponFromBits(static_cast<uint32_t>(leanFlags & k_weaponBits) >> k_weaponShift)};
    }
} //namespace game
