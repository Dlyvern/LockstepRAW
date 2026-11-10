#include "Audio/CombatAudio.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <string_view>

namespace game
{
    namespace
    {
        namespace audio = lockstep::audio;
        namespace physics = lockstep::physics;

        constexpr std::string_view k_folder{"sounds/"};
        constexpr std::array<std::string_view, 2> k_extensions{".ogg", ".wav"};
        constexpr uint32_t k_maxVariations{12};
        constexpr std::string_view k_interiorPrefix{"Zone_Interior_"};
        constexpr float k_impactRange{150.0f};
        constexpr uint8_t k_maxShotsPerFrame{3};
        constexpr float k_shotMinDistance{4.0f};
        constexpr float k_shotMaxDistance{220.0f};
        constexpr float k_impactMaxDistance{40.0f};
        constexpr float k_exitVolume{0.6f};
        constexpr float k_sirenDistance{120.0f};
        constexpr float k_thunderDistance{300.0f};
        constexpr float k_minThunderRain{0.3f};

        std::optional<std::string> findFile(const lockstep::assets::FileSystem& files, const std::string& stem)
        {
            for (std::string_view extension : k_extensions)
            {
                std::string path = std::string(k_folder) + stem + std::string(extension);

                if (files.exists(path))
                {
                    return path;
                }
            }

            return std::nullopt;
        }

        std::optional<audio::SoundHandle> loadOne(audio::AudioSystem& audioSystem,
            const lockstep::assets::FileSystem& files, const std::string& stem, const audio::SoundDescription& description)
        {
            const std::optional<std::string> path = findFile(files, stem);

            if (!path)
            {
                std::clog << "Game: sound 'sounds/" << stem << "' missing\n";
                return std::nullopt;
            }

            auto sound = audioSystem.loadSound(files, *path, description);

            if (!sound)
            {
                std::clog << "Game: sound '" << *path << "' not loaded: " << sound.error().message << '\n';
                return std::nullopt;
            }

            return sound.value();
        }

        std::vector<audio::SoundHandle> loadMany(audio::AudioSystem& audioSystem, const lockstep::assets::FileSystem& files,
            const std::string& stem, const audio::SoundDescription& description)
        {
            std::vector<audio::SoundHandle> sounds;

            for (uint32_t number = 1; number <= k_maxVariations; ++number)
            {
                const std::string variation = stem + std::to_string(number);

                if (!findFile(files, variation))
                {
                    break;
                }

                if (std::optional<audio::SoundHandle> sound = loadOne(audioSystem, files, variation, description))
                {
                    sounds.push_back(*sound);
                }
            }

            if (sounds.empty())
            {
                std::clog << "Game: no sounds 'sounds/" << stem << "<n>'\n";
            }

            return sounds;
        }

        uint32_t nextRandom(CombatAudio& combat)
        {
            combat.random ^= combat.random << 13;
            combat.random ^= combat.random >> 17;
            combat.random ^= combat.random << 5;
            return combat.random;
        }

        float randomRange(CombatAudio& combat, float low, float high)
        {
            return low + (high - low) * static_cast<float>(nextRandom(combat) % 10000) / 10000.0f;
        }

        void playAt(CombatAudio& combat, audio::AudioSystem& audioSystem, const std::vector<audio::SoundHandle>& sounds,
            const math::Vec3& position, float volume, audio::Bus bus)
        {
            if (sounds.empty())
            {
                return;
            }

            const audio::SoundHandle sound = sounds[nextRandom(combat) % sounds.size()];
            static_cast<void>(audioSystem.play(sound, audio::PlayDescription{.bus = bus,
                .volume = volume * randomRange(combat, 0.9f, 1.0f),
                .pitch = randomRange(combat, 0.95f, 1.05f),
                .position = position}));
        }

        void playGunshot(CombatAudio& combat, audio::AudioSystem& audioSystem, const std::vector<audio::SoundHandle>& near,
            const std::vector<audio::SoundHandle>& far, const math::Vec3& position, const math::Vec3& listener,
            float volume)
        {
            const bool isFar = math::length(position - listener) > combat.tuning.farDistance && !far.empty();
            playAt(combat, audioSystem, isFar ? far : near, position, volume, audio::Bus::World);
        }

        uint8_t newShots(uint8_t now, uint8_t last)
        {
            return std::min(static_cast<uint8_t>(now - last), k_maxShotsPerFrame);
        }

        std::vector<InteriorBox> interiorBoxes(const lockstep::assets::Scene& scene)
        {
            std::vector<InteriorBox> boxes;

            for (const lockstep::assets::SceneNode& node : scene.nodes)
            {
                if (!std::string_view(node.name).starts_with(k_interiorPrefix))
                {
                    continue;
                }

                InteriorBox box{.min = {1e9f, 1e9f, 1e9f}, .max = {-1e9f, -1e9f, -1e9f}};

                for (int corner = 0; corner < 8; ++corner)
                {
                    const math::Vec3 local{(corner & 1) ? 1.0f : -1.0f, (corner & 2) ? 1.0f : -1.0f,
                        (corner & 4) ? 1.0f : -1.0f};
                    const math::Vec3 scaled{local.x * node.transform.scale.x, local.y * node.transform.scale.y,
                        local.z * node.transform.scale.z};
                    const math::Vec3 world = node.transform.position + node.transform.rotation * scaled;
                    box.min = {std::min(box.min.x, world.x), std::min(box.min.y, world.y), std::min(box.min.z, world.z)};
                    box.max = {std::max(box.max.x, world.x), std::max(box.max.y, world.y), std::max(box.max.z, world.z)};
                }

                boxes.push_back(box);
            }

            return boxes;
        }

        bool isInside(const std::vector<InteriorBox>& boxes, const math::Vec3& point)
        {
            return std::any_of(boxes.begin(), boxes.end(), [&point](const InteriorBox& box) {
                return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y
                    && point.z >= box.min.z && point.z <= box.max.z;
            });
        }

        void keepLoop(audio::AudioSystem& audioSystem, std::optional<audio::VoiceHandle>& voice,
            const std::optional<audio::SoundHandle>& sound, float volume, audio::Bus bus = audio::Bus::Ambience)
        {
            if (!sound)
            {
                return;
            }

            if (!voice || !audioSystem.isPlaying(*voice))
            {
                auto started = audioSystem.play(*sound, audio::PlayDescription{.bus = bus, .volume = volume});
                voice = started ? std::optional<audio::VoiceHandle>(started.value()) : std::nullopt;
                return;
            }

            audioSystem.setVoiceVolume(*voice, volume);
        }

        void playThunder(CombatAudio& combat, audio::AudioSystem& audioSystem, const CombatAudioFrame& frame,
            float indoor)
        {
            const CombatAudioTuning& tuning = combat.tuning;

            if (frame.rain < k_minThunderRain || combat.bank.thunder.empty())
            {
                return;
            }

            combat.thunderSeconds -= frame.worldSeconds;

            if (combat.thunderSeconds > 0.0f)
            {
                return;
            }

            const auto pick = static_cast<std::size_t>(nextRandom(combat) % combat.bank.thunder.size());
            const float angle = randomRange(combat, 0.0f, 6.2832f);
            const math::Vec3 where = frame.listener
                + math::Vec3{std::cos(angle), 0.4f, std::sin(angle)} * k_thunderDistance;
            static_cast<void>(audioSystem.play(combat.bank.thunder[pick], audio::PlayDescription{.bus = audio::Bus::World,
                .volume = tuning.thunderVolume * frame.rain * randomRange(combat, 0.6f, 1.0f) * (1.0f - 0.4f * indoor),
                .pitch = randomRange(combat, 0.85f, 1.1f),
                .position = where}));
            combat.thunderSeconds = randomRange(combat, tuning.thunderMinSeconds, tuning.thunderMaxSeconds);
        }

        void updateAmbience(CombatAudio& combat, audio::AudioSystem& audioSystem, const CombatAudioFrame& frame)
        {
            if (frame.scene != combat.scene)
            {
                combat.scene = frame.scene;
                combat.interiors = frame.scene != nullptr ? interiorBoxes(*frame.scene) : std::vector<InteriorBox>{};
            }

            const CombatAudioTuning& tuning = combat.tuning;
            const float target = isInside(combat.interiors, frame.listener) ? 1.0f : 0.0f;
            const float step = tuning.ambienceFade > 0.0f ? frame.seconds / tuning.ambienceFade : 1.0f;
            combat.indoorWeight += std::clamp(target - combat.indoorWeight, -step, step);
            const float indoor = combat.indoorWeight;

            keepLoop(audioSystem, combat.street, combat.bank.street,
                tuning.streetVolume * (1.0f - indoor * (1.0f - tuning.indoorStreet)));
            keepLoop(audioSystem, combat.hum, combat.bank.hum, tuning.humVolume * indoor);

            if (frame.rain > 0.0f || combat.rain || combat.rainIndoor)
            {
                keepLoop(audioSystem, combat.rain, combat.bank.rain,
                    tuning.rainVolume * frame.rain * (1.0f - indoor * (1.0f - tuning.rainHeardIndoors)),
                    audio::Bus::World);
                keepLoop(audioSystem, combat.rainIndoor, combat.bank.rainIndoor,
                    tuning.rainIndoorVolume * frame.rain * indoor, audio::Bus::World);
            }
            playThunder(combat, audioSystem, frame, indoor);

            combat.sirenSeconds -= frame.seconds;

            if (combat.sirenSeconds <= 0.0f && combat.bank.siren)
            {
                const float angle = randomRange(combat, 0.0f, 6.2832f);
                const math::Vec3 where = frame.listener + math::Vec3{std::cos(angle), 0.0f, std::sin(angle)} * k_sirenDistance;
                static_cast<void>(audioSystem.play(*combat.bank.siren, audio::PlayDescription{.bus = audio::Bus::Ambience,
                    .volume = tuning.sirenVolume * (1.0f - 0.6f * indoor), .position = where}));
                combat.sirenSeconds = randomRange(combat, tuning.sirenMinSeconds, tuning.sirenMaxSeconds);
            }
        }

        const std::vector<audio::SoundHandle>& impactSounds(const CombatSoundBank& bank, const ShotImpact& impact,
            bool isFlesh)
        {
            if (isFlesh)
            {
                return bank.fleshHits;
            }

            return impact.material == ShotMaterial::Metal && !bank.metalHits.empty() ? bank.metalHits : bank.concreteHits;
        }

        ShotPath ownShotPath(const CombatAudioFrame& frame, uint8_t shotNumber)
        {
            const bool hasRules = frame.command != nullptr && frame.spread != nullptr && frame.penetration != nullptr;
            const math::Vec3 direction = hasRules ? shotDirection(*frame.command, frame.localIndex, shotNumber, *frame.spread)
                                                  : frame.aim;
            const PenetrationSettings stopAtFirst{.maxPenetrations = 0};

            return traceShotPath(*frame.physics, frame.level, frame.eye, direction, k_impactRange, frame.localController,
                {}, hasRules ? *frame.penetration : stopAtFirst);
        }

        void playOwnShots(CombatAudio& combat, audio::AudioSystem& audioSystem, const CombatAudioFrame& frame,
            const PlayerStatus& own)
        {
            const uint8_t shots = newShots(frame.localShots, combat.lastLocalShots);
            const bool landedHit = own.hitCount != combat.lastHits;

            for (uint8_t shot = 0; shot < shots; ++shot)
            {
                playAt(combat, audioSystem, combat.bank.v45Shots, frame.eye, combat.tuning.shotVolume, audio::Bus::World);
                combat.shots.push_back(CombatShot{.position = frame.eye, .isLocal = true});

                if (frame.physics == nullptr)
                {
                    continue;
                }

                const auto shotNumber = static_cast<uint8_t>(frame.localShots - shots + 1 + shot);
                const ShotPath path = ownShotPath(frame, shotNumber);

                for (std::size_t index = 0; index < path.impacts.size(); ++index)
                {
                    const ShotImpact& impact = path.impacts[index];

                    if (math::length(impact.point - frame.eye) >= k_impactMaxDistance)
                    {
                        break;
                    }

                    const bool isLast = index + 1 == path.impacts.size();
                    const bool isFlesh = !impact.controller.isNull() || (landedHit && isLast && !impact.isExit);
                    combat.impacts.push_back(CombatImpact{.point = impact.point, .normal = impact.normal,
                        .body = impact.body, .isFlesh = isFlesh, .isExit = impact.isExit});
                    playAt(combat, audioSystem, impactSounds(combat.bank, impact, isFlesh), impact.point,
                        combat.tuning.impactVolume * (impact.isExit ? k_exitVolume : 1.0f), audio::Bus::World);
                }
            }
        }
    } //namespace

    void playFleshHit(CombatAudio& combat, audio::AudioSystem& audioSystem, const math::Vec3& point, float strength,
        bool isHeadshot)
    {
        const std::vector<audio::SoundHandle>& sounds = combat.bank.fleshHits;

        if (sounds.empty())
        {
            return;
        }

        const float loudness = std::clamp(0.55f + 0.45f * strength, 0.4f, 1.4f) * combat.tuning.fleshHitVolume
            * (isHeadshot ? combat.tuning.headshotVolume : 1.0f);
        const float pitch = (isHeadshot ? 0.86f : 1.0f) * randomRange(combat, 0.94f, 1.06f) / std::clamp(0.8f + 0.2f * strength, 0.85f, 1.2f);
        static_cast<void>(audioSystem.play(sounds[nextRandom(combat) % sounds.size()], audio::PlayDescription{
            .bus = audio::Bus::World, .volume = loudness, .pitch = pitch, .position = point}));

        if (isHeadshot)
        {
            static_cast<void>(audioSystem.play(sounds[nextRandom(combat) % sounds.size()], audio::PlayDescription{
                .bus = audio::Bus::World, .volume = loudness * 0.7f, .pitch = pitch * 0.72f, .position = point}));
        }
    }

    CombatSoundBank loadCombatSounds(audio::AudioSystem& audioSystem, const lockstep::assets::FileSystem& files)
    {
        const audio::SoundDescription shot{.is3d = true, .minDistance = k_shotMinDistance, .maxDistance = k_shotMaxDistance};
        const audio::SoundDescription impact{.is3d = true, .minDistance = 1.5f, .maxDistance = k_impactMaxDistance};
        const audio::SoundDescription near{.is3d = true, .minDistance = 1.0f, .maxDistance = 25.0f};
        const audio::SoundDescription loop2d{.is3d = false, .isLooping = true, .isStreamed = true};

        CombatSoundBank bank{};
        bank.v45Shots = loadMany(audioSystem, files, "weapons/v45_shot_", shot);
        bank.v45FarShots = loadMany(audioSystem, files, "weapons/v45_shot_far_", shot);
        bank.npcShots = loadMany(audioSystem, files, "weapons/merc_shot_", shot);
        bank.npcFarShots = loadMany(audioSystem, files, "weapons/merc_shot_far_", shot);
        bank.concreteHits = loadMany(audioSystem, files, "impacts/concrete_", impact);
        bank.metalHits = loadMany(audioSystem, files, "impacts/metal_", impact);
        bank.fleshHits = loadMany(audioSystem, files, "impacts/flesh_", impact);
        bank.glassBreaks = loadMany(audioSystem, files, "impacts/glass_break_",
            audio::SoundDescription{.is3d = true, .minDistance = 2.0f, .maxDistance = 45.0f});
        bank.dryFire = loadOne(audioSystem, files, "weapons/dryfire", near);
        bank.heartbeat = loadOne(audioSystem, files, "player/heartbeat",
            audio::SoundDescription{.is3d = false, .isLooping = true});
        bank.street = loadOne(audioSystem, files, "ambience/street_loop", loop2d);
        bank.hum = loadOne(audioSystem, files, "ambience/hospital_hum", loop2d);
        bank.siren = loadOne(audioSystem, files, "ambience/siren_distant",
            audio::SoundDescription{.is3d = true, .isStreamed = true, .minDistance = 40.0f, .maxDistance = 400.0f});
        bank.rain = loadOne(audioSystem, files, "ambience/rain_outdoor", loop2d);
        bank.rainIndoor = loadOne(audioSystem, files, "ambience/rain_indoor", loop2d);
        bank.thunder = loadMany(audioSystem, files, "ambience/thunder_",
            audio::SoundDescription{.is3d = true, .isStreamed = true, .minDistance = 400.0f, .maxDistance = 2000.0f});
        return bank;
    }

    void updateCombatAudio(CombatAudio& combat, audio::AudioSystem& audioSystem, const CombatAudioFrame& frame)
    {
        const PlayerStatus& own = frame.statuses[frame.localIndex];

        if (!combat.hasLast)
        {
            combat.lastLocalShots = frame.localShots;
            combat.lastDamage = own.damageCount;
            combat.lastHits = own.hitCount;

            for (std::size_t index = 0; index < k_playerCount; ++index)
            {
                combat.lastShots[index] = frame.statuses[index].shotCount;
            }

            for (const NpcShotSource& npc : frame.npcs)
            {
                combat.npcShots[npc.key] = npc.shotCount;
            }

            combat.hasLast = true;
        }

        combat.impacts.clear();
        combat.shots.clear();
        updateAmbience(combat, audioSystem, frame);
        playOwnShots(combat, audioSystem, frame, own);
        combat.lastLocalShots = frame.localShots;
        combat.lastHits = own.hitCount;

        for (std::size_t index = 0; index < k_playerCount; ++index)
        {
            const PlayerStatus& status = frame.statuses[index];
            const std::optional<math::Vec3>& position = frame.positions[index];
            const math::Vec3 where = index == frame.localIndex ? frame.eye : position.value_or(frame.listener);

            if (index != frame.localIndex && position)
            {
                const uint8_t shots = newShots(status.shotCount, combat.lastShots[index]);

                for (uint8_t shot = 0; shot < shots; ++shot)
                {
                    playGunshot(combat, audioSystem, combat.bank.v45Shots, combat.bank.v45FarShots, where, frame.listener,
                        combat.tuning.shotVolume);
                    combat.shots.push_back(CombatShot{.position = where});
                }
            }

            combat.lastShots[index] = status.shotCount;
        }

        if (own.damageCount != combat.lastDamage)
        {
            playAt(combat, audioSystem, combat.bank.fleshHits, frame.eye, combat.tuning.hurtVolume, audio::Bus::Player);
            combat.lastDamage = own.damageCount;
        }

        if (own.isDowned && combat.bank.heartbeat)
        {
            if (!combat.heartbeat || !audioSystem.isPlaying(*combat.heartbeat))
            {
                auto started = audioSystem.play(*combat.bank.heartbeat, audio::PlayDescription{.bus = audio::Bus::Player,
                    .volume = combat.tuning.heartbeatVolume});
                combat.heartbeat = started ? std::optional<audio::VoiceHandle>(started.value()) : std::nullopt;
            }
        }
        else if (combat.heartbeat)
        {
            audioSystem.stop(*combat.heartbeat);
            combat.heartbeat.reset();
        }

        for (const NpcShotSource& npc : frame.npcs)
        {
            const auto [seen, isNew] = combat.npcShots.try_emplace(npc.key, npc.shotCount);
            const uint8_t shots = isNew ? 0 : newShots(npc.shotCount, seen->second);

            for (uint8_t shot = 0; shot < shots; ++shot)
            {
                playGunshot(combat, audioSystem, combat.bank.npcShots, combat.bank.npcFarShots, npc.position, frame.listener,
                    combat.tuning.npcShotVolume);
                combat.shots.push_back(CombatShot{.position = npc.position});
            }

            seen->second = npc.shotCount;
        }
    }

#if LOCKSTEP_DEV_TOOLS
    void combatAudioDevSection(CombatAudio& combat)
    {
        if (!ImGui::TreeNode("Combat audio"))
        {
            return;
        }

        CombatAudioTuning& tuning = combat.tuning;
        ImGui::Text("Indoor %.2f, %zu interior boxes", combat.indoorWeight, combat.interiors.size());
        ImGui::SliderFloat("Gunshots", &tuning.shotVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("NPC gunshots", &tuning.npcShotVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Distant from (m)", &tuning.farDistance, 5.0f, 100.0f);
        ImGui::SliderFloat("Impacts", &tuning.impactVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Hurt", &tuning.hurtVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Flesh hits (others' shots)", &tuning.fleshHitVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Headshot factor", &tuning.headshotVolume, 0.5f, 3.0f);
        ImGui::SliderFloat("Heartbeat", &tuning.heartbeatVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Street", &tuning.streetVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Hospital hum", &tuning.humVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Street heard indoors", &tuning.indoorStreet, 0.0f, 1.0f);
        ImGui::SliderFloat("Siren", &tuning.sirenVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Rain outdoors", &tuning.rainVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Rain indoors (muffled)", &tuning.rainIndoorVolume, 0.0f, 2.0f);
        ImGui::SliderFloat("Outdoor rain heard indoors", &tuning.rainHeardIndoors, 0.0f, 1.0f);
        ImGui::SliderFloat("Thunder", &tuning.thunderVolume, 0.0f, 2.0f);
        ImGui::TreePop();
    }
#endif
} //namespace game
