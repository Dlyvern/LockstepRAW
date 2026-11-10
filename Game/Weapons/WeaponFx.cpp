#include "Weapons/WeaponFx.hpp"

#include "GameState.hpp"
#include "Player/LocalPlayer.hpp"
#include "Presentation/WeaponPresentation.hpp"
#include "Weapons/WeaponSockets.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <iostream>
#include <optional>
#include <vector>

namespace game
{
    namespace
    {
        namespace audio = lockstep::audio;
        namespace physics = lockstep::physics;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr uint32_t k_npcKeyBase{0x10000u};
        constexpr uint8_t k_maxShotsPerFrame{3};
        constexpr float k_tracerRange{150.0f};
        constexpr std::size_t k_maxLandingSounds{4};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_muzzleReach{0.55f};
        constexpr float k_muzzleRight{0.12f};
        constexpr float k_muzzleDrop{0.1f};

        struct Shot
        {
            uint32_t key{0};
            std::optional<WeaponSocket> socket{};
            math::Vec3 position{0.0f, 0.0f, 0.0f};
            uint8_t ordinal{0};
            bool isLocal{false};
        };

        uint32_t nextRandom(WeaponFx& fx)
        {
            fx.random ^= fx.random << 13;
            fx.random ^= fx.random >> 17;
            fx.random ^= fx.random << 5;
            return fx.random;
        }

        float randomRange(WeaponFx& fx, float low, float high)
        {
            return low + (high - low) * static_cast<float>(nextRandom(fx) % 10000) / 10000.0f;
        }

        math::Vec3 randomUnit(WeaponFx& fx)
        {
            const math::Vec3 vector{randomRange(fx, -1.0f, 1.0f), randomRange(fx, -1.0f, 1.0f), randomRange(fx, -1.0f, 1.0f)};
            const float length = math::length(vector);

            return length > 1e-3f ? vector / length : k_up;
        }

        math::Quat basisRotation(const math::Vec3& xAxis, const math::Vec3& yAxis)
        {
            const math::Vec3 z = math::cross(xAxis, yAxis);
            math::Mat4 basis{1.0f};
            basis.setColumn(0, math::Vec4{xAxis.x, xAxis.y, xAxis.z, 0.0f});
            basis.setColumn(1, math::Vec4{yAxis.x, yAxis.y, yAxis.z, 0.0f});
            basis.setColumn(2, math::Vec4{z.x, z.y, z.z, 0.0f});

            return math::fromRotationMatrix(basis);
        }

        std::optional<WeaponSocket> localSocket(const GameState& state, const WeaponFx& fx)
        {
            if (!state.pawnCharacters || !state.pawnCharacters->firstPerson)
            {
                return std::nullopt;
            }

            return viewModelWeaponSocket(*state.pawnCharacters->firstPerson, state.pawnCharacters->viewModel, fx.worldFov);
        }

        std::optional<WeaponSocket> npcSocket(const GameState& state, uint32_t index)
        {
            for (const NpcLook& look : state.npcLooks.looks)
            {
                if (look.entity.index == index)
                {
                    return npcWeaponSocket(state.npcLooks, look.entity);
                }
            }

            return std::nullopt;
        }

        std::optional<WeaponSocket> socketOf(const GameState& state, const WeaponFx& fx, std::size_t localIndex,
            uint32_t key)
        {
            if (key >= k_npcKeyBase)
            {
                return npcSocket(state, key - k_npcKeyBase);
            }

            if (key == localIndex)
            {
                return localSocket(state, fx);
            }

            return state.pawnCharacters ? pawnWeaponSocket(*state.pawnCharacters, key) : std::nullopt;
        }

        uint8_t newShots(uint8_t now, uint8_t last)
        {
            return std::min(static_cast<uint8_t>(now - last), k_maxShotsPerFrame);
        }

        void addShots(std::vector<Shot>& shots, uint8_t now, uint8_t& last, const Shot& shot)
        {
            const uint8_t count = newShots(now, last);

            for (uint8_t number = 0; number < count; ++number)
            {
                Shot fired = shot;
                fired.ordinal = static_cast<uint8_t>(now - count + number + 1);
                shots.push_back(fired);
            }

            last = now;
        }

        std::vector<Shot> collectShots(GameState& state, EngineContext& engine, WeaponFx& fx, const WeaponFxFrame& frame)
        {
            ShotCounters& counters = fx.counters;
            const std::size_t localIndex = localPawnIndex(engine);
            std::vector<Shot> shots;

            if (!counters.hasLast)
            {
                counters.local = state.local.command.shotCount;

                for (std::size_t index = 0; index < k_playerCount; ++index)
                {
                    counters.players[index] = state.local.statuses[index].shotCount;
                }

                counters.hasLast = true;
            }

            if (frame.hasLocalPawn)
            {
                const math::Vec3 forward = math::normalize(frame.aim);
                const math::Vec3 right = math::normalize(math::cross(forward, k_up));
                const std::optional<WeaponSocket> socket = localSocket(state, fx);
                addShots(shots, state.local.command.shotCount, counters.local, Shot{
                    .key = static_cast<uint32_t>(localIndex),
                    .socket = socket,
                    .position = socket ? socket->muzzle
                                       : frame.eye + forward * k_muzzleReach + right * k_muzzleRight - k_up * k_muzzleDrop,
                    .isLocal = true});
            }
            else
            {
                counters.local = state.local.command.shotCount;
            }

            for (std::size_t index = 0; index < k_playerCount; ++index)
            {
                const uint8_t now = state.local.statuses[index].shotCount;
                const std::optional<WeaponSocket> socket = index != localIndex && state.pawnCharacters
                    ? pawnWeaponSocket(*state.pawnCharacters, index)
                    : std::nullopt;

                if (index == localIndex || !socket)
                {
                    counters.players[index] = now;
                    continue;
                }

                addShots(shots, now, counters.players[index], Shot{.key = static_cast<uint32_t>(index),
                    .socket = socket,
                    .position = socket->muzzle});
            }

            for (const NpcLook& look : state.npcLooks.looks)
            {
                if (!look.isDrawn)
                {
                    continue;
                }

                const auto [seen, isNew] = counters.npcs.try_emplace(look.entity.index, look.shotCount);

                if (isNew)
                {
                    continue;
                }

                const std::optional<WeaponSocket> socket = npcWeaponSocket(state.npcLooks, look.entity);
                Shot shot{.key = k_npcKeyBase + look.entity.index, .socket = socket};
                shot.position = socket ? socket->muzzle : drawnPlacement(look).position + k_up * 1.4f;
                addShots(shots, look.shotCount, seen->second, shot);
            }

            return shots;
        }

        void ejectCasing(WeaponFx& fx, const WeaponSocket& socket, physics::ControllerId ignore)
        {
            const WeaponFxTuning& tuning = fx.tuning;
            const math::Vec3 velocity = socket.right * randomRange(fx, tuning.ejectSpeed.x, tuning.ejectSpeed.y)
                + socket.up * randomRange(fx, tuning.ejectLift.x, tuning.ejectLift.y)
                - socket.forward * randomRange(fx, 0.0f, 0.6f) + randomUnit(fx) * 0.25f;
            const math::Vec3 spinAxis = math::normalize(socket.up + randomUnit(fx) * 0.4f);
            spawnDebris(fx.casings, DebrisPiece{.position = socket.port,
                .velocity = velocity,
                .rotation = basisRotation(socket.up, socket.forward),
                .spin = spinAxis * randomRange(fx, 18.0f, 40.0f),
                .ignore = ignore});
        }

        math::Vec3 tracerEnd(const GameState& state, EngineContext& engine, const Shot& shot, const WeaponFxFrame& frame)
        {
            math::Vec3 origin = shot.position;
            math::Vec3 direction = shot.socket ? shot.socket->forward : frame.aim;
            physics::RaycastIgnore ignore{};

            if (shot.isLocal)
            {
                origin = frame.eye;
                direction = localShotDirection(state, engine, shot.ordinal);
                ignore.controller = localController(state, engine);
            }

            direction = math::normalize(direction);
            const std::optional<physics::RaycastHit> hit = engine.physics.raycast(origin, direction, k_tracerRange, ignore);

            return hit ? hit->point : origin + direction * k_tracerRange;
        }

        void showShots(GameState& state, EngineContext& engine, WeaponFx& fx, const WeaponFxFrame& frame,
            const std::vector<Shot>& shots)
        {
            const Effects& effects = state.effects;
            const bool hasParticles = effects.particles.has_value() && effects.tuning.enabled;
            lockstep::render::ParticleSystem* particles = hasParticles ? &*state.effects.particles : nullptr;
            const lockstep::render::ParticleEffectId smoke = effects.tuning.muzzleSmoke ? effects.ids.muzzleSmoke
                                                                                         : lockstep::render::ParticleEffectId{};
            const physics::ControllerId local = frame.hasLocalPawn ? localController(state, engine) : physics::ControllerId{};

            for (const Shot& shot : shots)
            {
                if (fx.tuning.tails)
                {
                    playGunshotTail(fx.sounds, engine.audio, shot.position, state.combatAudio.indoorWeight);
                }

                if (!shot.socket)
                {
                    continue;
                }

                const float aimed = shot.isLocal && state.pawnCharacters ? viewModelZoom(state.pawnCharacters->viewModel)
                                                                          : 0.0f;
                flashMuzzle(fx.muzzle, particles, *shot.socket, shot.key, smoke, shot.isLocal,
                    1.0f + (fx.viewFlash.tuning.aimScale - 1.0f) * aimed);

                if (shot.isLocal)
                {
                    fireViewModelFlash(fx.viewFlash);
                }

                const int every = std::max(fx.muzzle.tuning.tracerEvery, 1);

                if (particles != nullptr && shot.ordinal % every == 0)
                {
                    fireTracer(fx.muzzle, *particles, shot.socket->muzzle, tracerEnd(state, engine, shot, frame));
                }

                if (fx.tuning.casings)
                {
                    ejectCasing(fx, *shot.socket, shot.isLocal ? local : physics::ControllerId{});
                }
            }
        }

        void dropMagazine(WeaponFx& fx, const WeaponSocket& socket, physics::ControllerId ignore)
        {
            spawnDebris(fx.magazines, DebrisPiece{.position = socket.magazine.position,
                .velocity = -(socket.magazine.rotation * k_up) * randomRange(fx, 0.6f, 1.1f) + socket.right * 0.2f
                    + randomUnit(fx) * 0.15f,
                .rotation = socket.magazine.rotation,
                .spin = randomUnit(fx) * randomRange(fx, 1.0f, 4.0f),
                .ignore = ignore});
        }

        void showReloads(GameState& state, EngineContext& engine, WeaponFx& fx, const WeaponFxFrame& frame)
        {
            updateReloadCues(fx.reloads, state.local.statuses, state.settings.ammo, frame.playerSeconds);
            const std::size_t localIndex = localPawnIndex(engine);
            const WeaponSoundBank& bank = fx.sounds.bank;

            for (const ReloadEvent& event : fx.reloads.events)
            {
                const bool isLocal = event.player == localIndex;

                if (isLocal && !frame.hasLocalPawn)
                {
                    continue;
                }

                const std::optional<WeaponSocket> socket = socketOf(state, fx, localIndex,
                    static_cast<uint32_t>(event.player));

                if (!isLocal && !socket)
                {
                    continue;
                }

                const math::Vec3 where = isLocal ? frame.eye : socket->port;
                const std::optional<audio::SoundHandle>& sound = event.cue == ReloadCue::MagazineOut ? bank.magazineOut
                    : event.cue == ReloadCue::MagazineIn ? bank.magazineIn
                    : bank.charge;

                if (sound)
                {
                    static_cast<void>(engine.audio.play(*sound, audio::PlayDescription{.bus = audio::Bus::Player,
                        .volume = fx.sounds.tuning.reload,
                        .pitch = randomRange(fx, 0.97f, 1.03f),
                        .position = where}));
                }

                if (event.cue == ReloadCue::MagazineOut && fx.tuning.magazines && socket)
                {
                    dropMagazine(fx, *socket, isLocal ? localController(state, engine) : physics::ControllerId{});
                }
            }
        }

        void playLandings(WeaponFx& fx, EngineContext& engine, const DebrisPool& pool,
            const std::array<std::vector<audio::SoundHandle>, k_surfaceCount>& sounds, float volume)
        {
            const std::size_t count = std::min(pool.landings.size(), k_maxLandingSounds);

            for (std::size_t index = 0; index < count; ++index)
            {
                const DebrisLanding& landing = pool.landings[index];
                const std::size_t surface = static_cast<std::size_t>(landing.surface);
                const std::vector<audio::SoundHandle>& list = !sounds[surface].empty()
                    ? sounds[surface]
                    : sounds[static_cast<std::size_t>(Surface::Concrete)];
                playWeaponSound(fx.sounds, engine.audio, list, landing.position, volume * landing.loudness);
            }
        }

        void prepare(GameState& state, EngineContext& engine, WeaponFx& fx)
        {
            if (!fx.isLookTried)
            {
                fx.isLookTried = true;

                if (auto casing = createCasingLook(engine.renderer, fx.casings); !casing)
                {
                    std::clog << "Game: casing mesh not created: " << casing.error().message << '\n';
                }

                if (auto magazine = createMagazineLook(engine.renderer, fx.magazines); !magazine)
                {
                    std::clog << "Game: magazine mesh not created: " << magazine.error().message << '\n';
                }
            }

            if (state.effects.particles)
            {
                static_cast<void>(createMuzzleEffects(fx.muzzle, *state.effects.particles, state.effects.atlas));
            }

            createViewModelFlash(fx.viewFlash, engine.renderer);

            const lockstep::assets::Scene* scene = state.level ? &state.level->scene : nullptr;

            if (scene != fx.scene)
            {
                fx.scene = scene;
                clearDebris(fx.casings);
                clearDebris(fx.magazines);
                fx.muzzle.lights.clear();
                fx.muzzle.barrels.clear();
            }
        }
    } //namespace

    void loadWeaponFx(WeaponFx& fx, EngineContext& engine)
    {
        fx.sounds.bank = loadWeaponSounds(engine.audio, engine.files);
    }

    void presentWeaponFx(GameState& state, EngineContext& engine, const WeaponFxFrame& frame)
    {
        WeaponFx& fx = state.weaponFx;
        fx.worldFov = frame.worldFov;
        prepare(state, engine, fx);
        beginWeaponSounds(fx.sounds);

        showShots(state, engine, fx, frame, collectShots(state, engine, fx, frame));
        showReloads(state, engine, fx, frame);

        if (state.pawnCharacters && state.pawnCharacters->firstPerson && frame.hasLocalPawn)
        {
            const PawnCharacters& characters = *state.pawnCharacters;
            drawViewModelFlash(fx.viewFlash, engine.renderer,
                viewModelLayerSocket(*characters.firstPerson, characters.viewModel), viewModelZoom(characters.viewModel));
        }

        const SceneLevel* level = state.level ? &*state.level : nullptr;
        updateDebris(fx.casings, engine.physics, level, frame.worldSeconds);
        updateDebris(fx.magazines, engine.physics, level, frame.worldSeconds);
        playLandings(fx, engine, fx.casings, fx.sounds.bank.casings, fx.sounds.tuning.casing);
        playLandings(fx, engine, fx.magazines, fx.sounds.bank.magazines, fx.sounds.tuning.magazine);
        drawDebris(fx.casings, engine.renderer);
        drawDebris(fx.magazines, engine.renderer);

        if (state.effects.particles && state.effects.tuning.enabled && state.effects.tuning.muzzleSmoke)
        {
            const std::size_t localIndex = localPawnIndex(engine);

            for (const auto& [key, barrel] : fx.muzzle.barrels)
            {
                if (const std::optional<WeaponSocket> socket = socketOf(state, fx, localIndex, key))
                {
                    smokeBarrel(fx.muzzle, *state.effects.particles, key, *socket, key == localIndex,
                        frame.worldSeconds);
                }
            }
        }

        updateMuzzleFlashes(fx.muzzle, engine.renderer, frame.worldSeconds);
    }

#if LOCKSTEP_DEV_TOOLS
    void weaponFxDevSection(WeaponFx& fx)
    {
        if (!ImGui::TreeNode("Weapon details"))
        {
            return;
        }

        MuzzleFlashTuning& muzzle = fx.muzzle.tuning;
        ImGui::Text("%zu casings, %zu magazines, %zu lights, %zu hot barrels", fx.casings.pieces.size(),
            fx.magazines.pieces.size(), fx.muzzle.lights.size(), fx.muzzle.barrels.size());
        ImGui::Checkbox("Flash particles", &muzzle.flashes);
        ImGui::Checkbox("Flash light", &muzzle.lights);
        ImGui::Checkbox("Tracers", &muzzle.tracers);
        ImGui::Checkbox("Casings", &fx.tuning.casings);
        ImGui::Checkbox("Magazine drops", &fx.tuning.magazines);
        ImGui::Checkbox("Gunshot tails", &fx.tuning.tails);
        ImGui::SliderInt("Tracer every", &muzzle.tracerEvery, 1, 10);
        ViewModelFlashTuning& view = fx.viewFlash.tuning;
        ImGui::Checkbox("Own flash (viewmodel)", &view.enabled);
        ImGui::SliderFloat("Own flash size (m)", &view.size, 0.0f, 0.3f);
        ImGui::SliderFloat("Own flash length (m)", &view.length, 0.0f, 0.4f);
        ImGui::SliderFloat("Own flash intensity", &view.intensity, 0.0f, 20.0f);
        ImGui::SliderFloat("Own flash 2nd frame", &view.secondFrame, 0.0f, 1.0f);
        ImGui::SliderInt("Own flash frames", &view.frames, 1, 4);
        ImGui::SliderFloat("Flash scale aiming", &view.aimScale, 0.0f, 1.0f);
        ImGui::SliderFloat("World flash size", &muzzle.flashSize, 0.0f, 2.0f);
        ImGui::SliderFloat("World flash intensity", &muzzle.flashIntensity, 0.0f, 2.0f);
        ImGui::SliderFloat("Light intensity", &muzzle.lightIntensity, 0.0f, 150.0f);
        ImGui::SliderFloat("Light range (m)", &muzzle.lightRange, 1.0f, 15.0f);
        ImGui::SliderFloat("Light in fog", &muzzle.lightVolumetric, 0.0f, 8.0f);
        ImGui::SliderFloat("Own light scale", &muzzle.ownLightScale, 0.0f, 2.0f);
        ImGui::SliderFloat("Own light in fog", &muzzle.ownLightVolumetric, 0.0f, 4.0f);
        ImGui::SliderFloat("Own light ahead (m)", &muzzle.ownLightAhead, 0.0f, 2.0f);
        ImGui::SliderFloat("Own shot smoke", &muzzle.ownSmoke, 0.0f, 1.0f);
        ImGui::SliderInt("Light frames", &muzzle.lightFrames, 1, 6);
        ImGui::SliderFloat("Heat decay (1/s)", &muzzle.heatDecay, 0.05f, 2.0f);
        ImGui::SliderFloat("Smoke from heat", &muzzle.smokeHeat, 0.0f, 10.0f);
        ImGui::SliderFloat("Smoke rate", &muzzle.smokeRate, 0.0f, 10.0f);
        ImGui::SliderFloat("Casing volume", &fx.sounds.tuning.casing, 0.0f, 2.0f);
        ImGui::SliderFloat("Magazine volume", &fx.sounds.tuning.magazine, 0.0f, 2.0f);
        ImGui::SliderFloat("Reload volume", &fx.sounds.tuning.reload, 0.0f, 2.0f);
        ImGui::SliderFloat("Indoor tail", &fx.sounds.tuning.indoorTail, 0.0f, 2.0f);
        ImGui::SliderFloat("Outdoor tail", &fx.sounds.tuning.outdoorTail, 0.0f, 2.0f);
        ImGui::TreePop();
    }
#endif
} //namespace game
