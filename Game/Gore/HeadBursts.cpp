#include "Gore/HeadBursts.hpp"

#include "Effects/EffectTextures.hpp"
#include "GameState.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/PbrMaterial.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace audio = lockstep::audio;
        namespace physics = lockstep::physics;
        namespace render = lockstep::render;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_gravity{0.0f, -9.81f, 0.0f};
        constexpr float k_twoPi{2.0f * math::k_pi};
        constexpr std::string_view k_soundStem{"sounds/impacts/headburst_"};
        constexpr uint32_t k_maxSounds{8};
        constexpr math::Vec4 k_fleshColour{0.34f, 0.025f, 0.02f, 1.0f};
        constexpr math::Vec4 k_boneColour{0.78f, 0.72f, 0.62f, 1.0f};
        constexpr float k_fleshDensity{1100.0f};
        constexpr float k_boneDensity{1900.0f};
        constexpr float k_boneShare{0.55f};
        constexpr float k_headAboveNeck{0.12f};
        constexpr float k_coneShare{0.6f};
        constexpr float k_coneAngle{0.6f};
        constexpr float k_minSplatter{0.2f};
        constexpr float k_spurtLift{0.35f};
        constexpr std::array<std::array<float, 3>, 12> k_icosahedron{{
            {-1.0f, 1.618f, 0.0f}, {1.0f, 1.618f, 0.0f}, {-1.0f, -1.618f, 0.0f}, {1.0f, -1.618f, 0.0f},
            {0.0f, -1.0f, 1.618f}, {0.0f, 1.0f, 1.618f}, {0.0f, -1.0f, -1.618f}, {0.0f, 1.0f, -1.618f},
            {1.618f, 0.0f, -1.0f}, {1.618f, 0.0f, 1.0f}, {-1.618f, 0.0f, -1.0f}, {-1.618f, 0.0f, 1.0f}}};
        constexpr std::array<std::array<uint32_t, 3>, 20> k_icosahedronFaces{{
            {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2},
            {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5},
            {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}}};

        math::Vec3 randomDirection(GoreRandom& random)
        {
            const float y = random.range(-1.0f, 1.0f);
            const float angle = random.range(0.0f, k_twoPi);
            const float ring = std::sqrt(std::max(0.0f, 1.0f - y * y));
            return math::Vec3{ring * std::cos(angle), y, ring * std::sin(angle)};
        }

        math::Vec3 inCone(const math::Vec3& axis, float angle, GoreRandom& random)
        {
            const math::Vec3 offset = randomDirection(random) * std::sin(angle) * random.next();
            const math::Vec3 turned = axis + offset;
            return math::lengthSquared(turned) > 0.0f ? math::normalize(turned) : axis;
        }

        std::vector<math::Vec3> lumpPoints()
        {
            GoreRandom random{.state = 0x51ED270Bu};
            std::vector<math::Vec3> points;

            for (const std::array<float, 3>& corner : k_icosahedron)
            {
                const math::Vec3 point = math::normalize(math::Vec3{corner[0], corner[1], corner[2]});
                points.push_back(point * random.range(0.6f, 1.15f)
                    * math::Vec3{1.0f, random.range(0.55f, 0.9f), 1.0f});
            }

            return points;
        }

        lockstep::Result<render::MeshHandle> createLump(render::Renderer& renderer, std::span<const math::Vec3> points)
        {
            std::vector<render::PbrVertex> vertices;
            std::vector<uint32_t> indices;

            for (const std::array<uint32_t, 3>& face : k_icosahedronFaces)
            {
                const math::Vec3 normal = math::normalize(math::cross(points[face[1]] - points[face[0]],
                    points[face[2]] - points[face[0]]));
                const math::Vec3 tangent = math::normalize(points[face[1]] - points[face[0]]);

                for (const uint32_t corner : face)
                {
                    indices.push_back(static_cast<uint32_t>(vertices.size()));
                    vertices.push_back(render::PbrVertex{{points[corner].x, points[corner].y, points[corner].z},
                        {normal.x, normal.y, normal.z}, {0.0f, 0.0f}, {tangent.x, tangent.y, tangent.z, 1.0f}});
                }
            }

            return renderer.createMesh(std::span<const render::PbrVertex>(vertices), std::span<const uint32_t>(indices));
        }

        render::ParticleEffectDescription mistDescription(render::TextureHandle atlas)
        {
            return render::ParticleEffectDescription{.texture = atlas,
                .atlasColumns = k_effectAtlasColumns,
                .atlasRows = k_effectAtlasRows,
                .firstFrame = k_smokeFrame,
                .frameCount = k_smokeFrames,
                .burstCount = 12,
                .lifetime = {1.4f, 2.6f},
                .speed = {0.3f, 1.8f},
                .coneAngle = math::k_pi,
                .spawnExtent = {0.08f, 0.08f, 0.08f},
                .acceleration = {0.0f, -0.25f, 0.0f},
                .drag = 2.2f,
                .size = {0.12f, 0.32f, 0.5f, 0.4f},
                .sizeScale = {0.7f, 1.3f},
                .color = {{0.4f, 0.025f, 0.025f, 0.45f}, {0.32f, 0.02f, 0.02f, 0.25f}, {0.25f, 0.015f, 0.015f, 0.0f},
                    0.35f},
                .spin = {-1.0f, 1.0f},
                .softness = 0.25f};
        }

        render::ParticleEffectDescription spurtDescription(render::TextureHandle atlas)
        {
            return render::ParticleEffectDescription{.texture = atlas,
                .atlasColumns = k_effectAtlasColumns,
                .atlasRows = k_effectAtlasRows,
                .firstFrame = k_dotFrame,
                .frameCount = 1,
                .shape = render::ParticleShape::Stretched,
                .burstCount = 22,
                .lifetime = {0.4f, 0.9f},
                .speed = {2.2f, 4.2f},
                .coneAngle = 0.2f,
                .spawnExtent = {0.02f, 0.02f, 0.02f},
                .acceleration = k_gravity,
                .drag = 0.4f,
                .size = {0.016f, 0.013f, 0.008f, 0.5f},
                .sizeScale = {0.7f, 1.6f},
                .color = {{0.3f, 0.012f, 0.012f, 1.0f}, {0.24f, 0.01f, 0.01f, 0.95f}, {0.18f, 0.008f, 0.008f, 0.0f},
                    0.7f},
                .stretch = 0.04f,
                .softness = 0.02f};
        }

        std::vector<audio::SoundHandle> loadSounds(EngineContext& engine)
        {
            std::vector<audio::SoundHandle> sounds;
            const audio::SoundDescription description{.is3d = true, .minDistance = 2.0f, .maxDistance = 60.0f};

            for (uint32_t number = 1; number <= k_maxSounds; ++number)
            {
                const std::string path = std::string(k_soundStem) + std::to_string(number) + ".ogg";

                if (!engine.files.exists(path))
                {
                    break;
                }

                if (auto sound = engine.audio.loadSound(engine.files, path, description))
                {
                    sounds.push_back(sound.value());
                }
            }

            return sounds;
        }

        void registerEffects(HeadBurstAssets& assets, Effects& effects)
        {
            if (!effects.particles || !effects.atlas.isValid())
            {
                return;
            }

            auto mist = effects.particles->createEffect(mistDescription(effects.atlas));
            auto spurt = effects.particles->createEffect(spurtDescription(effects.atlas));

            if (mist && spurt)
            {
                assets.mist = mist.value();
                assets.spurt = spurt.value();
            }
            else
            {
                std::clog << "Game: head burst particles rejected\n";
            }
        }

        HeadBurstAssets* assetsOf(GameState& state, EngineContext& engine)
        {
            HeadBursts& bursts = state.gore.headBursts;

            if (bursts.isAssetsTried)
            {
                return bursts.assets ? &*bursts.assets : nullptr;
            }

            bursts.isAssetsTried = true;
            HeadBurstAssets assets{};
            assets.chunkHull = lumpPoints();
            auto chunk = createLump(engine.renderer, assets.chunkHull);
            auto flesh = engine.renderer.createMaterial(render::PbrMaterialDescription{.baseColorFactor = k_fleshColour,
                .metallicFactor = 0.0f,
                .roughnessFactor = 0.3f});
            auto bone = engine.renderer.createMaterial(render::PbrMaterialDescription{.baseColorFactor = k_boneColour,
                .metallicFactor = 0.0f,
                .roughnessFactor = 0.6f});

            if (!chunk || !flesh || !bone)
            {
                std::clog << "Game: head burst gibs not available\n";
                return nullptr;
            }

            assets.chunk = chunk.value();
            assets.flesh = flesh.value();
            assets.bone = bone.value();
            assets.sounds = loadSounds(engine);
            registerEffects(assets, state.effects);
            bursts.assets = std::move(assets);
            return &*bursts.assets;
        }

        math::Vec3 jointPosition(const NpcLook& look, uint32_t joint)
        {
            const std::span<const math::Mat4> joints = look.instance.jointMatrices();
            const math::Transform& placement = drawnPlacement(look);
            const math::Vec3 local = joint < joints.size() ? math::transformPoint(joints[joint], math::Vec3{0.0f, 0.0f, 0.0f})
                                                           : math::Vec3{0.0f, 0.0f, 0.0f};
            const math::Vec3 scaled{local.x * placement.scale.x, local.y * placement.scale.y, local.z * placement.scale.z};
            return placement.position + placement.rotation * scaled;
        }

        struct Neck
        {
            math::Vec3 point{0.0f, 0.0f, 0.0f};
            math::Vec3 outwards{0.0f, 1.0f, 0.0f};
        };

        std::optional<Neck> neckOf(const NpcLook& look)
        {
            const anim::LimbInfo* head = look.model != nullptr ? anim::findLimb(*look.model, anim::Limb::Head) : nullptr;

            if (head == nullptr || head->rootJoint >= look.model->skeleton.joints.size())
            {
                return std::nullopt;
            }

            const uint32_t parent = look.model->skeleton.joints[head->rootJoint].parent;
            const math::Vec3 point = jointPosition(look, head->rootJoint);
            const math::Vec3 below = parent != anim::k_noJoint ? jointPosition(look, parent) : point - k_up;
            const math::Vec3 along = point - below;
            return Neck{.point = point, .outwards = math::lengthSquared(along) > 0.0f ? math::normalize(along) : k_up};
        }

        void throwGib(HeadBursts& bursts, const HeadBurstAssets& assets, physics::PhysicsWorld& physics,
            const math::Vec3& point, const math::Vec3& direction, bool isBone, GoreRandom& random)
        {
            const HeadBurstTuning& tuning = bursts.tuning;
            const float size = tuning.gibSize * random.range(0.6f, 1.3f) * (isBone ? k_boneShare : 1.0f);
            std::vector<math::Vec3> hull;

            for (const math::Vec3& corner : assets.chunkHull)
            {
                hull.push_back(corner * size);
            }

            const math::Quat spin = math::angleAxis(random.range(0.0f, k_twoPi), randomDirection(random));
            const math::Transform start{.position = point + randomDirection(random) * 0.05f, .rotation = spin};
            auto body = physics.createBody(physics::BodyDescription{.shape = physics::ConvexShape{.points = std::move(hull)},
                .type = physics::BodyType::Dynamic,
                .transform = start,
                .density = isBone ? k_boneDensity : k_fleshDensity,
                .friction = 0.8f,
                .restitution = isBone ? 0.35f : 0.15f,
                .isDebris = true});

            if (!body)
            {
                return;
            }

            physics.setVelocity(body.value(), inCone(direction, 0.9f, random) * (tuning.gibSpeed * random.range(0.5f, 1.4f)),
                randomDirection(random) * random.range(4.0f, 18.0f));

            while (!bursts.gibs.empty() && bursts.gibs.size() >= tuning.maxGibs)
            {
                physics.destroyBody(bursts.gibs.front().body);
                bursts.gibs.erase(bursts.gibs.begin());
            }

            bursts.gibs.push_back(Gib{.body = body.value(), .isBone = isBone, .size = size, .previous = start,
                .current = start});
        }

        void splatterAround(GoreState& gore, const BloodAssets& assets, const physics::PhysicsWorld& physics,
            const math::Vec3& point, const math::Vec3& direction, GoreRandom& random)
        {
            const HeadBurstTuning& tuning = gore.headBursts.tuning;
            const auto coneRays = static_cast<uint32_t>(static_cast<float>(tuning.splatterRays) * k_coneShare);

            for (uint32_t ray = 0; ray < tuning.splatterRays; ++ray)
            {
                const math::Vec3 way = ray < coneRays ? inCone(direction, k_coneAngle, random) : randomDirection(random);
                const auto hit = physics.raycast(point, way, tuning.splatterReach);

                if (!hit || !isPaintable(gore.decals, *hit))
                {
                    continue;
                }

                BloodDecal decal = decalOnSurface(hit->point, hit->normal, random.range(0.0f, k_twoPi));
                const auto look = static_cast<std::size_t>(random.next() * static_cast<float>(k_splatterLooks));
                decal.material = assets.splatters[std::min(look, k_splatterLooks - 1)];
                decal.size = tuning.splatterSize * (1.0f - 0.6f * hit->distance / tuning.splatterReach)
                    * random.range(0.6f, 1.2f);
                decal.stretch = random.range(1.0f, 1.8f);

                if (fitDecal(physics, decal, k_minSplatter))
                {
                    addDecal(gore.decals, decal, gore.tuning.maxDecals);
                    layWallRun(gore.decals, assets, physics, *hit, decal.size, gore.tuning.runs, gore.tuning.maxDecals,
                        random);
                }
            }
        }

        void sprayBlood(GameState& state, const HeadBurstAssets* assets, const math::Vec3& point,
            const math::Vec3& direction, GoreRandom& random)
        {
            GoreState& gore = state.gore;
            const HeadBurstTuning& tuning = gore.headBursts.tuning;
            const auto droplets = static_cast<uint32_t>(static_cast<float>(gore.tuning.hitDroplets) * tuning.dropletScale);

            for (const math::Vec3& way : {direction, math::normalize(direction + k_up), k_up})
            {
                throwDroplets(gore.spray, SprayBurst{.point = point, .direction = way, .count = droplets / 2,
                    .speed = 4.5f, .spread = 0.9f}, gore.tuning.maxDroplets, random);
            }

            Effects& effects = state.effects;
            render::ParticleSystem* particles = effects.particles ? &*effects.particles : nullptr;
            BloodBurstTuning big = gore.bursts;
            big.amount *= tuning.particleScale;
            big.headshotScale *= 1.15f;
            spawnBloodBurst(gore.burstEffects, particles, effects.atlas, big, BloodBurstHit{.point = point,
                .direction = direction, .damage = 255, .isHeadshot = true});

            if (particles != nullptr && assets != nullptr && assets->mist.isValid())
            {
                static_cast<void>(particles->burst(assets->mist, render::ParticleBurst{.position = point,
                    .direction = k_up, .countScale = 1.0f, .sizeScale = 1.0f, .velocity = direction * 0.8f}));
            }
        }

        void spurt(GameState& state, const HeadBurstAssets* assets, const Neck& neck, float strength)
        {
            GoreState& gore = state.gore;
            const math::Vec3 way = math::normalize(neck.outwards + k_up * k_spurtLift);
            throwDroplets(gore.spray, SprayBurst{.point = neck.point, .direction = way,
                .count = static_cast<uint32_t>(8.0f * strength + 0.5f), .speed = 3.5f * strength, .spread = 0.2f},
                gore.tuning.maxDroplets, gore.headBursts.random);

            Effects& effects = state.effects;

            if (effects.particles && assets != nullptr && assets->spurt.isValid())
            {
                static_cast<void>(effects.particles->burst(assets->spurt, render::ParticleBurst{.position = neck.point,
                    .direction = way, .countScale = strength, .sizeScale = 1.0f}));
            }
        }

        void playSplatter(HeadBursts& bursts, const HeadBurstAssets* assets, audio::AudioSystem& audioSystem,
            const math::Vec3& point)
        {
            if (assets == nullptr || assets->sounds.empty())
            {
                return;
            }

            const auto pick = static_cast<std::size_t>(bursts.random.next() * static_cast<float>(assets->sounds.size()));
            static_cast<void>(audioSystem.play(assets->sounds[std::min(pick, assets->sounds.size() - 1)],
                audio::PlayDescription{.bus = audio::Bus::World, .volume = bursts.tuning.volume,
                    .pitch = bursts.random.range(0.9f, 1.08f), .position = point}));
        }
    } //namespace

    void showHeadBurst(GameState& state, EngineContext& engine, const BloodAssets& assets, NpcLook& look,
        const GoreEvent& event, GoreRandom& random)
    {
        HeadBursts& bursts = state.gore.headBursts;
        const HeadBurstAssets* burstAssets = assetsOf(state, engine);
        const std::optional<Neck> neck = neckOf(look);
        const bool hasHead = neck && look.model != nullptr && anim::hasLimbs(*look.model)
            && (look.severedLimbs & anim::limbBit(anim::Limb::Head)) == 0;
        const math::Vec3 head = neck ? neck->point + neck->outwards * k_headAboveNeck : event.point;

        sprayBlood(state, burstAssets, head, event.direction, random);
        splatterAround(state.gore, assets, engine.physics, head, event.direction, random);
        playSplatter(bursts, burstAssets, engine.audio, head);

        if (!hasHead)
        {
            return;
        }

        look.severedLimbs = static_cast<anim::LimbMask>(look.severedLimbs | anim::limbBit(anim::Limb::Head));
        look.isHeadBurst = true;
        state.gore.stumps.push_back(BleedingStump{.entity = look.entity, .limb = anim::Limb::Head});
        bursts.fountains.push_back(NeckFountain{.entity = look.entity});
        std::clog << "Game: head burst\n";

        if (burstAssets != nullptr)
        {
            for (uint32_t gib = 0; gib < bursts.tuning.gibsPerBurst + bursts.tuning.gibsPerBurst / 2; ++gib)
            {
                throwGib(bursts, *burstAssets, engine.physics, head, math::normalize(event.direction + k_up * 0.5f),
                    gib >= bursts.tuning.gibsPerBurst, random);
            }
        }

        if (look.ragdoll)
        {
            const uint32_t chest = ragdollBodyOfLimb(state.npcLooks.ragdolls, *look.model, *look.ragdoll, anim::Limb::Torso);
            pushNpcBody(engine.physics, state.npcLooks, look, chest,
                math::normalize(event.direction + k_up * 0.25f) * bursts.tuning.push, std::nullopt);
        }
    }

    void presentHeadBursts(GameState& state, EngineContext& engine, float alpha, float seconds)
    {
        HeadBursts& bursts = state.gore.headBursts;
        const HeadBurstAssets* assets = bursts.assets ? &*bursts.assets : nullptr;

        std::erase_if(bursts.fountains, [&](NeckFountain& fountain)
        {
            fountain.seconds += seconds;
            fountain.pulse -= seconds;
            const auto found = std::find_if(state.npcLooks.looks.begin(), state.npcLooks.looks.end(),
                [&](const NpcLook& look) { return look.entity == fountain.entity; });

            if (found == state.npcLooks.looks.end() || fountain.seconds > bursts.tuning.fountainSeconds)
            {
                return true;
            }

            if (fountain.pulse <= 0.0f)
            {
                fountain.pulse = bursts.tuning.fountainPulse;

                if (const std::optional<Neck> neck = neckOf(*found))
                {
                    spurt(state, assets, *neck, 1.0f - 0.6f * fountain.seconds / bursts.tuning.fountainSeconds);
                }
            }

            return false;
        });

        std::erase_if(bursts.gibs, [&](Gib& gib)
        {
            gib.seconds += seconds;

            if (gib.seconds > bursts.tuning.gibSeconds || !engine.physics.isAlive(gib.body))
            {
                engine.physics.destroyBody(gib.body);
                return true;
            }

            if (const std::optional<math::Transform> placement = engine.physics.transform(gib.body))
            {
                if (math::lengthSquared(placement->position - gib.current.position) > 0.0f
                    || math::dot(placement->rotation, gib.current.rotation) < 1.0f)
                {
                    gib.previous = gib.current;
                    gib.current = *placement;
                }
            }

            return false;
        });

        if (assets == nullptr)
        {
            return;
        }

        for (const Gib& gib : bursts.gibs)
        {
            math::Transform drawn = math::interpolate(gib.previous, gib.current, alpha);
            drawn.scale = math::Vec3{gib.size, gib.size, gib.size};
            engine.renderer.submit(assets->chunk, gib.isBone ? assets->bone : assets->flesh, drawn);
        }
    }

    void clearHeadBursts(HeadBursts& bursts, physics::PhysicsWorld& physics)
    {
        for (const Gib& gib : bursts.gibs)
        {
            physics.destroyBody(gib.body);
        }

        bursts.gibs.clear();
        bursts.fountains.clear();
    }

} //namespace game
