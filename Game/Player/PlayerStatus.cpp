#include "Player/PlayerStatus.hpp"

#include "Player/PawnKick.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr std::byte k_statusTag{0x48};
        constexpr std::byte k_joinedBit{0x01};
        constexpr std::byte k_deadBit{0x02};
        constexpr std::byte k_armedBit{0x04};
        constexpr std::byte k_aimingBit{0x08};
        constexpr std::byte k_loweredBit{0x10};
        constexpr std::byte k_crouchedBit{0x20};
        constexpr std::byte k_slidingBit{0x40};
        constexpr std::byte k_downedBit{0x01};
        constexpr std::byte k_reloadingBit{0x02};
        constexpr std::byte k_flashlightBit{0x04};
        constexpr std::byte k_vaultingBit{0x08};
        constexpr std::byte k_kickingBit{0x10};
        constexpr float k_pitchStep{0.06f};
        constexpr std::byte k_combatBit{0x01};
        constexpr std::byte k_restartingBit{0x02};
        constexpr std::size_t k_gameFlagsOffset{k_playerStatusBytes - 1};
        constexpr int32_t k_maxEncodedValue{255};
        constexpr float k_reviveSteps{20.0f};
        constexpr std::size_t k_tagBytes{1};
        constexpr std::size_t k_bytesPerPlayer{13};

        enum class Field : std::size_t
        {
            Health = 0,
            Flags,
            MoreFlags,
            Bleed,
            Revive,
            Magazine,
            Reserve,
            DamageCount,
            DamageYaw,
            HitCount,
            KillCount,
            ShotCount,
            LookPitch
        };

        std::size_t at(std::size_t player, Field field)
        {
            return k_tagBytes + player * k_bytesPerPlayer + static_cast<std::size_t>(field);
        }

        std::byte clampedByte(int32_t value)
        {
            return static_cast<std::byte>(std::clamp(value, 0, k_maxEncodedValue));
        }

        std::byte bit(bool isSet, std::byte flag)
        {
            return isSet ? flag : std::byte{0};
        }

        bool has(std::byte flags, std::byte flag)
        {
            return (flags & flag) != std::byte{0};
        }

        int8_t pitchSteps(float pitch)
        {
            return static_cast<int8_t>(std::clamp(std::lround(pitch / k_pitchStep), -127L, 127L));
        }
    } //namespace

    PlayerStatus playerStatus(const Pawn& pawn, const DownedSettings& settings)
    {
        const bool isAlive = !pawn.entity.isNull();
        const float revive = settings.reviveSeconds > 0.0f ? pawn.reviveSeconds / settings.reviveSeconds : 0.0f;

        return PlayerStatus{.health = pawn.health,
            .isJoined = pawn.isJoined,
            .isDead = pawn.isJoined && !isAlive,
            .isArmed = pawn.isArmed,
            .isAiming = pawn.isArmed && pawn.hasCommand && isAimingReady(pawn.command, pawn.motion),
            .isLowered = pawn.isArmed && pawn.isLowered,
            .isCrouched = isAlive && pawn.motion.stance != PawnStance::Stand,
            .isSliding = isAlive && isSliding(pawn.motion),
            .isVaulting = isAlive && isVaulting(pawn.motion),
            .isKicking = isAlive && isKicking(pawn.motion),
            .isDowned = isAlive && pawn.isDowned,
            .isReloading = isReloading(pawn.ammo),
            .bleedSeconds = static_cast<uint8_t>(std::clamp(static_cast<int32_t>(std::ceil(pawn.bleedSeconds)), 0,
                k_maxEncodedValue)),
            .reviveProgress = std::floor(std::clamp(revive, 0.0f, 1.0f) * k_reviveSteps) / k_reviveSteps,
            .magazine = pawn.ammo.magazine,
            .reserve = std::min(pawn.ammo.reserve, k_maxEncodedValue),
            .damageCount = pawn.damageCount,
            .damageYaw = pawn.damageYaw,
            .hitCount = pawn.hitCount,
            .killCount = pawn.killCount,
            .shotCount = static_cast<uint8_t>(pawn.shots),
            .isFlashlightOn = isAlive && pawn.isArmed && pawn.hasCommand && pawn.command.isFlashlightOn,
            .lookPitch = isAlive && pawn.isArmed && pawn.hasCommand && pawn.command.isFlashlightOn
                ? static_cast<float>(pitchSteps(pawn.command.pitch)) * k_pitchStep
                : 0.0f};
    }

    std::array<std::byte, k_playerStatusBytes> encodeStatusMessage(const StatusMessage& message)
    {
        const PlayerStatuses& statuses = message.players;
        std::array<std::byte, k_playerStatusBytes> bytes{};
        bytes[0] = k_statusTag;
        bytes[k_gameFlagsOffset] = bit(message.isCombatActive, k_combatBit) | bit(message.isRestarting, k_restartingBit);

        for (std::size_t index = 0; index < statuses.size(); ++index)
        {
            const PlayerStatus& status = statuses[index];
            bytes[at(index, Field::Health)] = clampedByte(status.health);
            bytes[at(index, Field::Flags)] = bit(status.isJoined, k_joinedBit) | bit(status.isDead, k_deadBit)
                | bit(status.isArmed, k_armedBit) | bit(status.isAiming, k_aimingBit)
                | bit(status.isLowered, k_loweredBit) | bit(status.isCrouched, k_crouchedBit)
                | bit(status.isSliding, k_slidingBit);
            bytes[at(index, Field::MoreFlags)] = bit(status.isDowned, k_downedBit)
                | bit(status.isReloading, k_reloadingBit) | bit(status.isFlashlightOn, k_flashlightBit)
                | bit(status.isVaulting, k_vaultingBit) | bit(status.isKicking, k_kickingBit);
            bytes[at(index, Field::LookPitch)] = std::bit_cast<std::byte>(pitchSteps(status.lookPitch));
            bytes[at(index, Field::Bleed)] = static_cast<std::byte>(status.bleedSeconds);
            bytes[at(index, Field::Revive)] = clampedByte(static_cast<int32_t>(std::lround(status.reviveProgress
                * k_reviveSteps)));
            bytes[at(index, Field::Magazine)] = clampedByte(status.magazine);
            bytes[at(index, Field::Reserve)] = clampedByte(status.reserve);
            bytes[at(index, Field::DamageCount)] = static_cast<std::byte>(status.damageCount);
            bytes[at(index, Field::DamageYaw)] = static_cast<std::byte>(status.damageYaw);
            bytes[at(index, Field::HitCount)] = static_cast<std::byte>(status.hitCount);
            bytes[at(index, Field::KillCount)] = static_cast<std::byte>(status.killCount);
            bytes[at(index, Field::ShotCount)] = static_cast<std::byte>(status.shotCount);
        }

        return bytes;
    }

    std::optional<StatusMessage> decodeStatusMessage(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_playerStatusBytes || bytes[0] != k_statusTag)
        {
            return std::nullopt;
        }

        const std::byte gameFlags = bytes[k_gameFlagsOffset];
        StatusMessage message{.isCombatActive = has(gameFlags, k_combatBit),
            .isRestarting = has(gameFlags, k_restartingBit)};
        PlayerStatuses& statuses = message.players;

        for (std::size_t index = 0; index < statuses.size(); ++index)
        {
            const auto byteAt = [&bytes, index](Field field) { return bytes[at(index, field)]; };
            const auto valueAt = [&byteAt](Field field) { return static_cast<uint8_t>(byteAt(field)); };
            const std::byte flags = byteAt(Field::Flags);
            const std::byte moreFlags = byteAt(Field::MoreFlags);
            statuses[index] = PlayerStatus{.health = valueAt(Field::Health),
                .isJoined = has(flags, k_joinedBit),
                .isDead = has(flags, k_deadBit),
                .isArmed = has(flags, k_armedBit),
                .isAiming = has(flags, k_aimingBit),
                .isLowered = has(flags, k_loweredBit),
                .isCrouched = has(flags, k_crouchedBit),
                .isSliding = has(flags, k_slidingBit),
                .isVaulting = has(moreFlags, k_vaultingBit),
                .isKicking = has(moreFlags, k_kickingBit),
                .isDowned = has(moreFlags, k_downedBit),
                .isReloading = has(moreFlags, k_reloadingBit),
                .bleedSeconds = valueAt(Field::Bleed),
                .reviveProgress = std::min(static_cast<float>(valueAt(Field::Revive)) / k_reviveSteps, 1.0f),
                .magazine = valueAt(Field::Magazine),
                .reserve = valueAt(Field::Reserve),
                .damageCount = valueAt(Field::DamageCount),
                .damageYaw = valueAt(Field::DamageYaw),
                .hitCount = valueAt(Field::HitCount),
                .killCount = valueAt(Field::KillCount),
                .shotCount = static_cast<uint8_t>(valueAt(Field::ShotCount)),
                .isFlashlightOn = has(moreFlags, k_flashlightBit),
                .lookPitch = static_cast<float>(std::bit_cast<int8_t>(byteAt(Field::LookPitch))) * k_pitchStep};
        }

        return message;
    }
} //namespace game
