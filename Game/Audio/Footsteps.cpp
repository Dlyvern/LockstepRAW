#include "Audio/Footsteps.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace game
{
    namespace
    {
        namespace audio = lockstep::audio;

        constexpr std::array<std::string_view, 2> k_extensions{".wav", ".ogg"};
        constexpr std::string_view k_folder{"sounds/footsteps/"};
        constexpr float k_minDistance{1.5f};
        constexpr float k_maxDistance{30.0f};
        constexpr float k_minSpeed{0.1f};
        constexpr float k_landedSpeed{0.5f};
        constexpr float k_minFallDistance{0.5f};
        constexpr float k_maxFrameStep{1.5f};
        constexpr float k_bulletTimeThreshold{0.99f};

        std::vector<audio::SoundHandle> loadVariations(audio::AudioSystem& audioSystem,
            const lockstep::assets::FileSystem& files, const std::string& stem)
        {
            std::vector<audio::SoundHandle> sounds;

            for (uint32_t number = 1; number <= k_maxFootstepVariations; ++number)
            {
                for (std::string_view extension : k_extensions)
                {
                    const std::string path = stem + std::to_string(number) + std::string(extension);

                    if (!files.exists(path))
                    {
                        continue;
                    }

                    auto sound = audioSystem.loadSound(files, path, audio::SoundDescription{.is3d = true,
                        .minDistance = k_minDistance, .maxDistance = k_maxDistance});

                    if (!sound)
                    {
                        std::clog << "Game: footstep '" << path << "' not loaded: " << sound.error().message << '\n';
                        continue;
                    }

                    sounds.push_back(sound.value());
                    break;
                }
            }

            return sounds;
        }

        uint32_t nextRandom(Footsteps& footsteps)
        {
            uint32_t value = footsteps.random;
            value ^= value << 13;
            value ^= value >> 17;
            value ^= value << 5;
            footsteps.random = value;

            return value;
        }

        float signedRandom(Footsteps& footsteps)
        {
            return static_cast<float>(nextRandom(footsteps) % 2001u) / 1000.0f - 1.0f;
        }

        const std::vector<audio::SoundHandle>* pickSet(const Footsteps& footsteps, Surface surface, bool isLanding)
        {
            const auto index = static_cast<std::size_t>(surface);
            const auto concrete = static_cast<std::size_t>(Surface::Concrete);
            const FootstepBank& bank = footsteps.bank;

            for (const std::vector<audio::SoundHandle>* set : {isLanding ? &bank.landings[index] : &bank.steps[index],
                     &bank.steps[index], isLanding ? &bank.landings[concrete] : &bank.steps[concrete],
                     &bank.steps[concrete]})
            {
                if (!set->empty())
                {
                    return set;
                }
            }

            return nullptr;
        }

        void playStep(Footsteps& footsteps, audio::AudioSystem& audioSystem, const FootstepFrame& frame,
            Surface surface, const math::Vec3& position, float volume, bool isLanding)
        {
            const std::vector<audio::SoundHandle>* set = pickSet(footsteps, surface, isLanding);

            if (set == nullptr || volume <= 0.0f)
            {
                return;
            }

            const auto surfaceIndex = static_cast<std::size_t>(surface);
            auto pick = static_cast<uint32_t>(nextRandom(footsteps) % set->size());

            if (set->size() > 1 && pick == footsteps.lastPicks[surfaceIndex])
            {
                pick = (pick + 1) % static_cast<uint32_t>(set->size());
            }

            footsteps.lastPicks[surfaceIndex] = pick;
            const FootstepTuning& tuning = footsteps.tuning;
            const bool isBulletTime = frame.worldScale < k_bulletTimeThreshold;
            const float pitch = (1.0f + tuning.pitchJitter * signedRandom(footsteps))
                * (isBulletTime ? tuning.bulletTimePitch : 1.0f) * (isLanding ? 0.9f : 1.0f);
            const float gain = volume * (1.0f + tuning.volumeJitter * signedRandom(footsteps))
                * (isBulletTime ? tuning.bulletTimeVolume : 1.0f);

            auto voice = audioSystem.play((*set)[pick], audio::PlayDescription{.bus = audio::Bus::Player,
                .volume = gain, .pitch = pitch, .position = position});

            if (voice)
            {
                ++footsteps.played;
            }
        }

        float stepVolume(const FootstepTuning& tuning, const FootstepFrame& frame, float speed, bool isCrouched)
        {
            if (isCrouched)
            {
                return tuning.crouchVolume;
            }

            const float runBlend = math::clamp((speed - frame.walkSpeed)
                    / std::max(frame.runSpeed - frame.walkSpeed, k_minSpeed), 0.0f, 1.0f);

            return tuning.walkVolume + (tuning.runVolume - tuning.walkVolume) * runBlend;
        }

        float landingVolume(const FootstepTuning& tuning, float fallSpeed)
        {
            const float range = std::max(tuning.landFullSpeed - tuning.landMinSpeed, k_minSpeed);

            return tuning.landVolume * math::clamp((fallSpeed - tuning.landMinSpeed) / range, 0.15f, 1.0f);
        }

        float strideOf(const FootstepFrame& frame, float speed, bool isCrouched)
        {
            if (isCrouched)
            {
                return frame.crouchStride;
            }

            const float runBlend = math::clamp((speed - frame.walkStrideSpeed)
                    / std::max(frame.runStrideSpeed - frame.walkStrideSpeed, k_minSpeed), 0.0f, 1.0f);

            return frame.walkStride + (frame.runStride - frame.walkStride) * runBlend;
        }

        Surface surfaceAt(const FootstepFrame& frame, const FootstepPawn& pawn)
        {
            return frame.physics != nullptr ? surfaceBelow(frame.level, *frame.physics, pawn.foot, pawn.controller)
                                            : Surface::Concrete;
        }

        void updateLocal(Footsteps& footsteps, audio::AudioSystem& audioSystem, const FootstepFrame& frame,
            FootstepTracker& tracker, const FootstepPawn& pawn)
        {
            const bool hasStep = tracker.hasLast && pawn.footfalls != tracker.footfalls;
            const bool hasLanding = tracker.hasLast && pawn.landings != tracker.landings;
            tracker.footfalls = pawn.footfalls;
            tracker.landings = pawn.landings;
            tracker.hasLast = true;

            if (!hasStep && !hasLanding)
            {
                return;
            }

            tracker.surface = surfaceAt(frame, pawn);

            if (hasLanding && pawn.landSpeed >= footsteps.tuning.landMinSpeed)
            {
                playStep(footsteps, audioSystem, frame, tracker.surface, pawn.foot,
                    landingVolume(footsteps.tuning, pawn.landSpeed), true);
                return;
            }

            playStep(footsteps, audioSystem, frame, tracker.surface, pawn.foot,
                stepVolume(footsteps.tuning, frame, pawn.speed, pawn.isCrouched), false);
        }

        void updateRemote(Footsteps& footsteps, audio::AudioSystem& audioSystem, const FootstepFrame& frame,
            FootstepTracker& tracker, const FootstepPawn& pawn)
        {
            const math::Vec3 moved = pawn.foot - tracker.lastFoot;
            const bool isTeleport = math::length(moved) > k_maxFrameStep;

            if (!tracker.hasLast || isTeleport || frame.seconds <= 0.0f)
            {
                tracker = FootstepTracker{.lastFoot = pawn.foot, .hasLast = true};
                return;
            }

            tracker.lastFoot = pawn.foot;
            const float verticalSpeed = moved.y / frame.seconds;
            const FootstepTuning& tuning = footsteps.tuning;

            if (verticalSpeed < -tuning.remoteFallSpeed)
            {
                tracker.fallDistance -= moved.y;
                tracker.fallSpeed = std::max(tracker.fallSpeed, -verticalSpeed);
                return;
            }

            if (verticalSpeed > -k_landedSpeed && tracker.fallDistance > 0.0f)
            {
                if (tracker.fallDistance >= k_minFallDistance && tracker.fallSpeed >= tuning.landMinSpeed)
                {
                    tracker.surface = surfaceAt(frame, pawn);
                    playStep(footsteps, audioSystem, frame, tracker.surface, pawn.foot,
                        landingVolume(tuning, tracker.fallSpeed), true);
                    tracker.distance = 0.0f;
                }

                tracker.fallDistance = 0.0f;
                tracker.fallSpeed = 0.0f;
            }

            const float planar = math::length(math::Vec3{moved.x, 0.0f, moved.z});
            const float speed = planar / frame.seconds;

            if (speed < k_minSpeed)
            {
                return;
            }

            tracker.distance += planar;
            const float stepLength = 0.5f * strideOf(frame, speed, pawn.isCrouched);

            if (tracker.distance < stepLength)
            {
                return;
            }

            tracker.distance = std::fmod(tracker.distance, std::max(stepLength, k_minSpeed));
            tracker.surface = surfaceAt(frame, pawn);
            playStep(footsteps, audioSystem, frame, tracker.surface, pawn.foot,
                stepVolume(tuning, frame, speed, pawn.isCrouched), false);
        }
    } //namespace

    void loadFootsteps(Footsteps& footsteps, audio::AudioSystem& audioSystem, const lockstep::assets::FileSystem& files)
    {
        std::size_t stepCount = 0;
        std::size_t landingCount = 0;
        std::string missing;

        for (std::size_t index = 0; index < k_surfaceCount; ++index)
        {
            const std::string name = surfaceName(static_cast<Surface>(index));
            footsteps.bank.steps[index] = loadVariations(audioSystem, files, std::string(k_folder) + name + "_");
            footsteps.bank.landings[index] = loadVariations(audioSystem, files,
                std::string(k_folder) + name + "_land_");
            stepCount += footsteps.bank.steps[index].size();
            landingCount += footsteps.bank.landings[index].size();

            if (footsteps.bank.steps[index].empty())
            {
                missing += (missing.empty() ? "" : ", ") + name;
            }
        }

        if (stepCount == 0)
        {
            std::clog << "Game: no footstep sounds found; expected " << k_folder
                      << "<surface>_<n>.wav (or .ogg), n = 1.." << k_maxFootstepVariations
                      << ", surfaces concrete, tile, wood, metal, grass, asphalt; optional <surface>_land_<n>.wav\n";
            return;
        }

        std::clog << "Game: " << stepCount << " footstep and " << landingCount << " landing sounds loaded";

        if (!missing.empty())
        {
            std::clog << " (none for " << missing << ": concrete is used)";
        }

        std::clog << '\n';
    }

    void updateFootsteps(Footsteps& footsteps, audio::AudioSystem& audioSystem, const FootstepFrame& frame,
        std::span<const FootstepPawn> pawns)
    {
        std::array<bool, k_playerCount> isDrawn{};

        for (const FootstepPawn& pawn : pawns)
        {
            if (pawn.index >= footsteps.pawns.size())
            {
                continue;
            }

            isDrawn[pawn.index] = true;
            FootstepTracker& tracker = footsteps.pawns[pawn.index];

            if (pawn.isLocal)
            {
                updateLocal(footsteps, audioSystem, frame, tracker, pawn);
            }
            else
            {
                updateRemote(footsteps, audioSystem, frame, tracker, pawn);
            }
        }

        for (std::size_t index = 0; index < isDrawn.size(); ++index)
        {
            if (!isDrawn[index])
            {
                footsteps.pawns[index].hasLast = false;
            }
        }
    }

#if LOCKSTEP_DEV_TOOLS
    void footstepsDevSection(Footsteps& footsteps)
    {
        if (!ImGui::TreeNode("Footsteps"))
        {
            return;
        }

        FootstepTuning& tuning = footsteps.tuning;
        ImGui::Text("Played %u  surfaces: Player1 %s, Player2 %s", footsteps.played,
            surfaceName(footsteps.pawns[0].surface), surfaceName(footsteps.pawns[1].surface));
        ImGui::SliderFloat("Walk volume", &tuning.walkVolume, 0.0f, 1.5f);
        ImGui::SliderFloat("Run volume", &tuning.runVolume, 0.0f, 1.5f);
        ImGui::SliderFloat("Crouch volume", &tuning.crouchVolume, 0.0f, 1.5f);
        ImGui::SliderFloat("Landing volume", &tuning.landVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Landing min fall (m/s)", &tuning.landMinSpeed, 0.0f, 10.0f);
        ImGui::SliderFloat("Landing full fall (m/s)", &tuning.landFullSpeed, 1.0f, 20.0f);
        ImGui::SliderFloat("Pitch jitter", &tuning.pitchJitter, 0.0f, 0.3f);
        ImGui::SliderFloat("Volume jitter", &tuning.volumeJitter, 0.0f, 0.5f);
        ImGui::SliderFloat("Bullet-time pitch", &tuning.bulletTimePitch, 0.5f, 1.0f);
        ImGui::SliderFloat("Bullet-time volume", &tuning.bulletTimeVolume, 0.0f, 1.0f);
        ImGui::TreePop();
    }
#endif
} //namespace game
