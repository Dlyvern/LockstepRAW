#ifndef LOCKSTEP_GAME_AUDIO_COMBAT_AUDIO_HPP
#define LOCKSTEP_GAME_AUDIO_COMBAT_AUDIO_HPP

#include "Player/PlayerCommand.hpp"
#include "Player/PlayerStatus.hpp"
#include "Weapons/Penetration.hpp"
#include "Weapons/ShotAim.hpp"

#include <Lockstep/Audio/AudioTypes.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace lockstep::assets
{
    class FileSystem;
    struct Scene;
} //namespace lockstep::assets

namespace lockstep::audio
{
    class AudioSystem;
} //namespace lockstep::audio

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // The combat and ambience sounds, loaded from `sounds/` (Data/sounds by default). A missing
    // file leaves its list empty and that sound silent.
    // playerShots, playerFarShots: per loadout weapon, its gunshot close and distant
    //                          (`weapons/<m4|glock|v45>_shot_<n>`, `weapons/<...>_shot_far_<n>`).
    // npcShots, npcFarShots:   NPC rifles, close and distant (`weapons/merc_shot_<n>`,
    //                          `weapons/merc_shot_far_<n>`).
    // concreteHits, metalHits, fleshHits: bullet impacts (`impacts/<kind>_<n>`).
    // glassBreaks:             a pane shattering, crash and tinkle (`impacts/glass_break_<n>`).
    // dryFire:                 `weapons/dryfire` (reload sounds are WeaponSounds').
    // heartbeat:               looping, while the local player is downed (`player/heartbeat`).
    // street, hum:             looping ambience outdoors and indoors (`ambience/street_loop`,
    //                          `ambience/hospital_hum`).
    // siren:                   an occasional distant siren (`ambience/siren_distant`).
    // rain, rainIndoor:        looping rain heard outdoors and muffled through a roof
    //                          (`ambience/rain_outdoor`, `ambience/rain_indoor`).
    // thunder:                 distant thunder rolls while it rains (`ambience/thunder_<n>`).
    struct CombatSoundBank
    {
        std::array<std::vector<lockstep::audio::SoundHandle>, k_weaponCount> playerShots{};
        std::array<std::vector<lockstep::audio::SoundHandle>, k_weaponCount> playerFarShots{};
        std::vector<lockstep::audio::SoundHandle> npcShots;
        std::vector<lockstep::audio::SoundHandle> npcFarShots;
        std::vector<lockstep::audio::SoundHandle> concreteHits;
        std::vector<lockstep::audio::SoundHandle> metalHits;
        std::vector<lockstep::audio::SoundHandle> fleshHits;
        std::vector<lockstep::audio::SoundHandle> glassBreaks;
        std::optional<lockstep::audio::SoundHandle> dryFire;
        std::optional<lockstep::audio::SoundHandle> heartbeat;
        std::optional<lockstep::audio::SoundHandle> street;
        std::optional<lockstep::audio::SoundHandle> hum;
        std::optional<lockstep::audio::SoundHandle> siren;
        std::optional<lockstep::audio::SoundHandle> rain;
        std::optional<lockstep::audio::SoundHandle> rainIndoor;
        std::vector<lockstep::audio::SoundHandle> thunder;
    };

    // Volumes and distances, editable from the dev panel.
    // shotVolume, npcShotVolume: gunshot volumes.
    // farDistance:     metres from the listener beyond which the distant gunshot variants play.
    // impactVolume:    bullet impacts.
    // hurtVolume:      the thud when the local player is hit.
    // fleshHitVolume:  a confirmed hit on a character heard by everyone but the shooter (scaled by
    //                  the hit's strength).
    // headshotVolume:  factor on a head hit, which also layers a second, lower flesh sound.
    // heartbeatVolume: the downed heartbeat.
    // streetVolume, humVolume: ambience loops at full weight.
    // indoorStreet:    how much of the street loop is heard indoors.
    // ambienceFade:    seconds the ambience takes to cross between indoors and outdoors.
    // sirenMinSeconds, sirenMaxSeconds: pause between distant sirens.
    // sirenVolume:     the siren.
    // rainVolume:      the rain loop outdoors in full rain.
    // rainIndoorVolume: the muffled rain loop indoors in full rain.
    // rainHeardIndoors: how much of the outdoor rain loop still comes through indoors.
    // thunderVolume:   distant thunder.
    // thunderMinSeconds, thunderMaxSeconds: pause between thunder rolls (World time).
    struct CombatAudioTuning
    {
        float shotVolume{0.9f};
        float npcShotVolume{0.8f};
        float farDistance{28.0f};
        float impactVolume{0.6f};
        float hurtVolume{0.8f};
        float fleshHitVolume{0.8f};
        float headshotVolume{1.4f};
        float heartbeatVolume{0.8f};
        float streetVolume{0.6f};
        float humVolume{0.5f};
        float indoorStreet{0.2f};
        float ambienceFade{1.5f};
        float sirenMinSeconds{25.0f};
        float sirenMaxSeconds{70.0f};
        float sirenVolume{0.35f};
        float rainVolume{0.55f};
        float rainIndoorVolume{0.45f};
        float rainHeardIndoors{0.12f};
        float thunderVolume{0.7f};
        float thunderMinSeconds{20.0f};
        float thunderMaxSeconds{60.0f};
    };

    // An axis-aligned interior box (a level's Zone_Interior_* node), for the ambience.
    struct InteriorBox
    {
        math::Vec3 min{0.0f, 0.0f, 0.0f};
        math::Vec3 max{0.0f, 0.0f, 0.0f};
    };

    // A bullet impact combat audio found this frame (the local player's shots), for visual effects.
    // A round that goes through something gives an impact on both faces.
    // point, normal: where the ray hit and the surface normal there (for an exit face, the
    //               direction the round left in).
    // body:          the static or dynamic body hit (null for a character controller).
    // isFlesh:       it hit a character.
    // isExit:        the far face of something the round went through.
    struct CombatImpact
    {
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 normal{0.0f, 1.0f, 0.0f};
        lockstep::physics::BodyId body{};
        bool isFlesh{false};
        bool isExit{false};
    };

    // A gunshot combat audio heard this frame, for visual effects.
    // position: where the shot came from (the local player's eye, else the shooter's chest).
    // isLocal:  fired by this machine's player.
    struct CombatShot
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        bool isLocal{false};
    };

    // One armed NPC as combat audio needs it this frame.
    // key:       a stable id (the host's entity id), for its shot counter.
    // position:  where it is drawn.
    // shotCount: its replicated shot counter.
    struct NpcShotSource
    {
        uint32_t key{0};
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        uint8_t shotCount{0};
    };

    // What combat audio reads each frame.
    // scene:          the loaded level's scene (interior boxes), or null.
    // physics:        the physics world (bullet impact ray).
    // localIndex:     this machine's player.
    // localShots:     the local command's shot counter (no network delay for the own weapon).
    // localController: the local pawn's controller, ignored by the impact ray (may be null).
    // eye, aim:       the local view, for the own shots' impacts (`aim` only without `command`).
    // command:        the local player's newest command: the own shots' directions, as the host
    //                 traces them (shotDirection()), or null.
    // level:          the loaded level (penetration materials), or null.
    // spread, penetration: the shot rules, or null (then the shots go along `aim` and stop at
    //                 the first surface).
    // statuses:       both players' statuses (partner gunshots and reloads, damage, downed).
    // positions:      per player, where its pawn is drawn, if it is.
    // npcs:           the armed NPCs.
    // listener:       the listener position (camera).
    // rain:           how hard it rains, 0 (no rain sounds) to 1.
    // worldSeconds:   World time since the previous frame (thunder timing).
    // seconds:        Player time since the previous frame.
    struct CombatAudioFrame
    {
        const lockstep::assets::Scene* scene{nullptr};
        const lockstep::physics::PhysicsWorld* physics{nullptr};
        std::size_t localIndex{0};
        uint8_t localShots{0};
        lockstep::physics::ControllerId localController{};
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        math::Vec3 aim{0.0f, 0.0f, -1.0f};
        const PlayerCommand* command{nullptr};
        const SceneLevel* level{nullptr};
        const SpreadSettings* spread{nullptr};
        const PenetrationSettings* penetration{nullptr};
        PlayerStatuses statuses{};
        std::array<std::optional<math::Vec3>, k_playerCount> positions{};
        std::span<const NpcShotSource> npcs{};
        math::Vec3 listener{0.0f, 0.0f, 0.0f};
        float rain{0.0f};
        float worldSeconds{0.0f};
        float seconds{0.0f};
    };

    // Gunshots, impacts, hurt and downed sounds, and ambience. Presentation only, never
    // replicated: each machine plays them from the shot counters and statuses it already has.
    // bank, tuning:  the sounds and their levels.
    // lastLocalShots: the local shot counter last seen.
    // lastShots:     per player, the shot counter last seen.
    // lastDamage, lastHits: the local player's damage and hit counters last seen.
    // npcShots:      per NPC key, its shot counter last seen.
    // heartbeat, street, hum, rain, rainIndoor: looping voices, if playing.
    // indoorWeight:  0 outdoors to 1 indoors, eased.
    // interiors:     the level's interior boxes; `scene` is the scene they came from.
    // sirenSeconds:  time to the next distant siren.
    // thunderSeconds: World time to the next thunder roll while it rains.
    // random:        presentation-only random state.
    // hasLast:       the counters above are valid (false after loading).
    // impacts, shots: what updateCombatAudio() found this frame (replaced every call).
    struct CombatAudio
    {
        CombatSoundBank bank{};
        CombatAudioTuning tuning{};
        uint8_t lastLocalShots{0};
        std::array<uint8_t, k_playerCount> lastShots{};
        uint8_t lastDamage{0};
        uint8_t lastHits{0};
        std::unordered_map<uint32_t, uint8_t> npcShots{};
        std::optional<lockstep::audio::VoiceHandle> heartbeat{};
        std::optional<lockstep::audio::VoiceHandle> street{};
        std::optional<lockstep::audio::VoiceHandle> hum{};
        std::optional<lockstep::audio::VoiceHandle> rain{};
        std::optional<lockstep::audio::VoiceHandle> rainIndoor{};
        float indoorWeight{0.0f};
        std::vector<InteriorBox> interiors{};
        const lockstep::assets::Scene* scene{nullptr};
        float sirenSeconds{30.0f};
        float thunderSeconds{12.0f};
        uint32_t random{0x2545F491u};
        bool hasLast{false};
        std::vector<CombatImpact> impacts{};
        std::vector<CombatShot> shots{};
    };

    // Loads every combat and ambience sound found under `sounds/`, logging what is missing.
    // audio:  the audio system.
    // files:  the mounted content.
    // Returns: the loaded sounds.
    [[nodiscard]] CombatSoundBank loadCombatSounds(lockstep::audio::AudioSystem& audio,
        const lockstep::assets::FileSystem& files);

    // Plays this frame's combat sounds and keeps the ambience loops going.
    // combat: the combat audio state.
    // audio:  the audio system.
    // frame:  what happened this frame.
    void updateCombatAudio(CombatAudio& combat, lockstep::audio::AudioSystem& audio, const CombatAudioFrame& frame);

    // Plays the fleshy impact of a confirmed hit on a character, 3D at the hit point on the World
    // bus. Call from frame() for gore events the shooter's machine has not already played.
    // combat:     the combat audio state.
    // audio:      the audio system.
    // point:      world point of the hit.
    // strength:   how hard the hit was (about 0.3 light, 1 a player's shot, up to 2).
    // isHeadshot: a head hit: louder and lower, with a second layered sound.
    void playFleshHit(CombatAudio& combat, lockstep::audio::AudioSystem& audio, const math::Vec3& point,
        float strength, bool isHeadshot);

#if LOCKSTEP_DEV_TOOLS
    // Draws the combat audio section of the Game dev panel: volumes and distances.
    // combat: the combat audio state.
    void combatAudioDevSection(CombatAudio& combat);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_AUDIO_COMBAT_AUDIO_HPP
