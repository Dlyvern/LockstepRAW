#ifndef LOCKSTEP_GAME_NPCS_NPC_HPP
#define LOCKSTEP_GAME_NPCS_NPC_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
#include <Lockstep/World/Entities.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game
{
    namespace math = lockstep::math;

    // Which side an armed NPC fights for. Mercs fight the players and the squad; the squad (the
    // two officers who go in with the players) fights the mercs.
    enum class NpcFaction : uint8_t
    {
        Merc,
        Squad
    };

    // An NPC's overall state.
    // Idle:   at its post, unaware (squad: following the players).
    // Alert:  heard or was told of a threat; takes cover facing where it came from.
    // Combat: fights a target it has seen.
    // Dead:   a body; it stays where it fell.
    enum class NpcMode : uint8_t
    {
        Idle,
        Alert,
        Combat,
        Dead
    };

    // What an NPC is doing right now inside its mode.
    // Hold:   stands (or crouches) and watches.
    // Move:   walks or runs along a path (see NpcMoveReason).
    // Cover:  hides at its cover spot.
    // Peek:   stands up or steps out of cover and shoots when it sees its target.
    // Reload: changes magazines in cover.
    enum class NpcAction : uint8_t
    {
        Hold,
        Move,
        Cover,
        Peek,
        Reload
    };

    // Why an NPC moves, for callouts and the dev panel.
    enum class NpcMoveReason : uint8_t
    {
        None,
        Cover,
        Advance,
        Flank,
        FallBack,
        Follow
    };

    // A thing an NPC may target: a player pawn or another NPC.
    // kind:  what it is.
    // index: the pawn index or the index into NpcRoster::npcs.
    struct NpcTargetRef
    {
        enum class Kind : uint8_t
        {
            None,
            Pawn,
            Npc
        };

        Kind kind{Kind::None};
        uint32_t index{0};

        friend bool operator==(const NpcTargetRef&, const NpcTargetRef&) = default;
    };

    // A place to fight from: where the NPC hides from its threat and where it shoots from.
    // hide:  foot position out of the threat's sight (crouched for low cover).
    // peek:  foot position to shoot from (the same as `hide` for low cover: it stands up).
    // isLow: low cover: crouch to hide, stand to shoot.
    struct CoverSpot
    {
        math::Vec3 hide{0.0f, 0.0f, 0.0f};
        math::Vec3 peek{0.0f, 0.0f, 0.0f};
        bool isLow{false};
    };

    // One host-simulated armed NPC: a World-domain entity with a capsule controller, its senses,
    // its tactics and its weapon. Host only; the client sees the replicated entity and the
    // NpcStateMessage.
    // entity, controller: its entity (kept while dead, as the body) and its controller (null
    //               while dead).
    // faction:      merc or squad.
    // group:        merc floor group (0 ground, 1 second floor, 2 third) or k_squadGroup.
    // name:         the marker it spawned at, for logs.
    // home, homeYaw: where it spawned and which way it faced.
    // health:       hit points left.
    // mode, action: what it is doing (see NpcMode, NpcAction).
    // actionSeconds: World seconds left of the current action (cover, peek, reload, hold).
    // yaw:          facing (0 looks along -z, as for pawns).
    // isCrouched:   its capsule is low (hiding behind low cover).
    // verticalSpeed, isGrounded: falling state.
    // senseTimer:   World seconds until it looks around again.
    // target:       what it aims at; Kind::None without one.
    // canSeeTarget: it saw the target on its last look.
    // threat:       where it last saw or heard the enemy.
    // hasThreat:    `threat` is valid.
    // threatAge:    World seconds since `threat` was confirmed.
    // reactionSeconds: World seconds left before it reacts to a target it just spotted.
    // suppression:  how pinned down it is (0 calm; above the threshold it stays in cover).
    // path, pathIndex: the path being walked and the next waypoint.
    // moveReason:   why it moves.
    // isRunning:    moves at run speed.
    // stuckSeconds: World seconds it has made no progress on its path.
    // cover:        the spot it fights from, if any.
    // failedPeeks:  peeks in a row without seeing the target (it repositions after a few).
    // isPeekSuccessful: it saw its target during the current peek.
    // hasFallenBack: it already pulled back since its group started falling back.
    // rounds:       rounds left in its magazine.
    // burstLeft:    shots left of the current burst.
    // fireCooldown: World seconds until the next shot may fire.
    // aimSeconds:   World seconds it has tracked its current target (accuracy improves).
    // shotCount:    shots fired, wrapping (replicated for muzzle flashes).
    // decisionTimer: World seconds until it reconsiders its position.
    // avoidSpot:    the goal of its last path that it got stuck on; not chosen as cover again.
    struct Npc
    {
        lockstep::world::EntityId entity{};
        lockstep::physics::ControllerId controller{};
        NpcFaction faction{NpcFaction::Merc};
        uint8_t group{0};
        std::string name;
        math::Vec3 home{0.0f, 0.0f, 0.0f};
        float homeYaw{0.0f};
        int32_t health{0};
        NpcMode mode{NpcMode::Idle};
        NpcAction action{NpcAction::Hold};
        float actionSeconds{0.0f};
        float yaw{0.0f};
        bool isCrouched{false};
        float verticalSpeed{0.0f};
        bool isGrounded{false};
        float senseTimer{0.0f};
        NpcTargetRef target{};
        bool canSeeTarget{false};
        math::Vec3 threat{0.0f, 0.0f, 0.0f};
        bool hasThreat{false};
        float threatAge{0.0f};
        float reactionSeconds{0.0f};
        float suppression{0.0f};
        std::vector<math::Vec3> path{};
        std::size_t pathIndex{0};
        NpcMoveReason moveReason{NpcMoveReason::None};
        bool isRunning{false};
        float stuckSeconds{0.0f};
        std::optional<CoverSpot> cover{};
        uint8_t failedPeeks{0};
        bool isPeekSuccessful{false};
        bool hasFallenBack{false};
        int32_t rounds{0};
        int32_t burstLeft{0};
        float fireCooldown{0.0f};
        float aimSeconds{0.0f};
        uint8_t shotCount{0};
        float decisionTimer{0.0f};
        std::optional<math::Vec3> avoidSpot{};
    };

    // Capsule height of an NPC crouched behind low cover, metres.
    inline constexpr float k_npcCrouchHeight{1.2f};

    // Group of the squad officers (merc groups are floors 0..2).
    inline constexpr uint8_t k_squadGroup{3};

    // Number of NPC groups (three merc floors and the squad).
    inline constexpr std::size_t k_npcGroupCount{4};

    // What a group of NPCs knows together.
    // isEngaged:      it has fought since it spawned (for "Clear." and fall back).
    // members:        NPCs spawned into it.
    // losses:         of them, dead.
    // isFallingBack:  it lost too many and pulls back.
    // flankSeconds:   World seconds until one member may try a flank.
    // calmSeconds:    World seconds with no member in combat (ends the engagement).
    // calloutSeconds: World seconds until a member may call out again.
    struct NpcGroup
    {
        bool isEngaged{false};
        uint32_t members{0};
        uint32_t losses{0};
        bool isFallingBack{false};
        float flankSeconds{0.0f};
        float calmSeconds{0.0f};
        float calloutSeconds{0.0f};
    };

    // A shot fired this tick, which NPCs hear and feel pass by.
    // origin:     the shooter's eyes.
    // end:        where the shot stopped.
    // isPolice:   fired by a player or the squad (mercs react to it), else by a merc.
    // shooter:    who fired.
    struct NpcShotEvent
    {
        math::Vec3 origin{0.0f, 0.0f, 0.0f};
        math::Vec3 end{0.0f, 0.0f, 0.0f};
        bool isPolice{true};
        NpcTargetRef shooter{};
    };

    // A line an NPC calls out, sent to the client and shown as a subtitle near the speaker.
    // speaker: index into NpcRoster::npcs.
    // id:      dialogue line id.
    struct NpcCallout
    {
        std::size_t speaker{0};
        std::string id;
    };

    // A hit an NPC's shot landed, for the host rules to apply.
    // target:  who was hit.
    // damage:  hit points.
    // source:  the shooter's eyes.
    // shooter: the NPC that fired (index into NpcRoster::npcs).
    struct NpcHit
    {
        NpcTargetRef target{};
        int32_t damage{0};
        math::Vec3 source{0.0f, 0.0f, 0.0f};
        std::size_t shooter{0};
    };

    // Tuning of one faction's fighters.
    // health:       hit points.
    // damage:       hit points one of its shots takes.
    // spread:       aim error (radians, standard deviation-like) of a settled shot.
    // magazine:     rounds before it reloads.
    struct NpcFighterSettings
    {
        int32_t health{100};
        int32_t damage{6};
        float spread{0.06f};
        int32_t magazine{25};
    };

    // NPC AI tuning, editable from the dev panel (host).
    // merc, squad:     per-faction fighter tuning.
    // sightRange:      farthest it sees a target, metres.
    // sightHalfAngle:  half the sight cone, radians (doubled once alert or fighting).
    // closeSense:      it notices anyone this close regardless of facing, metres.
    // hearingRange:    gunshots on its own floor within this are heard, metres.
    // hearingOtherFloor: the same through floors.
    // senseInterval:   World seconds between looks around (perception runs at this rate).
    // reactionMin/Max: delay before an unaware NPC reacts to a target it spots, World seconds
    //                  (half that when already alert or fighting).
    // walkSpeed, runSpeed: m/s of World time.
    // turnRate:        radians per World second.
    // burstMin/Max:    shots per burst.
    // shotInterval:    World seconds between shots of a burst.
    // burstPauseMin/Max: pause between bursts.
    // aimSettleSeconds: tracking time after which aim error is lowest (3x at the start).
    // movingTargetSpread: aim error multiplier for a target moving at run speed.
    // reloadSeconds:   magazine change.
    // coverMin/Max:    World seconds hiding between peeks.
    // peekMin/Max:     World seconds of a peek.
    // suppressionPerHit, suppressionPerNearMiss: suppression a hit or a shot passing within
    //                  nearMissDistance adds; suppressionDecay is lost per World second;
    //                  above suppressionThreshold it stays in cover and aims worse.
    // nearMissDistance: metres.
    // coverSearchRadius: how far it looks for cover, metres.
    // lostTargetSeconds: unseen this long, it moves up to find the target.
    // giveUpSeconds:   unseen this long, it calms down to Alert.
    // flankMin/Max:    World seconds between flank attempts of a group.
    // fallBackLosses:  share of a group dead that makes it fall back.
    // squadFollowDistance: how close the squad keeps to the players, metres.
    // squadLeash:      beyond this from the nearest player the squad regroups, metres.
    // calloutRange:    a client shows a callout within this of the speaker, metres.
    // calloutCooldown: World seconds between callouts of a group.
    // playerDamage:    hit points a player's shot takes from an NPC.
    struct NpcSettings
    {
        NpcFighterSettings merc{};
        NpcFighterSettings squad{.health = 45, .damage = 12, .spread = 0.06f, .magazine = 30};
        float sightRange{32.0f};
        float sightHalfAngle{1.0f};
        float closeSense{3.0f};
        float hearingRange{35.0f};
        float hearingOtherFloor{14.0f};
        float senseInterval{0.1f};
        float reactionMin{0.45f};
        float reactionMax{0.9f};
        float walkSpeed{1.6f};
        float runSpeed{3.2f};
        float turnRate{6.0f};
        int32_t burstMin{3};
        int32_t burstMax{5};
        float shotInterval{0.12f};
        float burstPauseMin{0.5f};
        float burstPauseMax{1.1f};
        float aimSettleSeconds{1.2f};
        float movingTargetSpread{1.6f};
        float reloadSeconds{2.5f};
        float coverMin{1.0f};
        float coverMax{2.4f};
        float peekMin{1.4f};
        float peekMax{2.8f};
        float suppressionPerHit{0.6f};
        float suppressionPerNearMiss{0.25f};
        float suppressionDecay{0.35f};
        float suppressionThreshold{0.8f};
        float nearMissDistance{1.5f};
        float coverSearchRadius{12.0f};
        float lostTargetSeconds{6.0f};
        float giveUpSeconds{20.0f};
        float flankMin{12.0f};
        float flankMax{20.0f};
        float fallBackLosses{0.5f};
        float squadFollowDistance{3.5f};
        float squadLeash{8.0f};
        float calloutRange{30.0f};
        float calloutCooldown{2.0f};
        int32_t playerDamage{34};
    };

    // The host's armed NPCs.
    // npcs:      every NPC spawned in this level (the dead stay as bodies).
    // groups:    shared knowledge per group (k_npcGroupCount).
    // shots:     shots fired this tick (players, squad and mercs), cleared every tick.
    // callouts:  lines called out this tick, sent to the client and shown, cleared every tick.
    // random:    xorshift state for aim error, timings and choices.
    // calloutSeconds: World seconds until anyone may call out again.
    struct NpcRoster
    {
        std::vector<Npc> npcs;
        std::array<NpcGroup, k_npcGroupCount> groups{};
        std::vector<NpcShotEvent> shots;
        std::vector<NpcCallout> callouts;
        uint32_t random{0x2545F491u};
        float calloutSeconds{0.0f};
    };

    // Returns: a readable name for `mode`, for logs and the dev panel.
    [[nodiscard]] const char* toString(NpcMode mode) noexcept;

    // Returns: a readable name for `action`, for logs and the dev panel.
    [[nodiscard]] const char* toString(NpcAction action) noexcept;

    // Returns: a readable name for `reason`, for logs and the dev panel.
    [[nodiscard]] const char* toString(NpcMoveReason reason) noexcept;
} //namespace game

#endif //LOCKSTEP_GAME_NPCS_NPC_HPP
