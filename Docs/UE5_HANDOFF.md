# LockstepRAW -> Unreal Engine 5: handoff

## How to use this file

- **Audience:** an AI coding assistant (Claude Code) working in a fresh UE5 C++ project. You have
  none of the old repo's context. This file is your context.
- **What it is:** a reference spec of a game that was built over 18 commits (2026-11) on a custom
  C++20/Vulkan engine ("LockstepRAW", renderer library "Vela"). The game is being rebuilt in UE5.
  Port the **design, rules, tuning and content**, not the engine.
- **Read order:** sections 1 and 7 first (game and plan), then the system section you are working
  on. Put the parts you rely on into your project's CLAUDE.md, rewritten in your own words, rather
  than pasting this whole file.
- **Numbers:** every number comes from the old code defaults (metres, seconds, radians unless
  stated; "rpm" rounds per minute). Most were tuned by feel in play, so treat them as starting
  values. "World time" means time that bullet-time slows. "Player time" is never slowed.
  **[?]** marks something uncertain or inconsistent in the old code.
- **Units:** the old engine used metres with Y up. UE uses centimetres with Z up. Multiply
  distances by 100, and swap axes on import. Blender is Z up, and its glTF export converts to Y up.
- **Source archive:** `~/LockstepRAW_UE5_Export/LockstepRAW_content_source.tar.zst` contains
  `Content/source/`, `Data/` (sounds, dialogue, intro logos), `Story/`, `CREDITS.md` and
  `CLAUDE.md` (the old engine's decision log). Cooked engine data is not included and is useless
  in UE anyway. The old code stays at `/home/dlyvern/Projects/LockstepRAW` (Game/, Engine/) if
  you need to check a detail; paths in this file are relative to that repo.
- **Machine:** see section 6.3 before you build or run anything. The user's laptop has frozen
  several times from heavy agent runs.

---

## 1. The game

### 1.1 Pitch

- A two-player online co-op FPS: one host (listen server) and one client, always exactly two seats.
  Solo play (host alone) is allowed.
- Genre: grounded police tactical shooter that turns into horror. Bullet-time is the signature
  mechanic. The vibe is Trepang2 (movement, gunfeel, gore, slow-mo) plus Underhell (grounded
  cop story sliding into horror).
- Art direction: **realistic** (PBR, Trepang2-like). The user explicitly does **not** want a
  body-cam/VHS look for gameplay. The only body-cam touch is the title card in the intro.
- Tone: grounded, dry, tired. Humour comes from money, bureaucracy and fatigue. Nobody explains
  the plot. Horror is absent in the first missions, apart from rare deniable hints.

### 1.2 Story (Story/bible.md, Story/mission_01.md, Story/foreshadowing.md)

Many names are still **OPEN** (undecided by the user). Don't invent final names. Use the
placeholders.

- **Setting:** Port Merrow, a fictional rust-belt river-port city. The district is Carver Street /
  Saint Abel: brick tenements, an unmaintained park, boarded shops, the hospital.
- **Leads:** two broke flatmates. Both are patrol cops at the underfunded 14th Precinct and
  part-time members of its Special Response Team (SRT, six officers).
  - Player 1 is patient, dry, keeps the flat running.
  - Player 2 is quick-mouthed, a good shot, bad with money, loyal.
  - Names are OPEN. Both are men in the draft (the chief says "gentlemen").
- **Chief:** placeholder name Roy Calder. Sarcastic about money and secretly caring. He is the
  radio voice through the hospital and keeps pushing the leads down.
- **Station NPCs:** desk sergeant (29 days to retirement), armorer (counts every round, daughter on
  the honor roll), range officer (half deaf), holding guard (buys prisoners sandwiches), bullpen
  officers (Rodriguez brings cake Friday), prisoners.
- **Squad:** a sergeant squad lead, Rodriguez (proposed), and two more. Two AI squad officers go in
  with the players.
- **Weapons in fiction:** the bible says fictional designations only. The KRISS-style SMG is
  always called the **"V-45"**. **Conflict:** the code calls the other guns "M4" and
  "Glock 17" (real names). Ask the user before shipping.

**Spoilers (team only, never stated in level 1):**

- A hidden research program runs a lab under Saint Abel Memorial (the "East Annex"). The test
  subjects are the two officers themselves.
- The "terrorists" are the program's own contractors running a field trial. They move material
  up and out. The basement hostages are bait.
- The chief is in on it. He herds the leads down. They were chosen because they are close.
- **The lab:** the leads find an empty, clean lab with two chairs with restraints. Gas fills it
  and both pass out. Who triggers the gas (the chief remotely, or the firefight) is OPEN.
- **The chair:** they wake restrained side by side. The lights flicker, a power cut frees them,
  and "monsters" break in. Bullet-time triggers for the first time (automatic, unlimited). They
  shred them. Blackout, title card. The escape is not played.
- **The monsters are the squad.** The squad came down to rescue them, and the dosed leads saw
  them as monsters. The squad:
  - shouts the leads' names (distorted)
  - never quite shoots back
  - reaches toward them instead of attacking
  - wears SWAT gear under the "wrong" silhouette (visible on a second playthrough)
  The last one killed is the other player's character, revealed only at the end of the game.
- **One mind:** the two leads share one mind, and bullet-time is that shared power. So
  bullet-time is **one shared state for both players, never per player**.
  - Before the chair: no bullet-time.
  - In the chair fight: automatic and unlimited.
  - Afterwards: a player-controlled shared meter.
  - **Note:** the old code has a dev/test version with no limit: either player holds Alt (see 2.6).
- **After level 1:** the chief interrogates them ("the squad is dead, it's your fault"). From
  then on NPCs only ever address one person.

### 1.3 Mission 1, "Saint Abel" (title OPEN), about 40-50 min plus the morning and station

| Part | Level | Content | State in old code |
|---|---|---|---|
| 1 Morning | apartment (linked into district) | Wake in the flat. Alarm, coffee, mugs, radio, mirror, lockers (each player takes their own gear: badge + sidearm). Front door needs both players geared. Walk through the park to the car. | Implemented |
| 2 Station | station | Lobby (sergeant), chief's office briefing (2nd floor, both players at the desk), bullpen chatter, basement armory (armor, V-45, ammo), holding cells (optional), 6-lane range (optional pop-up targets), car. About 5 min, no horror. | Implemented |
| 3A Hospital A | hospital_a | Arrival in an **alley across the street at sunset in rain**. Cordon: police cars, ambulances, squad van, squad outside, cordon officer. Go round the back through the yard and **loading dock** into the ground floor (lobby, ER). Then two ward floors with mercs (F2 wards, F3 ICU/theatres), survivors barricaded in a lounge on F3 (left behind; the chief's radio says "hostages are below"), and the keyed **service lift** down. | Built, dressed, playable. Mercs are opt-in (`--dev-mercs`) [?: enable for real play] |
| 3B Hospital B | hospital_b | Lift arrival in an underground parking garage (decks P0/P1/P2), with the first deniable horror: light faults, radio static, a drag trail, a figure gone when the flashlight arrives. A fire shutter drops and **separates the squad**. Bridge to the **East Annex** (old wing: waiting hall, offices, pharmacy, chapel, old wards; mercs). Long basement (laundry, boiler, morgue), a utility tunnel west, sewers, a **hidden door** of newer concrete, the research wing, and **the lab**: gas, then the chair fight. | Greybox only (`Content/source/scripts/hosp_b2.py`). Markers are authored; the gas, chair, monsters and horror beats are **not coded** |
| Next | interrogation | A bare room with the chief. No dialogue yet. | Not started |

Hospital A geometry (from `Content/source/scripts/hosp_a.py`, `hosp_a_outside.py`; Blender
coordinates in metres, Z up):

- **Building:**
  - footprint x 0..60, y 0..30
  - storeys G z0, F2 z4, F3 z8, roof z12
  - two stair cores (West at x 0.45, East at x 53.4; halls at y 21..27.4)
  - main lifts and a service lift shaft (x 37..40, y 26.5..29.7)
- **Floors:**
  - G: ER bays with a nurse island, lobby desk, cafeteria, security office
  - F2: wards (3 beds per side)
  - F3: ICU bays (west), operating theatres (east), survivors lounge (NW, barricaded), crates
    at the service lift
- **Outside:**
  - street to the south, plaza, ambulance bay with canopy (west), parking (east)
  - loading-dock pit with two truck bays, a ramp up to the yard, and a delivery truck in bay 2
  - yard with dumpsters, pallets, gas cage, generator, staff cars, fence
  - neighbouring brick blocks, with the arrival alley at x 25..31 across the street
- **Beats in order:**
  - Arrival (28,-40)
  - Yard (60,40)
  - GroundFloor (46,26)
  - Ward_F2 (3.5,22,4)
  - ICU_F3 (56.5,22,8)
  - Survivors (10,22,8)
  - Interact_ServiceLift (38.5,26.2,9.2)

Hospital B beats (hosp_b2.py), in order:

- GarageP0, GarageP1, SquadSplit, GarageP2
- Bridge, AnnexF2, AnnexGround
- Basement, Tunnel, Sewer
- HiddenDoor, Lab

Planned (unused) markers:

- `Horror_LightFault_1/2`, `Horror_DragTrail`, `Horror_Figure`, `Horror_RadioStatic`,
  `Horror_SewerSound`
- `Chair_P1/P2`, `Gas_Vent_1..4`, `Spawn_Monster_1..6`, `Lab_PowerCut`, `Lab_TitleCard`
- `Body_N` (dead staff placement)

---

## 2. Gameplay systems: rules and tuning

### 2.0 Global

- Simulation runs at a fixed **60 Hz** tick (at most 5 ticks per frame), and rendering
  interpolates. Simulation code never reads the wall clock.
- There are two time domains:
  - **Player:** always scale 1. It drives pawn movement, kick, lean, reload, bleed-out and revive.
  - **World:** scaled by bullet-time. It drives physics, NPCs, effects, the World audio bus,
    animated lights and weather.
- **Presentation-only** things never replicate and are spawned from frame updates or confirmed
  events, never from re-simulated ticks:
  - animation, head motion, sounds, particles, decals, gore visuals, cloth
- **Client-only by design:** hallucinations and local NPC barks.

### 2.1 Movement (Game/Player/PawnMovement.hpp, PawnLean, PawnMantle, PawnVault)

Feel target: Trepang2 pace, weighty camera.

**Capsule**

| Item | Value |
|---|---|
| Capsule radius / standing height | 0.3 / 1.8 m |
| Crouch height / slide height | 1.2 / 1.0 m |
| Eye height | capsule top minus 0.2: stand 1.6, crouch 1.0, slide 0.8 |
| Step offset / slope limit | 0.35 m / 45° |
| Growing taller | only if the bigger capsule fits. Leaving a slide falls back to crouch if standing doesn't fit |

**Speeds and acceleration**

| Item | Value |
|---|---|
| Walk (default, no key) | 3.8 m/s forward |
| Run (Shift, forward only) | 6.4 m/s. Capped at 6.0 with the weapon raised |
| Back / strafe / sprint-strafe | 3.0 / 3.4 / 4.2 m/s |
| ADS cap / crouch cap | 2.4 / 2.2 m/s |
| Diagonal | ellipse through the forward and side speeds. Analog magnitude clamped to 1 |
| Run requires | sprint held, not aiming, standing, not sliding, forward input > 0 |
| Ground accel | 40 m/s² below walk speed, 14 m/s² from walk to run speed |
| Decel | 26 m/s² |
| Turn (sideways) rate | blends from 40 at walk speed to 20 m/s² at run speed |
| Air steering / air drag | 4 m/s² / 0.2 per second (no input) |
| Gravity | 20 m/s². Grounded stick-down force 4.5 m/s |
| Stamina | none for movement (breathing stamina exists, see 2.3) |

**Jump and landing**

| Item | Value |
|---|---|
| Jump height | 0.55 m (v0 ≈ 4.69 m/s) |
| Coyote time / jump buffer | 0.1 / 0.1 s |
| Jump from crouch or slide | stands up if it fits. A slide keeps its speed into the jump |
| Ceiling hit | zeroes upward speed |
| Hard landing | starts at fall speed 8.5 m/s, full severity at 14 m/s. Slowdown 0.2–0.6 s by severity. Planar velocity ×0.4 on impact; the speed cap recovers linearly over the last 0.2 s |
| Fall damage | **none** |

**Slide**

| Item | Value |
|---|---|
| Trigger | fresh crouch press while sprint is held, forward input, grounded, standing, planar speed ≥ 4.6 m/s, cooldown over |
| Duration | 0.75 s |
| Speed | starts at max(8.5, current) and decays quadratically to 2.6: v = end + (start−end)·(t_left/T)². It never speeds up |
| Steering | none (0 rad/s). The slide keeps its start direction |
| Cooldown | 0.6 s after the slide ends |
| Early end | blocked: travelled < 30% of the intended distance in a tick |
| After the slide | crouch if crouch is still held |

**Lean (Q/E)**

| Item | Value |
|---|---|
| Eye offset | 0.35 m sideways |
| Time to full lean | 0.2 s, linear |
| Wall check | clearance 0.2 m. Two rays (at the eye and 0.35 m below it) shorten the lean |
| Body | rolled about the feet. Camera roll at full lean 0.17 rad |
| Not allowed | while running, sliding or mantling |

**Vault** (tried first; jump press with forward input ≥ 0.5, on the ground, in coyote time or sliding)

| Item | Value |
|---|---|
| Obstacle | top 0.45–1.3 m above the feet, thickness ≤ 0.9 m (thicker → mantle) |
| Probes | 0.3, 0.55, 0.8 m height. Reach = radius + 0.55 + 0.07·speed |
| Landing | floor at most 1.5 m below. Land margin 0.12 + 0.05 per m/s over walk speed |
| Motion | cross speed = entry speed clamped to 4.2–7.5 [? clamp inferred]. Feet clear the top by 0.08. Rise 6.5 + 0.6 per m/s over walk |
| Exit | keeps 0.85 of planar speed, initial drop 1.5 m/s |
| Weapon | not ready during the vault |

**Mantle** (when vault doesn't apply)

| Item | Value |
|---|---|
| Trigger | buffered jump, forward ≥ 0.5, ledge top 0.4–1.3 m above the feet |
| Detection | wall ray at 0.4 m height (reach radius + 0.6). Top ray 0.15 m past the face. Top normal.y ≥ 0.7. Headroom check |
| Motion | rise at 5.5 m/s to ledge + 0.02, then 0.7 m forward over 0.12 s |
| Exit | speed capped to walk (3.8) |
| Abort | if blocked or on a ceiling hit |

**Weapon readiness**

- After running stops, the weapon cannot fire or aim for 0.15 s. This also applies during kicks.

**Kick (V)** (Game/Player/KickSettings.hpp, Melee/KickRules.cpp; host decides, sent to the client as a reliable event)

| Item | Value |
|---|---|
| Timing | 0.5 s long, single impact at 0.17 s, cooldown 0.2 s after |
| Movement while kicking | ×0.35 |
| Can start | grounded or in coyote time, not sliding or mantling |
| Hit volume | range 1.2 m from the capsule axis, cone cos 0.8 (~37°), strike height 0.85 m. Anything within 0.6 m always hits. NPC radius 0.35 |
| Mercs | 30 damage + torso flinch (≥ 30 also staggers) |
| All NPCs (squad too) | shoved 0.7 m (swept) |
| Physics toys | +7 m/s forward, +2 m/s up |
| Breakable glass | three rays at 0.45 / 0.85 / 1.25 m height (range + 0.1) shatter panes |
| Doors | **no kick effect**. Doors are interact-only |
| First-person pose | procedural leg (KickPose): chamber 0.08 s, strike 0.17 s, hold 0.07 s, thigh lift 2.05 rad, knee 1.7 → 0.05, spine lean back 0.15 |

### 2.2 Camera, head motion, first-person body

**FOV**

| Item | Value |
|---|---|
| World FOV | **60° vertical** (option 50–80) |
| ADS | eases to 52° |
| Sprint | +0.085 rad (rate 11/s) |
| Bullet-time | −0.05 rad |
| Return rate | 6/s |
| Viewmodel FOV | separate, fixed **55° vertical**. Depth squeezed so the weapon never clips walls. Arms/weapon never cast shadows |

**HeadMotion** (Game/Presentation/HeadMotion.hpp)

| Item | Value |
|---|---|
| Eye height easing | 12/s |
| Step smoothing | 25/s. Steps > 0.45 m snap |
| Bob stride | walk 1.7, run 2.75, crouch 1.25 m per cycle (matched to the clips). Walk stride applies at 1.6 m/s, run stride at 3.75 m/s |
| Bob depth | walk 0.022, run 0.04, crouch 0.012 m. Side bob = 0.45 × vertical. Fade 8/s |
| Landing dip | 0.42 per m/s of fall speed, as an impulse into a spring (k 140, damping 13). Max 0.22 m. Needs fall speed ≥ 1.5 m/s and air time ≥ 0.12 s |
| Jump take-off dip | 0.55 m/s |
| Roll | strafe 0.018 at full strafe, slide −0.07, vault −0.08, rate 8/s |
| Vault eye | smoothing 13/s, max offset 1.6 |
| Recoil punch | 0.35 rad/s per shot (spring 160/20). ×0.5 ADS, ×0.75 crouched |
| Bullet-time | bob ×0.75, roll rate ×0.5, landing damping ×1.4 (blend 4/s) |

**HeadWeight** (camera weight)

| Item | Value |
|---|---|
| Acceleration lean | pitch and roll 0.06 rad per m/s of velocity change. Spring 150/15, limit 0.045 |
| Turn roll | 0.0035, limit 0.03 |
| Head lag | 0.0025, limit 0.01. Off while aiming. Turn smoothing 20/s |
| Footfall plant | drop 0.10 m/s, pitch 0.05, spring 420/28 |
| Event pitch kicks | jump 0.25, landing 0.07 per m/s (falls ≥ 2.5 m/s), vault 0.45, kick start 1.1, kick impact 0.45 |
| Kick push | 0.9 m/s forward (spring 160/16) |

**Mouse**

| Item | Value |
|---|---|
| Sensitivity | 0.0025 rad/px (options 0.0005–0.008) |
| ADS multiplier | ×0.7, then ÷ optic magnification |
| Pitch limit | ±1.45 rad |
| Filtering | raw input, no smoothing or acceleration |
| Look application | every render frame on top of the last command (presentation), and on the tick for simulation |

**Accessibility**

- "Reduce head bob" cuts bob to 1/3. "Reduce motion blur" turns it off.

**First-person body**

| Item | Value |
|---|---|
| Visible parts | legs only (thighs and shins). The full body is visible only in mirrors and in shadows |
| Placement | pushed back ≥ 0.05 m so the hip cut stays below the view (hip radius 0.16, margin 0.08, assumes a half vertical FOV of 0.62 rad) |
| During a kick | may come up to 0.15 m forward |

UE: one full-body mesh with `bOwnerNoSee` for the shadow/mirror body, plus a legs-only mesh
(head/torso bones scaled to 0 or a separate mesh) with `bOnlyOwnerSee`. Arms and weapon go in a
separate first-person rig. UE 5.5+ has a "First Person Rendering" feature (FirstPersonFieldOfView,
FirstPersonScale on primitives): use it for the 55° viewmodel FOV and anti-clipping.

### 2.3 Weapons (Game/Weapons/*, WeaponCatalog.cpp)

**Loadout:** 1 = M4, 2 = Glock 17, 3 = KRISS ("V-45"), mouse wheel cycles. The wanted weapon rides
in the input command. The host switches (switch time = old weapon's holster + new weapon's draw;
no shots meanwhile; a running reload is cancelled; each weapon keeps its own magazine and reserve).

| Field | M4 | Glock 17 | KRISS / V-45 |
|---|---|---|---|
| Calibre | 5.56 | 9 mm | .45 ACP |
| Magazine | 30 | 17 | 30 |
| Max reserve / issued reserve | 150 / 120 | 85 / 68 | 120 / 90 |
| Fire rate | 750 rpm | 450 rpm | 900 rpm |
| Modes (B cycles) | Semi, Auto | Semi | Semi, Burst (3, +0.12 s pause), Auto |
| Damage multiplier → damage per hit (34 × mult) | 1.4 → 48 | 0.75 → 26 | 1.0 → 34 |
| Hits to kill a merc (100 HP), no penetration | 3 | 4 | 3 |
| Reload tactical / empty (s) | 3.633 / 3.8 | 2.8 / 3.033 | 2.0 / 2.6 |
| Holster / draw (s) | 0.567 / 1.033 | 0.467 / 0.75 | 0.35 / 0.5 |
| Recoil climb per shot (rad) | 0.0085 | 0.011 | 0.0065 |
| First-shot multiplier | 1.8 | 1.2 | 1.7 |
| Build per shot / max build | 0.07 / 1.9 | 0.02 / 1.3 | 0.05 / 1.7 |
| Horizontal wander per shot / band (rad) | 0.0055 / 0.034 | 0.004 / 0.028 | 0.0045 / 0.028 |
| Recoil recovery rate (1/s) | 6 | 9 | 6 |
| Spread hip / ADS (half-angle, rad) | 0.02 / 0.0008 | 0.016 / 0.0018 | 0.018 / 0.0012 |
| Moving spread (hip only) | +0.016 | +0.012 | +0.016 |
| Penetration power / max surfaces | 1.6 / 3 | 0.6 / 1 | 1.0 / 2 |
| Muzzle flash scale | 1.3 | 0.8 | 1.0 |
| Default optic | holographic (EOTech-style) | none (no rail) | iron sights (holo or 4x selectable) |

**Shared recoil rules**

- Wander pulls back to centre by 0.15 per shot. The kick is applied over 0.045 s.
- Recovery starts 0.14 s after the last shot. 90% of the climb recovers; 10% stays in the look.
- A 0.25 s pause ends a "string" (the build resets).
- Multipliers: ADS ×0.7, crouched ×0.75.
- Camera recoil moves the aim. The visual weapon kick is separate (below).

**Spread**

- Hip cone = (hip + min(|move|,1)·moving + 0.03 if sprinting) × (0.7 if crouched).
- ADS cone = ADS spread × crouch factor, with no movement penalty.
- Scatter is a **deterministic hash** of (shot number, shooter index), uniform over a disk. So
  client-side impacts match host hits without a second ray.

**Shot direction (ADS sight-line aiming)**

- A shot leaves the eye along the command's look (which already includes camera recoil), turned
  by `aimOffset`.
- `aimOffset` is the sight line the shooter actually saw:
  - from the hip: the look/crosshair
  - in ADS: the ray from the eye through the **drawn front post / optic axis**, including sway,
    breathing, weapon kick and head motion, blended by the aim-in amount
- The host clamps `aimOffset` to **0.14 rad**, then adds the spread cone.
- Iron-sight eye distance: M4 0.11 m, Glock 0.3 m. The KRISS rear sight sits 0.20 m in front of
  the eye.

**Fire control**

- The client runs the trigger (Semi/Burst/Auto). The host refuses shots faster than the cyclic
  rate (tolerance 25% of the interval).
- Shot counters wrap, so lost or duplicated packets never drop or double a shot.
- Firing on an empty magazine starts a reload and doesn't fire. A dry-fire sound exists.

**Ammo and reload**

- Reload only if not already reloading, the magazine isn't full, and reserve > 0. Empty magazine
  (0 rounds) → the empty reload time.
- Rounds move only at the end of the reload. **No chambered +1.**
- Issue/refill:
  - issue = full magazine, reserve = max(current, issued)
  - ammo pickup = max reserve
- Reload cue timelines (clip seconds; magazine out → drop magazine + sound; magazine in; charge):

| Reload | Clip length | Mag out | Mag in | Charge |
|---|---|---|---|---|
| M4 tactical | 3.633 | 1.0 | 2.033 | – |
| M4 empty | 3.8 | 1.033 | 2.067 | 2.667 |
| Glock tactical | 2.8 | 0.833 | 1.467 | – |
| Glock empty | 3.033 | 0.833 | 1.467 | 1.9 (slide) |
| KRISS tactical | 2.0 | 0.30 | 1.15 | – |
| KRISS empty | 2.6 | 0.30 | 1.20 | 1.75 |

- Viewmodel clips: FPAim (pose), ReloadTactical, ReloadEmpty, Draw, Holster. Fade in 0.1 s, fade
  out 0.15 s.
- In UE these cues become **AnimNotifies**.

**Hitscan, damage and penetration**

- Range 100 m from the eye. Characters always stop the round.
- **No friendly fire:** a player hitting a player or a squad NPC does no damage.
- Damage = max(1, round(34 × weapon mult × penetration damage scale)).
- NPC health: merc 100, squad 45.
- **No headshot or limb damage multipliers exist** [? probably intended; decide with the user].
  Head and leg hits change reactions and gore only.
- Penetration (Weapons/Penetration):
  - **Material** is guessed from node/material name keywords:
    - glass/window → Glass
    - concrete/brick/floor → Hard
    - metal/locker/car → Metal
    - wood/door/crate → Wood
    - cardboard/sofa/curtain → Soft
    - wall/plaster/ceiling → Drywall
    - unnamed level geometry → Hard; props → Soft
  - **Cost per metre of thickness:** Glass 2, Soft 3, Drywall 5.5, Wood 7, Metal 40, Hard 60.
  - **Thickness** comes from an inside/back trace. Anything > 0.45 m stops the round. The round
    passes if thickness × cost < remaining power, which then drops by that amount.
  - **Per surface passed:** damage ×= 0.85 × (power left / starting power); range ×0.7.
  - **Glass** is free and doesn't count toward max surfaces. Up to 4 panes break per shot.
  - Impacts are drawn on **both faces** (entry and exit).
  - In UE: use a **Physical Material** per surface (with a custom penetration-cost field) instead
    of name keywords.
- Thickest wall a shot goes through at full power:

| Weapon | Drywall | Wood | Metal | Hard |
|---|---|---|---|---|
| M4 | 0.29 m | 0.23 m | 40 mm | 27 mm |
| Glock | 0.11 m | 0.086 m | 15 mm | 10 mm |
| KRISS | 0.18 m | 0.14 m | 25 mm | 17 mm |

**Weapon sway and viewmodel** (Characters/WeaponSway, ViewModel)

| Item | Value |
|---|---|
| ADS time | 0.15 s linear |
| Raise/lower | 0.3 s |
| Sprint pose blend | 0.16 s |
| Vault swing | 0.1 s |
| Look-lag sway | 0.16 rad per rad of look, limit 0.08. Spring 85/11 (underdamped). Roll 0.6 per rad |
| Bob | 6 mm side (vertical 0.66 of that), roll 0.008. Strafe tilt 0.08 rad, shift 12 mm |
| ADS | all sway, bob and movement reactions ×0.35 |
| Movement reaction | kick 0.02, dip 0.6, spring 75/9, limit 35 mm. Muzzle pitch 0.06 / yaw 0.05 per m/s (limit 0.06) |
| Jump / landing | jump drop 0.3 m/s, pitch 0.5. Landing 0.06 drop + 0.1 pitch per m/s (≥ 1.5 m/s). Air spring 110/10, limit 0.05 |
| Lowered pose (X) | pitch 0.3, yaw 0.35, roll 0.1 |
| Sprint pose | pitch 0.35, yaw 0.6, roll 0.5. Per-step swing 0.07, bounce 16 mm, roll swing 0.06 |
| Visual kick (all weapons) | per shot: climb 0.75 rad/s, yaw ±0.3, roll 1.4 (70% clockwise), back 0.6 m/s. First shot ×1.5, build 0.04 up to 1.5. Springs: climb/yaw 300/24, roll 220/16, back 450/32. ×0.45 ADS, ×0.7 crouched. Caps 0.15 rad / 0.05 m |

**Breathing and breath hold**

- Sway amplitude 0.004 rad from the hip, 0.0028 rad aimed. Lissajous shape:
  yaw = sin(φ)·0.6, pitch = sin(2φ + 0.7).
- Breath rate 0.25/s at rest, 0.6/s at zero stamina (where sway is ×3.5).
- Stamina 0–1: drains 0.14/s while sprinting, recovers 0.1/s.
- **Hold breath:** Shift while aimed > 50%.
  - Steady for 3.0 s at sway ×0.12 (blend 6/s).
  - After 3 s it shakes: ×2.0 plus a 9 Hz tremble of 0.0018 rad until released.
  - Recovery = 2.5 s × fraction of the hold used (min 0.6 s). A full hold costs 0.2 stamina.
  - Sounds: breath_hold / breath_release. A light DoF cue (0.3) while holding.

**Optics** (Weapons/Optics)

- **Holographic (1x):**
  - reticle: 68 MOA ring (2 MOA line) + 1 MOA dot, HDR red (5, 0.2, 0.1)
  - projected **at infinity**, so it shows only through the window
  - cheek-weld eye distance 0.14 m
- **4x scope, picture-in-picture:**
  - true FOV 6.5° (apparent 26°), eye relief 0.075 m
  - eye-box radius 6 mm; leaving it shows the scope shadow
  - breathing sway ×1.6 while scoped; outer view blurred (DoF 0.35)
  - sensitivity ÷ 4
  - second-focal-plane duplex reticle with stadia and an illuminated dot (6, 0.15, 0.05)
  - shows only once aim-in > 0.05
  - the second view renders from the objective lens, without the viewmodel
  - scope render target: Low 256, Medium/High 512, Ultra 1024 px
- **UE:**
  - holo: a material on the glass using a reticle projected at infinity (camera-vector-based UV)
  - scope: a SceneCapture2D into a render target (capture only while ADS, low resolution,
    hidden-actor list for the viewmodel) plus a lens material with vignette/eye-box. Or use the
    cheaper "parallax lens material + FOV zoom" trick.

**Flashlight** (F when there's nothing to interact with)

- Warm LED (1.0, 0.93, 0.82), range 30 m, inner half-angle 8°, outer 24°.
- Casts shadows. Volumetric scattering 0.25 for your own beam, 0.7 for the partner's (looks
  better from outside).
- Switch-on flicker over 0.32 s: brightness 0.85, 0.08, 0.65, 0.2, 0.95, 0.55, then 1.
- The on/off state and a coarse pitch replicate to the partner, whose light sits on its gun.
- **Bug to fix:** Interact and flashlight share F, so one press can do both. Give them separate
  keys or make the priority explicit.

**Weapon zones** (`Zone_Safe_*` / `Zone_Hostile_*` boxes)

- Safe: weapon always lowered, no fire or ADS. Hostile: always up. Open (default): up, can lower
  with X.
- Safe wins where zones overlap. Safe becomes Open while combat is active.
- Holding a physics toy lowers the weapon.

**Brass and magazines**

- Casings (radius/length):
  - sizes: 5.56 4.8/45 mm, 9 mm 4.9/19 mm, .45 5.75/22.8 mm
  - pool of 100 per weapon, life 30 s
  - ejection: right 1.6–2.6 m/s, up 0.9–1.7, back 0–0.6, ±0.25 jitter; spin 18–40 rad/s
  - bounce: restitution 0.4, friction 0.6
  - per-surface sound: ≤ 3 bounces, above 0.5 m/s (full volume at 3.5)
  - shrink away over the last 0.5 s
- Dropped magazines:
  - max 8 per weapon, life 120 s
  - leave at 0.6–1.1 m/s along the magazine axis + 0.2 m/s right; restitution 0.2
- Weapons dropped by dead NPCs: max 32, life 300 s, 1 m/s along the shot + 0.6 m/s up.

**Muzzle effects**

- Muzzle flash light: intensity 22 (engine units), range 4 m, at least 1 frame / 0.02 s; your
  own ×0.35.
- Tracers: every 4th round, life 0.09 s, only beyond 1 m.
- Barrel heat: +1 per shot, max 14, decays 0.45/s. Smoke wisps above heat 3.
- First-person flash: a star sprite (3 variants, 2 frames), ×0.45 when aimed.

**Bullet holes** (decals)

- Base 0.07 m. Calibre scale: M4 1.0, Glock 1.1, KRISS 1.3.
- Surface scale: concrete 1.0, plaster 1.35, tile 1.8, wood 1.3, metal 0.75, glass 2.6 (star
  crack), fabric 0.6.
- Dented relief.
- Budget: Low 48 / Medium 96 / High 160 / Ultra 320, oldest first.
- A hole is removed when its body moves or breaks.
- Partner and NPC shots also leave holes (one muzzle ray, 60 m).

**HUD for weapons**

- Ammo "mag / reserve" bottom right (0.96, 0.90), red-orange when empty, "RELOADING" prefix.
- Weapon + mode line ("M4   AUTO").
- Hit marker 0.15 s; kill marker red 0.35 s.
- Crosshair: a tiny white dot.
- **No health bar** (removed on purpose).

### 2.4 Health, downed, revive, checkpoints (Player/PawnHealth, Simulation/PlayerCombat)

**Health and damage taken**

- Max HP 100. Damage taken per NPC bullet: merc 6, squad 12 (squad only in the twist).

**Downed**

- At 0 HP the player is **downed**, never killed outright. Downed means:
  - crawl at forced crouch with input ×0.6 (≈ 1.32 m/s)
  - no run, jump, kick, lean, aim, bullet-time or interact; the reload is cancelled
- Bleed-out: 30 s (Player time). Each hit while downed removes 0.15 s per HP.
- **Revive:** the partner holds F within 2.0 m (feet to feet, 3D) for 3.0 s. Releasing F or
  leaving range resets progress. The revived player gets 40 HP.

**Squad failure**

- Triggers: all present players are downed, or anyone bleeds out.
- After a 3.0 s fade, both players respawn at the last **checkpoint** with full HP and a full
  loadout. Living mercs return to their posts healed; dead mercs stay dead. The squad respawns
  near the checkpoint. Broken glass stays broken.

**HUD**

- Damage-direction marks: ≤ 6, 1.6 s each.
- Hurt flash: +0.6 per hit, decays 2.5/s, max 0.35, dark red.
- Downed tint 0.3. Heartbeat loop while downed.
- Texts: "DOWN N s" with "Crawl to cover..." or "Being revived N%". For the partner: "Hold F to
  revive your partner", "Reviving N%", "Partner down N s".

### 2.5 Interactions and level flow

**Interactions** (Game/Interaction)

- F within 2 m of the eyes. The first thing the look ray hits wins. The host checks reach with
  +0.3 m tolerance. State is host-authoritative and replicated.
- Doors and lockers rotate around a hinge. The moving parts are children of an `Interact_*`
  empty placed on the pivot.

**Morning** (apartment/district)

- Phases: Apartment → GoToCar → MorningComplete.
- Alarm clock: rings until silenced (shake 0.06 rad at rate 55).
- Coffee machine: brews 6 s. Mugs Mug_A/Mug_B, one per player.
- Radio, mirror, wall clock.
- `Interact_Locker_1/2`: either player opens either door (0.6 s, 100°). Gear `Interact_Gear_1/2`
  can be taken only by its owner; it gives badge + sidearm and arms the pawn.
- `Interact_FrontDoor`: needs this player geared, the partner geared and within 3 m; opens over
  1 s to 95°.
- `Interact_StreetDoor`: 1.2 s.
- Both players within 3 m of `CarSpot` → 2 s fade → next level.
- Toys (`Toy_*`; names with "ball" are throwable):
  - carry 0.75 m in front; throw (fire) 9 m/s + 1.5 up; drop 1 m/s
  - kick: speed × 0.9 + 1 m/s, lift 1.5
  - ball physics: density 90, restitution 0.65, friction 0.6
  - `Court*` nodes get a code-made basketball (r 0.12 m)

**Station**

- Phases: Report → Briefing → Armory → GoToCar → MissionStart.
- Report: F on `Interact_Briefing`, only when all players are within 4 m horizontally (±2 m
  vertically) of the chief's desk. Otherwise "wait for partner".
- Briefing: plays `station.chief.briefing.01-14` back to back (total subtitle time + 0.5 s).
- Armory: each player takes `Interact_ArmoryArmor`, `ArmoryWeapon`, `ArmoryAmmo`.
- Range (optional, `Target_*` up to 8):
  - the intro line plays when an armed player is within 18 m
  - targets fall in 0.2 s, stay down 3 s, rise over 0.6 s
  - all targets down → a "done" line
- `CarSpot` again → hospital.

**Hospital**

- `Beat_*` markers are both **triggers and checkpoints**.
- A beat is reached when a non-downed player is within 4 m horizontally and 2 m vertically.
  - Checkpoint = beat position ±1 m.
  - With mercs enabled, it spawns that floor's merc group (once per floor). Floor comes from
    `_G_`/`_F2`/`_F3` in the spawn name, else from height / 4 m.
- Objective switches from "enter" to "basement" at `Beat_GroundFloor`.
- `Interact_ServiceLift` (reach 3.5 m): "F  Take the service lift" → fade 2 s → loads hospital_b
  at 2.5 s.

**Level switch**

- Hold 1 s, then load; then 0.4 s hold and a 1.6 s fade in.
- Host-driven: the client follows. The checkpoint is cleared on load.

**Echo recorder**

- The host records both pawns' position/yaw/pitch per tick, plus interactions, during the morning.
- Replayed later as looping ghosts (client-only).
- An optional curiosity. In UE: Replay system or a simple custom recorder. Low priority.

### 2.6 Bullet-time

- **Old code (test rules):** while **either** player holds Left Alt or the middle mouse button,
  the host sets the World domain scale to **0.2**.
  - No meter, limit or cooldown. Downed players can't request it.
  - The scale and the tick it took effect are in every snapshot. Clients never set it themselves.
- **Story rules (to implement):**
  - none before the chair
  - automatic and unlimited in the chair fight
  - later, one **shared** player-controlled meter
  - always one global state for both players
- **Effects:**
  - Slows: physics, NPCs, effects, animated lights, weather, the World audio bus (pitch = scale,
    low-pass).
  - Not slowed: players (movement, aim, reload) in Player time.
  - Presentation:
    - DoF fades in (rate 5/s in, 3/s out)
    - FOV −0.05 rad
    - heavier head motion
    - footsteps at pitch ×0.88, volume ×0.8
    - shorter rain streaks
- **UE approach:**
  - Replicate `bBulletTime` and the scale in GameState, and call `SetGlobalTimeDilation(0.2)` on
    every machine.
  - Give player pawns, their weapons, their timers and the first-person rig
    `CustomTimeDilation = 1/0.2 = 5`.
  - **Risk:** CharacterMovementComponent prediction and the server's move-time-discrepancy
    checks under time dilation. Prototype this in milestone 1 with 2-player PIE and simulated
    lag. The fallback is no global dilation and slowing NPCs, physics substeps, Niagara and
    audio explicitly.
  - Chaos physics and Niagara follow global dilation.
  - Audio: push a SoundMix (World SoundClass pitch 0.2, low-pass ~1.4 kHz) or use FMOD bus
    pitch/low-pass.

### 2.7 NPC AI (Game/Npcs/*; host only)

**Factions and groups**

- Merc and Squad. Groups: merc floors 0/1/2 plus the squad.
- Modes: Idle → Alert → Combat → Dead (bodies stay).
- Actions: Hold, Move, Cover, Peek, Reload. Move reasons: Cover, Advance, Flank, FallBack, Follow.

**NpcSettings**

| Item | Merc | Squad |
|---|---|---|
| HP | 100 | 45 |
| Damage per shot to players | 6 | 12 |
| Base spread | 0.06 rad | 0.06 rad |
| Magazine | 25 | 30 |

**Senses**

- Sight 32 m, half-cone 1.0 rad (doubles, capped at π, once Alert or Combat). Close sense 3 m in
  any direction.
- Hearing: 35 m on the same floor (|dy| < 2.5 m), 14 m through floors. Mercs hear police shots;
  the squad hears merc shots.
- Perception runs every 0.1 s. Line of sight is a ray to the target's chest or eye.
- Target score = distance, +15 if the target is downed, −3 for the current target. Lowest wins.
- Reaction 0.45–0.9 s (halved when Alert; ≤ 0.2 s when attacked). Retarget 0.15 s.
- On first sight:
  - Combat mode, callout "Contact" (or "Two. Police" when it sees ≥ 2 players)
  - same-group Idle NPCs within 25 m become Alert (squad mates go straight to Combat)
  - if the alerted group is ≥ 3 mercs, 50% chance of "Hold them here"

**Movement**

- Walk 1.6, run 3.2 m/s, turn 6 rad/s. Crouched ×0.5.
- Crouch capsule 1.2 m.
- Faces the threat while moving in combat within 25 m.

**Firing**

- Bursts of 3–5 shots, 0.12 s apart, 0.5–1.1 s between bursts. Reload 2.5 s. Range 80 m.
- Aim settles over 1.2 s.
- Fires only when: in Combat, sees the target, reaction over, standing, not on a path, action is
  Peek or Hold, aim within 0.25 rad, not flinching.
- Spread = base × (1 + 2·unsettled) × (1 + min(suppression, 2)) × (1 + 0.6·min(targetSpeed/6.4, 1))
  × (0.7 if the target is downed). Error is roughly normal (sum of 3 uniforms).
- NPC bullets break glass (≤ 4 panes).

**Cover and peek**

- Hide 1.0–2.4 s, peek 1.4–2.8 s. Cover search radius 12 m.
- Candidate nodes must:
  - have a cover bit facing the threat (±1 of 8 directions)
  - be within ±1 m height
  - have a threat distance of 3–40 m
  - be ≥ 1.2 m from other NPCs' spots
- Score = distance + anchor weight × distance to anchor + random 0–1. The top 24 get raycast
  checks.
- Spot types:
  - hidden: geometry blocks a crouched head (1.0 m) within 2 m toward the threat
  - low cover: a standing head (1.6 m) sees the threat
  - corner: a 0.75 m side-step sees the threat
- "In cover" means within 0.6 m.

**Suppression**

- +0.6 per hit, +0.25 per near miss (a shot passing within 1.5 m). Decays 0.35/s.
- Above 0.8 the NPC stays in cover; above 1.2 a peek is aborted.

**Combat logic**

- Reconsider position every 1 s.
- Reload in cover when below 35% of the magazine ("Reloading").
- Lost target: unseen 6 s → advance (cover between 4 m and distance − 2 m; "Moving").
- Unseen 20 s → give up (Combat → Alert → Idle).
- Fallback chain when there is no cover: seek cover (retry 2.5 s); else hold and walk toward the
  threat, stopping 6 m short.
- After a long move: 30% chance of "Set".

**Group tactics**

- Engaged while anyone fights. "Clear" after 8 s with nobody alerted.
- **Flank** every 12–20 s when ≥ 2 members are fighting:
  - the flanker is the ready NPC with the most failed peeks ("Moving")
  - anchor = threat + random side × 8 m + 2 m back
  - a mate calls "Covering" and peeks at once
- **Fall back** when losses ≥ 50% (group ≥ 2): "Fall back. Next floor".
  - cover anchored 10 m beyond the threat, ≥ current distance + 4 m
  - members staggered 1.5 s apart
- "Man down": called by the nearest mate within 15 m.
- Callout cooldowns: 2 s per group, 1 s global (urgent ones skip the global).
- Callouts are shown to a client within 30 m of the speaker.

**Squad follow**

- Goal 3.5 m behind the nearest player, ±1.2 m to the side by index. Re-path beyond 4.5 m.
- Runs when more than 6 m away. Leash 8 m: seek cover near the players, else run to them.
- Beyond 10 m with no path → **warp** to an unseen spot behind the leader. "Unseen" means no
  player within 1 m and no player with view dot > 0.4 and a clear line.
- Squad cover is anchored on the nearest player (downed players get +100 penalty).

**Squad watch sectors (while holding)**

- 16 rays of 14 m measure openness. Score = 0.5·self + 0.25·each neighbour.
- Penalties: 0.9 × alignment with a mate's sector (mates within 12 m), 0.8 × facing² toward
  players within 6 m. +0.15 to keep the current sector.
- Re-pick every 5–9 s or after moving 1.5 m.
- Sweep ±0.35 rad over 7 s (phase offset 2.9 s per index), turning at 40% of the turn rate.
- A remembered threat (< 45 s) gets faced with a ±0.15 sweep.
- Glances at a player for 1.6 s (cooldown 7 s more) when:
  - the player stares at it: within 10 m, dot > 0.985, for 0.8 s; or
  - the player passes within 1.8 m faster than 0.5 m/s.

**Hit reactions** (presentation)

- Flinch 0.18 s World time (head ×2).
- **Stagger** 0.45 s at ×0.35 speed when damage ≥ 30 or on any leg hit.
- Additive springs on Mixamo bones: 2.4 Hz, damping 0.55, impulse 7 rad/s at 34 dmg.
  - torso takes 0.45 of limb hits; head ×1.6; max 0.7 rad
  - bone weights: Spine .3/.35/.35, Neck .4, Head .6, Shoulder .35, Arm .65, UpLeg .8, Leg .2
  - stagger fold: impulse 9, fold 0.35 rad, knee 1.6/0.4

**Ragdolls**

- Up to 32 simulating (≥ 1 s before the cap may freeze one).
- Impulses: kill 110 kg·m/s, bullet on a ragdoll 70, lift 0.2, blend 0.1 s.
- Physics: friction 0.8, damping 0.05 linear / 0.6 angular, self-collision on.
- Bodies stay shootable.

**Armed crouch**

- Clips RifleCrouchIdle/Walk and AimCrouchIdle/Walk. Fallback: upper body of RifleIdle/AimIdle
  (from "Spine") layered over CrouchIdle/Walk legs.

**Nav**

- Old code: a load-time 0.5 m grid with A* (agent radius 0.25 m, height 1.75 m, step 0.45 m).
  No doors, jumps or ladders.
- **UE:** NavMesh (Recast) plus EQS for cover queries, StateTree or Behavior Tree for the brain,
  and AI Perception (Sight, Hearing, Damage) with the numbers above. Optionally Smart Objects for
  cover spots.

**Placeholder NPCs** (`Npc_*`, client-only, looping idle, no physics)

- Role from the name: Chief, DeskSergeant, Armorer, RangeOfficer, Guard, Officer, Prisoner,
  Civilian, Survivor, SquadOutside, CordonOfficer.
- Pose by role (civilian/survivor crouch; squadoutside/cordonofficer/guard armed; others idle).
  Override with `_Idle/_Crouch/_Armed/_Aim`.
- Barks when the local player comes within 2.5 m (20 s cooldown): a random
  `<level>.<role>.idle.*` line.

**Legacy melee "Enemy"** (arena test target, 3 HP): don't port.

### 2.8 Gore (Game/Gore/*)

**Dismemberment**

- Only on a **killing hit or a hit on a corpse**.
- The limb is the nearest bone to the hit: head, upper arm, forearm, thigh or shin, L/R. The torso
  never severs. A limb can't be cut if it or its parent is already gone.
- Rule: the head always severs on a kill. Other limbs sever if damage ≥ 30 and roll < 0.6. NPC
  shots may sever too.
- **Head burst** instead of a head sever when roll < 0.35 × clamp(dmg/30, 0, 1.5) + close bonus.
  Close bonus = 0.3 × (1 − d/6) under 6 m. So about 40% at range, about 70% point-blank.
- Wound caps: models carry hidden `StumpCap`/`PieceCap` parts, shown at the outermost cut.
  - Cooked from `Content/source/characters/gore/wound.fbx` + `Textures_wound.png`.
  - **UE:** split meshes per limb (or Skeletal Mesh "hide bone" + spawn a severed-piece
    skeletal/static mesh with a cap). A Chaos Flesh/Dismemberment-style system is overkill; the
    common approach is per-limb mesh sections + bone break + cap mesh.
- Severed pieces: ≤ 24, life 60 s, launched at 5.5 m/s. They take bullet impulses and leave smear
  trails every 0.18 m.

**Blood (GoreTuning)**

- Decals: ≤ 320, oldest replaced. Splatter 1.0 m, reaching 3 m behind the hit.
- Pool 1.2 m, grows over 9 s, appears 0.7 s after death, floors only (normal.y ≥ 0.8).
- Wall runs: 65% of wall splatters run for 3–7 s.
- Decal fitting: footprint probes must agree (depth 0.025 m, alignment 0.85), else the decal
  shrinks ×0.6.
- Droplets: ≤ 400, 18 per hit (×3 for a sever), each leaving a drip decal (0.08 m per 1 cm
  droplet).
- Stumps: spurt 4 s (every 0.32 s), bleed 12 s.
- Head burst:
  - 8 flesh gibs + 4 bone bits (≤ 48, life 45 s)
  - neck fountain 1.1 s
  - 16 splatter rays, 4.5 m reach
  - chest push 240 kg·m/s
  - sound `impacts/headburst_*`
- Blood decal source art: `characters/gore/blooddecals/` (Fab "Blood Decals" 4K), `decal_*.png`,
  `gore.glb` (`Blood_Splat`, `Blood_Splatter`, `Blood_Pool`, `Blood_Run_1..3`).

**Character stains**

- ≤ 8 bind-pose stain spots per body (hit radius 0.075 m, stump 0.14 m).
- Grime 0.12. Blood dries in 150 s. Rain soak 20 s; drying indoors 240 s.
- **UE:** a skeletal-mesh material with a small array of stain positions (Custom Primitive Data
  or a dynamic material instance with vector params), or a render-target wound mask.

**Network**

- The host applies sever rules and sends gore events reliably. The client reports its local
  corpse hits to the host.

### 2.9 Breakable glass

- Every `Breakable_Glass*` object is one pane: its own object, origin at the pane centre, a thin
  box with a glass material.
- It has a collider and blocks bullets and players until broken.
- **One bullet** (player, partner or NPC) or a kick breaks it. The collider is removed and the
  bullet flies on unslowed.
- Host-authoritative broken set, replicated (late join included). Reset on level load, kept over
  checkpoint restarts.
- Effects:
  - shards 6 per m², 70% fly along the shot
  - floor debris 30 per m² (≤ 320 total)
  - sound at 0.9, 2–45 m
- **UE:** Chaos Geometry Collection (pre-fractured) or simply hide the pane + Niagara shards +
  decal. The simple route is closer to the old behaviour and cheaper to replicate.

### 2.10 Dialogue (Game/Dialogue, Data/dialogue/*.txt)

**Format**

- `id | speaker | text` per line. `#` comments. `@include <file>` (max depth 4).
- Speaker `hud` = on-screen text (objectives, notices, narration). Others show as
  `SPEAKER: text`.

**Subtitles**

- Time = 1.2 s + 0.055 s per character, clamped 2–8 s.
- One at a time, queued, wrapped at 70 columns.

**Triggers**

- exact id (objectives)
- prefix in order (briefing 01-14, range intro)
- random pick by prefix (barks)
- AI callouts (`hospital.gunman.callout.01..10`: Contact, ContactTwo, Moving, Covering,
  Reloading, Set, FallBack, ManDown, Hold, Clear; `hospital.squad.combat.*`)

**Files**

- `morning.txt` (hud only)
- `station.txt` (~83 lines: chief briefing 14, officer, prisoner, range officer, armorer,
  sergeant, guard, hud)
- `hospital.txt` (~100 lines, DRAFT, by beats 1-5)
- apartment/district/hospital_a only `@include` the others

**Not implemented**

- radio filter for the chief/squad
- `{P1}/{P2}` name substitution
- `hospital.chair.vo.*` (recording script only, played distorted; subtitles show
  `hospital.chair.shout.*`)

**UE**

- Import into a DataTable (CSV: Id, Speaker, Text) or String Tables for localisation.
- A subtitle subsystem with the timing rule above.
- Radio voices: a Submix with band-pass + distortion (MetaSounds or Submix effects).

### 2.11 Audio design (Engine/src/Audio, Game/Audio/*; FMOD Core in the old code)

**Buses**

- Master ← Music, Ambience, World, Player, Ui.
- **World** carries doors, NPC and partner shots, impacts, rain, thunder and physics. It follows
  bullet-time: pitch = scale, low-pass cutoff = 700·(22000/700)^scale (≈ 1.4 kHz at 0.2),
  smoothed 0.12 s.
- **Player** carries own footsteps, weapons, hurt and heartbeat. Always at 1.
- Defaults: 256 voices, doppler 0.5.

**Gunshots**

- Per weapon `weapons/<m4|glock|v45>_shot_1-4`, plus `_shot_far_1-3` beyond 28 m.
- 3D 4–220 m. Volume 0.9 for players, 0.8 for NPCs (`merc_shot`).
- **Tails:**
  - `tails/indoor_*`: 4–80 m, 0.45 × indoor weight
  - `tails/outdoor_*`: 20–500 m, 0.6 × (1 − indoor)
  - indoor weight = listener inside a `Zone_Interior_*` box, eased over 1.5 s

**Other combat sounds**

- Impacts 1.5–40 m at 0.6 (exit ×0.6).
- Flesh 0.8 × (0.55 + 0.45·strength). Headshot ×1.4 at pitch 0.86, plus a 2nd layer (0.7 vol,
  0.72 pitch).
- Reload 0.75. Casings 0.35, magazines 0.6, both per surface.
- Flashlight click 0.55, fire-mode switch 0.5, breath 0.4.

**Footsteps**

- Surface from the hit body's name keywords:
  - asphalt/road/parking/street → Asphalt
  - grass/lawn/turf → Grass
  - tile/ceramic/bathroom → Tile
  - wood/laminate/parquet → Wood
  - metal/steel/grate/rail → Metal
  - else → Concrete
  - **UE:** Physical Materials with a SurfaceType enum.
- Volumes walk 0.35, run 0.8, crouch 0.15. Jitter: pitch ±6%, volume ±12%.
- Interval = half a stride (walk 1.5 m, run 2.4 m, crouch 1.1 m). Local steps sync to head-bob
  footfalls.
- Landing: silent below 3 m/s, full volume at 10 m/s.
- Vault 0.75, kick swing 0.6, kick hit 1.0. 3D range 1.5–30 m. No immediate repeats.

**Ambience** (the same for every level, mixed by indoor weight and rain)

- `street_loop`: 0.6, 20% heard indoors.
- `hospital_hum`: 0.5 × indoor.
- Rain: `rain_outdoor` 0.55 (12% heard indoors), `rain_indoor` 0.45 × indoor.
- `siren_distant`: every 25–70 s, 120 m away at a random azimuth.
- `thunder_*`: delayed distance/343 s, placed 300 m out.
- Menu: rain 0.55, siren 0.35, street 0.25.

**Light buzz**

- `fluorescent_buzz` loop 0.3 (15% while dark). Crackle on each toggle (0.3, 0.14 s cooldown).
- Nearest 3 tubes within 12 m.

**Drips**

- Plinks synthesised in code (4 variants, 44.1 kHz, 0.12 s).

**UE options**

- **MetaSounds + Submixes + SoundClasses/SoundMix**: built in, fine for this scope, works with
  Audio Gameplay Volumes for indoor/outdoor.
- **FMOD for Unreal plugin**: the old design maps 1:1. It needs an FMOD licence and the
  attribution below.
- Recommend MetaSounds unless the user wants FMOD Studio.
- Gunshot tails: Audio Gameplay Volume or interior volumes driving the indoor weight; optionally
  Convolution Reverb on a submix.

### 2.12 Lights, weather, cloth, screens, effects (presentation, World time)

**Animated lights** (tags in the light's name, combinable with `_Shadow`)

- **`_Flicker`** (failing tube):
  - 3 s episodes, 0.035 s slots, steady dropout chance 0.006 per slot
  - burst chance 0.18 per episode, lasting 0.15–0.9 s
  - long-off chance 0.07: 0.25 s stutter, off 0.6–2.2 s, 0.4 s restrike
  - off level 0.03
  - quiet 3D buzz + crackles
- **`_FlickerHard`** (dying tube, mostly off): sparks 0.012 per slot; 0.12 chance of an on spell
  lasting 0.8–2.5 s.
- **`_SirenRed` / `_SirenBlue`**:
  - 0.72 s cycle (red first half, blue second)
  - double flash 0.065 s on, 0.05 s gap
  - colours red (1, 0.04, 0.02), blue (0.04, 0.16, 1)
  - lights within 3 m share a car phase; cars are offset ±8%
- **`_Pulse`**: cosine, 2.6 s period, low 0.15.
- Emissive lamp meshes within 1 m of an animated light dim with it.
- **UE:** a light-animation ActorComponent driving intensity + a material parameter (MPC or
  Custom Primitive Data) on nearby emissive meshes. Deterministic per seed.

**Static light conventions**

- `_Shadow`: casts shadows (old budget: nearest 4).
- `_Tube`: line light along local X, length = X scale. UE: Rect light or Point light with Source
  Length.
- `_Downlight`, `_WallWash`, `_Grille`, `_Fluorescent`: emission profiles. UE: IES profiles.
- Old budget: 16 local lights within 12 m of the eye, 3 m fade.
- UE with Lumen + VSM + MegaLights can afford more. Keep shadow casters limited on the 4 GB GPU.

**Rain** (`Level_Weather_Rain`)

- Streaks: 5000/s in a ~9×9 m box, 6.5 m above the eye, leading camera motion by 0.6 s; die
  inside interiors.
- Splashes within 9 m (≤ 24 per frame).
- Wet surfaces and puddles outdoors. Wetness soaks in 30 s, dries in 150 s.
- Overcast look: sun ×0.3, sky ×0.55, fog density 0.012, saturation ×0.85, clouds coverage 0.93.
- Lens drops (when looking up / into the wind, outdoors): 12/s.
- Wind-blown litter.
- **UE:** Niagara GPU rain attached to the camera, occlusion via a top-down scene-depth capture
  or Niagara collision; a wetness material function driven by an MPC; Volumetric Clouds;
  Exponential Height Fog with volumetric fog.

**Lightning**

- Only when rain ≥ 0.3. Interval 9–35 s ÷ rain. Distance 900–9000 m.
- 2–4 strokes (attack 0.012 s, decay 0.07 s).
- Brightness clamp(1800/d, 0.25, 1.4). Thunder delay = d/343 s, loudness clamp(1800/d, 0.15, 1.2).
- While it outshines the sun, it acts as the frame's directional light with shadows.

**Wind**

- 2.5 m/s clear, 9 m/s in rain. Gustiness 0.6 (new gust every 1.5–6 s, eased 1.2 s).
- Heading 70°, wandering ±25° over 90 s. Clouds drift 4× wind.
- Foliage sways outside interiors. UE: WindDirectionalSource + foliage material WPO.

**Cloth** (`Cloth_*`, e.g. hospital bed curtains)

- Verlet: 0.22 m spacing, 6 iterations, top row pinned, damping 0.985.
- Simulated within 14 m. People push through (capsule 0.32 × 1.8 m). Bullets kick it at
  3.5 m/s (0.4 m radius). Outdoor panels move with wind.
- Colliders removed.
- **UE:** Chaos Cloth on a skeletal mesh (or the Cloth asset editor in 5.4+), collision with pawn
  capsules. Or a cheap vertex-animation material for distant curtains.
- `Content/source/scripts/split_curtains.py` made the per-panel objects. Don't rerun the old
  `level_fix2.py`.

**Screens** (`Screen_*` empty at a display's centre facing Blender −Y)

- Emissive meshes within 0.35 m become the screen.
- Tags:
  - behaviour: `_Flicker`, `_Off`, `_Scroll`
  - content: `_Image` (keep authored image; default is a procedural terminal), `_Panel`
    (vending machine/sign)
  - colour: `_Blue/_Green/_Amber/_Red/_White`
- Each screen gets an unshadowed spot light (range 3.5 m), max 4 nearby.
- UE: emissive material with scanlines + a Rect light.

**Drips** (`Fx_Drip_*` 2-3 cm under a ceiling)

- A drop every 1.6 s ±60% falls to the floor, splashes, plinks and ripples a puddle (r 0.3 m).

**Particle markers**

- `Fx_Steam_*` 9/s, `Fx_Smoke_*` 5/s, `Fx_Dust_*` ceiling dust 30/s, all emitting along Blender +Z.
- Dust motes inside interiors.
- Impact effects: puff, grit, chunks, sparks (metal); muzzle smoke.
- **UE:** Niagara systems.

**Time-of-day looks** (`Level_Look_Day|Evening|Sunset|Night`; none = Day)

| Look | Values |
|---|---|
| Day | sun azimuth 145°, elevation 38° |
| Evening | sun azimuth 250°, elevation 5°, colour (1, .62, .38), intensity ×0.5 |
| Sunset (hospital A) | sun azimuth −120°, elevation 6°, colour (1, .56, .28), warm horizon glow, sky-coloured fog |
| Night | no sun, sky 0.04 |

- Shared grade: contrast 1.15, saturation 0.8, cool shadows / warm highlights split-tone, vignette
  0.3, film grain 0.035.
- Auto exposure. Motion blur 0.35, slight chromatic aberration, lens dirt.
- Hospital B uses Evening.
- **UE:** one Post Process Volume preset per look (or a DataAsset applied at level load) plus
  Sky Atmosphere + Directional Light + Volumetric Clouds.

**Mirrors**

- `Interact_Mirror` meshes are planar reflectors. The local player's full body is drawn only in
  mirrors.
- UE: Planar Reflection actor (costly; enable only near the mirror) or a SceneCapture.

**Interior zones** (`Zone_Interior_*` cube empties covering an interior incl. walls)

- Old use: dim sky light inside, add a fill light, drive indoor audio weight and the
  rain/dust/drying rules, bound the GI bake.
- **UE:** Lumen handles the lighting. Keep the boxes as an `AInteriorVolume` (audio weight,
  weather rules) or Audio Gameplay Volumes.

### 2.13 HUD, menus, intro, options

**HUD positions** (normalised screen)

| Element | Position | Size |
|---|---|---|
| Prompt (e.g. "F  Take the service lift") | (0.5, 0.56) | 28, warm (1, .86, .55) |
| Objective | top-left (0.03, 0.04) | 24 |
| Narration | (0.5, 0.64), max 4 lines, 5 s each, 0.6 s fade | |
| Caption | (0.5, 0.47) | 40 |
| Subtitles | (0.5, 0.88) | 26 |

- Screen fade to black.
- Combat HUD: see 2.3 and 2.4. Minimal, no health bar.

**Menus** (old immediate-mode UI, monospaced Cousine font)

- **Main:** title "L O C K S T E P", subtitle "CO-OP INCIDENT RESPONSE // FILE 0417". Buttons
  HOST GAME, JOIN GAME, OPTIONS, CREDITS, QUIT.
- **Backdrop:** a live drifting camera in hospital A's rainy sunset alley (eye 1.65 m at
  PlayerStart_1).
- **Host:** name (≤ 20 chars), UDP port (default 27020).
- **Join:** name, host IP, port. Direct IP only, no matchmaking.
- **Lobby:**
  - seats HOST / PARTNER with READY
  - the host picks the operation: Morning (district), Hospital A, Station, Hospital B, Test Arena
  - START when the partner is ready, or START SOLO with an empty seat
  - connect timeout 12 s
- **Pause:** RESUME, OPTIONS, QUIT TO MAIN MENU, QUIT GAME.
- **Options** (apply and save immediately):

| Tab | Contents |
|---|---|
| Graphics | preset + every quality field, window size, present mode, fullscreen, FOV 50–80 |
| Audio | Master/World/Player/Ambience/Music |
| Controls | sensitivity, invert Y, rebind every action, reset |
| Accessibility | subtitles, reduce head bob, reduce motion blur, skip intro |

- **Credits:** built from CREDITS.md + sounds credits, auto-scrolling, ending with "Thank you
  for playing."
- **UE:** CommonUI + UMG (CommonActivatableWidget stack, gamepad-ready), Enhanced Input user
  settings for rebinding (`UEnhancedInputUserSettings`, 5.3+), GameUserSettings for scalability.
  For online: Online Subsystem Null / direct IP `open <ip>` first; Steam/EOS later.

**Intro** (skippable; any key skips a card, Esc skips all; the warning shows once even with skip
enabled)

| Card | Duration | Content | Sound |
|---|---|---|---|
| Warning | 4.6 s (fades 0.7/0.7) | graphic violence / photosensitive epilepsy, "Headphones recommended." | warning_drone (Music 0.55) |
| Engine logo | 9.2 s | "Made with Vela" dust-mote wordmark (gather 0.7, hold, gust at 6.2) | vela_dark, vela_gust at 5.95 s |
| Middleware | 4.2 s | FMOD logo + PhysX logo + FMOD line | credits_swell |
| Title | 3.45 s | body-cam style "L O C K S T E P", "UNIT 07 CAM 2", REC | title_static, title_cut at 3.0 s |

- In UE, replace the Vela/PhysX cards with an Unreal Engine logo card, as the UE EULA branding
  rules allow; keep FMOD only if FMOD is used.
- Build the intro as a Level Sequence or UMG animations. Logos are in `Content/source/intro/` and
  `Data/intro/`.

### 2.14 Networking model

**Old model**

- Valve GameNetworkingSockets, UDP 27020. Listen server; Player1 = host.
- Every tick the host sends a full unreliable snapshot (~52 B per entity, 60 Hz).
- The client renders interpolated 6 ticks (100 ms) behind.
- Input: the client sends each tick's command plus up to 8 redundant older ones. The host buffers
  2 ticks.
- **PlayerCommand** (19 B):
  - move (int8×2), yaw, pitch
  - wrapping counters: shotCount, interactCount, reloadCount
  - flags: bulletTime, sprint, aim, lower, weaponUp, crouch, jump, interactHeld, flashlight,
    kick
  - lean −1/0/1, aimOffset (int16), optic, weapon
- **Prediction:** only the client's own pawn movement, with the same step code.
  - Reconcile (rewind + replay, 240-move history) when position differs > 0.01 m or any
    velocity/timer/stance/flag differs.
  - The correction fades ×0.85 per tick; > 1.5 m snaps.
- The trigger and the magazine are predicted for presentation only. The HUD never shows more than
  the host's magazine.
- **Lag compensation:** the host keeps 24 ticks of hittable positions and rewinds to the tick the
  client saw − 0.5. [? the comment says 250 ms; 24 ticks is 400 ms]

**Ownership**

| Host-authoritative | Client-only / presentation |
|---|---|
| Movement (validated), damage, hitscan, penetration, ammo, reload, weapon switch, fire rate | Animation (all, incl. motion matching), head motion, viewmodel |
| Kick resolution, downed/revive/restart, checkpoints | Sounds, particles, decals, bullet holes, blood decals |
| NPC AI and state, callouts | Cloth, rain, lightning, wind, animated lights |
| Bullet-time scale | Placeholder NPCs and their barks |
| Gore events (sever/head burst), broken glass set | Hallucinations / horror beats (planned; must stay client-only) |
| Interactions, room/station/hospital state, level flow, lobby | Echo ghosts |

**Reliable status per player** (on change)

- health, flags (joined/downed/aiming/lowered/crouched/sliding/reloading/flashlight/
  vaulting/kicking/switching)
- bleed-out, revive %, ammo, hit/kill/damage counters, flashlight pitch, optic, weapon

**UE mapping**

- Listen server. `ACharacter` + `UCharacterMovementComponent` subclass. Custom movement for slide,
  vault and mantle via custom compressed flags / `FSavedMove_Character` (or UE 5.4+
  `FCharacterNetworkMoveData` extension), so they are predicted and corrected.
- Lean as a replicated, predicted float.
- Shots: client fires locally (cosmetic), sends a Server RPC with a shot counter, aim offset and
  view timestamp. The server validates the rate, rewinds hitboxes (custom lag compensation), and
  multicasts or replicates the confirmed impacts.
- The same deterministic spread hash lets clients draw impacts at once.
- GAS is optional. It gives prediction keys for abilities (fire, reload, kick, bullet-time), but
  it is not required at this scale; plain components + RPCs are fine and simpler.
- Gore/glass/state: replicated arrays (FastArraySerializer) and reliable multicasts. Remember late
  join.
- Testing: PIE "Play As Listen Server", 2 players, with network emulation (Average/Bad profile).

---

## 3. Content inventory (`Content/source/` in the archive)

**Folders**

| Folder | Size | Contents |
|---|---|---|
| `apartment/` | 1.1 GB | `apartment.blend` + `.glb` (the flat: 2 beds, kitchen, bathroom, lockers, coffee machine, mirror). Linked into the district at second-floor height |
| `district/` | 1.4 GB | `district.blend` + `.glb`: building, stairs, street, park (trees, grass, court), the players' car (Crown Victoria). Export by making linked instances real, exporting, then reloading |
| `station/` | 1.0 GB | `station.blend` + `.glb`: 14th Precinct (lobby, chief's office 2F, bullpen, basement armory, holding cells, 6-lane range, parking) |
| `hospital/` | 1.1 GB | `hospital_a.blend/.glb` (dressed, 164 MB glb, sunset + rain). `hospital_b.blend/.glb` (greybox). `buildings*.blend`, `vehicles.blend`, `props_extra*.blend` (asset libraries). `tex/` (mattress, papers, police tape, chainlink, tower windows). `decals/` (posters, graffiti, leaks, street atlases; `acg/` ambientCG decals). `ph/` (Poly Haven props/textures incl. `disp` height maps) |
| `characters/swat/` | (characters 3.8 GB total) | `swat.glb` (unarmed), `swat_armed.glb` (rifle + MM clips), `swat_arms.glb`, first-person rigs `swat_fp_kriss/m4/glock.glb`, `source/SWAT Operator Green.blend`, `anims/` (Mixamo unarmed FBX: idle, walk, run, strafes, turns, crouch), `anims_rifle/` (Mixamo rifle FBX: idle, aim, walk, run, strafes aiming, backward, turn) |
| `characters/tactical/` | | `tactical.glb`, `tactical_armed.glb`: Fab "Tactical Soldier Outfit for MetaHuman" body on the SWAT skeleton with the SWAT head (squad/police) |
| `characters/robber/` | | `robber.glb`, `robber_armed.glb`: Fab "Robb" masked robber (mercs) |
| `characters/mixamo_mm/` | | `raw/` Mixamo packs (Pro Rifle, Rifle 8-Way, Shooter, Basic Shooter, Pistol Handgun Locomotion, Lite Rifle, aiming/crouch/relaxed). `glb/` 61 converted clips (walk/run 8-way, starts/stops, turns 45/90/180, crouch 8-way, pistol set, idles). `work/` |
| `characters/gore/` | | `gore.blend/.glb` (blood decal meshes), `wound.fbx` + `Textures_wound.png` (stump caps), `blood.png`, `decal_*.png`, `blooddecals/` (Fab 4K blood decals) |
| `characters/*.blend` | | `characters.blend`, `candidates.blend` (character work files) |
| `weapons/` | 72 MB | `m4/` (`M4 Animations` FBX pack: M4 + arms + clips; `m4_held.glb`), `glock/` (`Glock 17` FBX pack; `glock_held.glb`), `scope1/` (`eotech.blend/.glb` holo from `scope1_lp.obj`; `scope4x.blend/.glb` procedural 4x scope) |
| `props/` | 698 MB | apartment props: locker-equipment, alarm-clock, small-tv-stand, vintage day bed, painted cabinet, `polyhaven/` trees |
| `sounds/_downloads/` | 557 MB | raw sound packs (zips: firearms, footsteps, impacts, 100 CC0 SFX v1/v2, Prepared_SFX_Library.7z) |
| `intro/` | | PhysX and FMOD logo PNGs |
| `scripts/` | | ~150 Blender/Python helpers. Key ones listed below; many others are one-off probes/renders |

**Other archive folders**

- `Data/sounds/`: the processed game sounds (ogg).
  - `ambience/`, `footsteps/<surface>_1-9`, `casings/`, `magazines/`
  - `impacts/` (concrete, flesh, glass_break, headburst, metal)
  - `player/` (breath, heartbeat, kick, vault)
  - `tails/`, `weapons/` (m4/glock/v45/merc shots near+far, v45 reload parts, dryfire,
    firemode_switch, flashlight_click), `intro/`
  - `CREDITS.txt`
- `Data/dialogue/*.txt` (2.10). `Data/intro/*.rgba` (raw logo pixels).

**Key scripts** (`Content/source/scripts/`)

| Script | Purpose |
|---|---|
| `mm_fbx_to_glb.py` | Mixamo FBX → glb (Blender headless) |
| `mm_retarget.py` | Retargets Mixamo clips onto the SWAT skeleton by world-rotation deltas from the T-pose, scales hips by hip height, keeps root motion, 30 fps |
| `chars/arm_clearance.py` | Moves hands + rifle out of bulky vests (see 5.2) |
| `chars/fit_char.py` → `finish_char.py` → `merge_glb.py` | Fit a new outfit onto the SWAT skeleton; keep the skeleton and clips byte-identical |
| `fp_weapon.py`, `fp_reload.py`, `kriss_sockets.py`, `optic_rail.py`, `scope4x_build.py`, `eotech_prep.py` | First-person weapons, sockets, optics |
| `hosp_a.py`, `hosp_a_outside.py`, `hosp_b.py`, `hosp_b2.py`, `dockwell.py` | Procedural hospital greybox + markers (b2 = current part B) |
| `deco_*.py`, `furn.py`, `detail.py`, `place_decals.py`, `gen_decals.py` | Hospital dressing |
| `hosp_tubes.py` | Ceiling troffer lights → 1.1 m `_Tube` lights |
| `hosp_a_screens_drips.py` | Screen/drip markers |
| `split_curtains.py` | `Cloth_Curtain_NN` panels |
| `weather_marker.py`, `sunset.py` | Look/weather markers |
| `zfix*.py`, `zvert*.py`, `uvfix.py`, `fix_mapping.py`, `fixscale.py` | Content fixes (see 5.2) |
| `ph_fetch.py` | Downloads Poly Haven displacement maps |
| `make_*_sounds.py`, `synth_headburst.py` | Synthesised sounds (kick, vault, intro, weapon fx, loadout shots) |

### 3.1 Characters and animation

**Skeleton and bodies**

- All characters share the **SWAT skeleton** (Mixamo naming: Hips, Spine, Spine1, Spine2, Neck,
  Head, LeftShoulder/Arm/ForeArm/Hand, LeftUpLeg/Leg/Foot/ToeBase...) and the same clips.
- The model name stays "SWAT".

| Body | Used for |
|---|---|
| SWAT operator | Players (placeholder) and civilians/survivors/prisoners |
| Tactical | Squad/police; alternates with SWAT by marker number, overridden by `_Swat`/`_Tactical` |
| Robber | Mercs |

**Head gear** (material names `HeadGear_NVG`, `HeadGear_Helmet`, `HeadGear_Headset` hidden per
look)

| Look | Notes |
|---|---|
| Full | Player1 |
| NoNVG | Player2 |
| Balaclava | Default for civilians/prisoners/survivors |

- Squad mix by number: [Full, NoNvg, Full, Balaclava, NoNvg]. Merc mix: [Balaclava, Balaclava,
  NoNvg, Balaclava].
- Marker overrides: `_Full`, `_NoNVG`, `_Balaclava`.

**Clips**

- Unarmed/rifle base set: Idle, Walk, Run, strafes, turns, CrouchIdle, CrouchWalk, RifleIdle,
  AimIdle, RifleCrouch*/AimCrouch*.
- Motion-matching sets: `MM_Rifle_*`, `MM_Aim_*`, `MM_Crouch_*`, `MM_Pistol_*` (61 clips).
- Animation is **presentation only**, derived from replicated movement.

**Old locomotion stack**

- inertialization
- motion matching: 30 Hz database, 27 features (feet positions/velocities, hip velocity,
  3 trajectory points). Weights: foot position 0.75, foot velocity 0.6, hip velocity 1.5,
  trajectory 1.5/1.5.
- stride/orientation warping, foot locking, lean
- foot IK with pelvis drop (probe 0.55 up / 0.6 down, max foot raise 0.45, max pelvis drop 0.45)
- root-yaw removal

**UE mapping**

- Retarget once with IK Rig + IK Retargeter onto the UE Mannequin (or keep the SWAT skeleton as
  the project skeleton).
- **Motion Matching** with Pose Search: Pose Search Database per stance (Rifle/Aim/Crouch/Pistol)
  from the Mixamo clips (export **with root motion, "In Place" off**), Chooser tables, Character
  Trajectory component.
- Inertialization node, Orientation/Stride Warping (Animation Warping plugin), foot IK via Control
  Rig.
- The Game Animation Sample project is the reference setup.
- First-person arms + weapon clips (FPAim, ReloadTactical/Empty, Draw, Holster) as a separate
  skeletal mesh.

### 3.2 Level authoring conventions

Blender object names drive gameplay. glTF extras/custom properties were **not** used, so
everything is names and materials. In UE, re-express each one as an actor/component.
Import the .blend levels via glTF/FBX, or better, Datasmith/USD, then run an **editor utility**
(Python or C++) that converts named empties into actors.

**Spawns and markers**

| Name | Meaning | UE equivalent |
|---|---|---|
| `PlayerStart_1/2` | player spawns (else ±1 m around `PlayerStart`/`Spawn`) | `APlayerStart` with PlayerStartTag "1"/"2" |
| `CarSpot` | car / level-exit trigger (both players within 3 m) | trigger volume + level-flow actor |
| `Beat_<Name>` | hospital beat: trigger (4 m horiz / 2 m vert), checkpoint, merc-floor spawn; `Beat_GroundFloor` switches the objective | `ABeatTrigger` (box) with checkpoint transforms |
| `Spawn_Merc_<G\|F2\|F3\|Annex\|B\|R\|AG>_N` | merc spawn (floor from the name or height/4 m; faces node −Z) | `AMercSpawnPoint` with group/floor property |
| `Npc_Squad_<n>` | armed squad AI spawn | `ASquadSpawnPoint` |
| `Npc_<Role>[_N][_Idle\|_Crouch\|_Armed\|_Aim][_Full\|_NoNVG\|_Balaclava][_Swat\|_Tactical]` | client-only placeholder person (faces Blender +Y) | `AAmbientNpc` with role/pose/gear enums |
| `Body_N`, `Horror_*`, `Chair_P1/P2`, `Gas_Vent_N`, `Spawn_Monster_N`, `Lab_PowerCut`, `Lab_TitleCard`, `Zone_Combat_*` | authored in the hospital scripts, **not used by code yet** (planned beats) | design-time actors |

**Interaction and physics**

| Name | Meaning | UE equivalent |
|---|---|---|
| `Interact_<Name>` | usable object (F, 2 m); moving parts are children of the empty placed on the hinge | actor implementing `IInteractable`; door = hinge component (timeline) |
| `Interact_Locker_1/2`, `Interact_Gear_1/2`, `Interact_FrontDoor`, `Interact_StreetDoor`, `Interact_AlarmClock`, `Interact_CoffeeMachine`, `Mug_A/B`, `Interact_Radio`, `Interact_Mirror`, `Interact_WallClock`, `Interact_Briefing`, `Interact_ArmoryArmor/Weapon/Ammo`, `Interact_ServiceLift` | specific interactables (2.5) | subclasses / data-driven |
| `Toy_*` ("ball" = throwable), `Court*` | physics toys, basketball | simulated StaticMeshActor + pickup component |
| `Target_*` | range pop-up targets (≤ 8) | `ARangeTarget` |
| `Blocker_*` | invisible collision boxes (block players, NPCs, rays; no blood) | Blocking Volume / invisible collision mesh |
| `Breakable_Glass*` | one pane each (origin at centre, thin box) | `ABreakableGlass` actor |
| `Cloth_*` | hanging cloth panel (origin at the top-edge centre); collider removed | Chaos Cloth skeletal mesh actor |
| `Decal_*` | thin decal meshes 2–5 mm off a surface, alpha blend | `ADecalActor` (DBuffer decals) or keep as mesh decals |

**Zones, look and weather**

| Name | Meaning | UE equivalent |
|---|---|---|
| `Zone_Interior_*` | cube empty covering an interior incl. walls (axis-aligned) | `AInteriorVolume` / Audio Gameplay Volume; Lumen does the lighting |
| `Zone_Safe_*` / `Zone_Hostile_*` | weapon lowered / raised (box −1..1) | `AWeaponStanceVolume` |
| `Level_Look_Day\|Evening\|Sunset\|Night` | time-of-day preset | World Settings / level DataAsset + PPV |
| `Level_Weather_Rain` | rainy level | level DataAsset `bRain` |

**Lights**

| Name | Meaning | UE equivalent |
|---|---|---|
| `_Shadow` | casts shadows | Cast Shadows on |
| `_Tube` | X scale = length | Source Length / Rect light |
| `_Downlight\|_WallWash\|_Grille\|_Fluorescent` | emission profile | IES profile |
| `_Flicker\|_FlickerHard\|_SirenRed\|_SirenBlue\|_Pulse` | animated (2.12) | `ULightAnimComponent` |
| `_NoBounce` | excluded from the GI bake | Indirect Lighting Intensity 0 |

**Screens and effects**

| Name | Meaning | UE equivalent |
|---|---|---|
| `Screen_*[_Flicker\|_Off\|_Scroll\|_Image\|_Panel][_Blue...]` | glowing display (faces −Y) | screen material + Rect light component |
| `Fx_Steam_*`, `Fx_Smoke_*`, `Fx_Dust_*` | emitters along Blender +Z | Niagara components |
| `Fx_Drip_*` | ceiling leak | Niagara + decal puddle |

**Material and shading rules**

- Glass: Principled Transmission > 0 or Alpha Blend with low alpha. An opaque dark "glass" stays
  a panel.
- Foliage: glTF MASK (Round node before Alpha).
- Shading by name:
  - skin: `face/skin/hand/head`, not `headgear`
  - fabric: `body/robber/tac_set/uniform/cloth`, `mattress/sofa/curtain/pillow/blanket/carpet/towel/trashbag`, not `frame`
  - UE: Subsurface Profile / Substrate, Cloth shading model
- Penetration and footstep surfaces by name keyword (2.3, 2.11). In UE: Physical Materials.
- Blender rule: every object belongs to exactly one collection (the same as its parent), or
  linked instancing duplicates it.

### 3.3 Licences and attribution duties (CREDITS.md, Data/sounds/CREDITS.txt)

**CC BY 4.0 (must be credited in the shipped game's credits)**

- ~33 Sketchfab models: beds, nightstands, kitchen, doors, coffee maker, fridge, pizza box,
  plate carrier, toilet, vanity, shower, coat rack, rug, Crown Victoria, chief's chair/desk/lamp/
  carpet, police caps, street buildings, police car, ambulance, semi-truck, armored truck,
  dumpster, vending machine, pallet, brick blocks, medical bed.
- The SWAT Operator (SpatialNeglect, Fab, CC BY 4.0; includes the KRISS model).
- Authors and URLs are in CREDITS.md. Copy that table into the UE credits screen.

**CC0 (no duty, listed for completeness)**

- Poly Haven models and textures, ambientCG decals.
- Sounds: Free Firearm Sound Library, Impacts/Gun/sfx, rubberduck 100 CC0 SFX #1/#2.

**CC BY 3.0**

- "Footsteps on different surfaces" (OpenGameArt). Credit the pack and its freesound.org authors
  per its licence files.

**Mixamo**

- Royalty-free, no attribution.

**To confirm before shipping (Fab / user-provided)**

- Locker Equipment, Small TV stand (zips, licence unknown).
- Blood Decals (Weasel Whistle, "free for personal and commercial use").
- **Tactical Soldier Outfit for MetaHuman** (RMDavid, no licence stated). It is a native UE 5.6+
  asset: re-download it from Fab into the UE project instead of using the decimated glb.
- **Robb** (Eemanuel3Dl, no licence stated).
- Glock 17 and M4 Animations (Fab listings 5b920af4… / 38da8f1e…; which is which and the licence
  are to confirm). Fab assets get Fab's standard licence; check whether they include UE-native
  versions.
- Holographic sight `scope1_lp.obj` (.ST3D, "sharing for free", no licence).

**Other**

- In-house: 4x scope, wound stump, blood splat, synthesised sounds (siren, heartbeat, buzz, rain,
  thunder, glass, casings, tails, kick, vault, intro).
- **[? discrepancy]** CREDITS.md says gunshots come from the Free Firearm Sound Library (CC0).
  Data/sounds/CREDITS.txt says "gunshot.mp3 supplied by the project owner". Ask the user.
- **Fonts:** Noto Sans and Cousine (SIL OFL 1.1).
- **FMOD** (only if used in UE): free indie tier, register the project before release. Credits
  must show "Made with FMOD Studio by Firelight Technologies Pty Ltd." and the FMOD logo.
- **PhysX/Vela** logos: drop them; they are no longer used.
- **Process rule:** every new third-party asset gets a line in CREDITS.md when added.

---

## 4. UE5 mapping

Target: **UE 5.6 or 5.7** (Motion Matching and Chooser are mature there; the MetaHuman outfit
needs 5.6+). C++ project with Blueprint for content glue.

| System | Recommended UE5 feature | Notes |
|---|---|---|
| Rendering | Lumen GI + reflections (software ray tracing), Nanite for static level meshes, Virtual Shadow Maps, **MegaLights** for many local lights (experimental/beta by version: test), TSR | Replaces the whole custom renderer (irradiance bake, SSR, GTAO, PCSS, shadow atlas, TAA). On the RTX 3050 4 GB, target the Medium/High scalability equivalent. Lumen at "High" is the main cost |
| Post / look | Post Process Volumes + per-look DataAsset, Sky Atmosphere, Volumetric Clouds, Exponential Height Fog (volumetric) | Grade as in 2.12. Bokeh DoF in bullet-time via PPV weight |
| Decals | DBuffer deferred decals (bullet holes, blood), decal fade/lifetime + budget manager | Blood relief via normal maps; bullet hole budget by quality |
| Glass | Translucent material (Substrate or Thin Translucent) + breakable actor | |
| Physics | **Chaos** (rigid bodies, ragdolls via Physics Asset, physical animation for hit reactions) | Ragdoll + bullet impulses; Physical Animation Component for flinches is an option |
| Destruction | Chaos Geometry Collections only if wanted for glass/props; simple hide+Niagara is enough | |
| Cloth | Chaos Cloth | Curtains near the player only |
| Characters | Skeletal meshes, **IK Rig + IK Retargeter**, **Control Rig** (foot IK, hand IK on weapons, procedural kick leg), **Motion Matching (Pose Search)** + Chooser + Inertialization + Animation Warping | Use the Game Animation Sample as the template |
| First person | 5.5+ First Person rendering (FOV/scale per primitive) or a separate FOV via material/WPO; owner-only meshes | Legs-only body + full shadow body |
| VFX | **Niagara** (muzzle, tracers, impacts, blood, rain, dust, drips, glass shards, casings could be Niagara mesh particles with collision) | Casings/mags as Niagara with collision events → sounds |
| Audio | **MetaSounds**, Submixes, SoundClass/SoundMix (bullet-time pitch + LPF), Audio Gameplay Volumes (indoor), attenuation presets; or the **FMOD for Unreal plugin** | See 2.11 |
| Input | **Enhanced Input** (Input Actions + Mapping Contexts; player-mappable keys) | Hold/toggle crouch, hold-to-revive (Hold trigger 3 s) |
| UI | **CommonUI + UMG** | Menus, lobby, options, HUD, subtitles |
| Abilities | Gameplay Ability System **optional** | Plain components are simpler for 2 players |
| Networking | Listen server, replication, **CharacterMovementComponent** subclass with custom moves (slide/vault/mantle/lean), server RPC shots + custom lag compensation, Replication Graph/Iris not needed | 2.14 |
| AI | AIController + **StateTree** (or Behavior Tree), **AI Perception**, **EQS** (cover), NavMesh, Smart Objects (optional), Crowd avoidance (Detour) | 2.7 numbers |
| World | **Level Instances** / sub-levels per area; **World Partition** only for the outdoor district if it grows; seamless travel between apartment → station → hospital A → B | Service-lift ride = `ServerTravel` (seamless) with a fade |
| Intro / cutscenes | **Sequencer** (Level Sequences), Movie Render Queue for trailers | Chair-room reveal, gas, title card |
| Data | DataTables/DataAssets for weapons, NPC settings, looks, dialogue | Keep all numbers in assets, not code |
| Saves | SaveGame (settings, checkpoint, intro-seen) + GameUserSettings | |

**Don't port**

- Vela and the custom renderer internals: render graph, shaders, shadow cache, irradiance bake,
  SSR/GTAO/fog/TAA, the G-buffer layout, lens-flare passes, light-tile budgets.
- The cooker, pak format, VFS, cooked mesh/texture formats, BC compression rules (UE imports and
  cooks).
- The custom net transport/snapshot code, the time-domain engine plumbing, and the ImGui dev tools
  (use UE console commands, cheat manager, editor utility widgets).
- The legacy melee "Enemy" and the test arena.
- The texture-size capping rules (use UE texture groups / LOD bias instead).

**Do port** (as data or reference): all tuning numbers, dev drills as console commands
(`--dev-fire-drill`, `--dev-mercs`, `--dev-start-beat <name>`, `--dev-weapon`, …), and the
authoring conventions as editor tooling.

---

## 5. Hard-won lessons

### 5.1 Performance (old engine; the principles carry over)

- **Pixel cost dominates** on the 3050 at 1080p.
  - Double-sided character materials cost about **1.3 ms in close-ups**. Cook character bodies
    **single-sided** (back-face culled). Keep two-sided only for first-person models, weapons and
    gore.
  - In UE, untick Two Sided on character materials exported from Blender: the Blender glTF export
    marks everything double-sided.
- **Shadows were the biggest scene cost.** What helped:
  - caching static shadow tiles
  - instancing static casters
  - skipping tiny casters
  - limiting cascades for characters
  - culling sun casters
  - limiting shadowed local lights to the nearest 2–4
  - In UE: VSM caching handles much of this. Still cap shadow-casting local lights, mark small
    props "Cast Shadow" off, and use shadow-casting distance.
- Per-pixel local light masks and light distance fades (12 m with a 3 m fade) kept many lights
  cheap.
- Fog and SSR at half resolution; scope view at 256–512 px.
- Texture memory: cap texture size by object size (~1024 texels per metre, max 2048). The low
  preset dropped the top 2 mips. VRAM is 4 GB: watch the UE texture streaming pool.
- **Menus and intro capped at 60 fps.** Uncapped dev builds ran at 1000+ fps, pinned the GPU and
  froze the laptop. Always vsync / `t.MaxFPS`.
- Graphics presets existed (Low/Medium/High/Ultra). Low = 960x540, 0.75 render scale, FXAA.
  Mirror this with UE Scalability groups and a custom options screen.

### 5.2 Content gotchas

- **Coplanar faces / z-fighting in Blender:** layered ground slabs (grass, road, sidewalk) and
  stacked boxes flickered.
  - Fix: separate heights (grass −0.2, roads −0.12, sidewalks 0), or cut holes instead of
    overlapping.
  - Decals 2–5 mm off the surface.
  - `zfix*.py` were one-off vertex nudges; check imports for z-fighting.
- **UV layers when merging meshes:** merged objects can end up with several UV maps where the
  material uses a non-active or non-render one, so the textures come out wrong. Make sure the
  render UV layer is the one the material uses before export (`uvfix.py`).
- **glTF can't express** non-identity Mapping nodes (texture offset/rotate/scale), vertex-colour
  mixes or other shader-node tricks. Bake them into UVs or split materials (`fix_mapping.py`).
  Check every imported material in UE.
- **Foliage:** export alpha as MASK (Round node). Use alpha-coverage-preserving mips.
- **Mixamo for vests:** Mixamo rifle clips are for slim bodies, so hands and forearms sink 5–7 cm
  into plate carriers. `arm_clearance.py` moves the rifle and hands as one rigid piece away from
  the chest and re-solves two-bone IK. In UE, do the same with a Control Rig/IK offset layer or by
  editing clips.
- **Mixamo for motion matching:** download with **"In Place" OFF** (root motion is needed for
  trajectories). Both rigs must rest in the same T-pose for retargeting. Scale hips by the
  hip-height ratio. Sample at 30 fps.
- **Scale checks:** imported packs arrive at arbitrary scale (Sketchfab/Fab). Always check real
  dimensions:
  - door ~2.0–2.1 m
  - character ~1.8 m
  - storey 4 m in the hospital
  - weapons against the hands
  - `fixscale.py`/`meas.py` existed for this
  - UE is centimetres: confirm the import scale once
- **Characters:** keep material names stable (head-gear hiding uses them).
  - Cook NPC bodies with limb regions + wound caps, or dismemberment silently turns off.
  - Keep the skeleton and clips identical across bodies.
- **Blender collections:** every object in exactly one collection (the same as its parent), else
  linked instancing duplicates it.
- **Lights:** recheck the light layout after moving geometry (the old GI bake needed a recook).
- **Content scripts** go in `Content/source/scripts/`, never only in /tmp (tmpfs was wiped once
  on a crash).
- Don't rerun `level_fix2.py`, which predates the curtain split.

### 5.3 Workflow rules for agents on the user's laptop (mandatory)

- **Hardware:** RTX 3050 Laptop 4 GB VRAM, 22 GB RAM, 12 threads, Linux (Wayland). GPU memory
  exhaustion and parallel builds have **frozen and crashed the machine several times**.
- **Test runs:**
  - Always the lowest settings: UE editor scalability Low/Medium, `sg.*` low, small PIE window.
  - Always vsync or a frame cap (`t.MaxFPS 60`); never uncapped, except one short single run for a
    perf measurement.
  - Always under a timeout; never leave instances running.
  - Several instances are OK only if all are low/capped. Prefer PIE with 2 players in one process
    over two standalone processes.
- **Builds:**
  - Serialized and capped. One build at a time through a lock, e.g.
    `flock /tmp/ue-build.lock <UBT command>`.
  - Limit parallelism: `BuildConfiguration.xml` `<MaxParallelActions>4</MaxParallelActions>`.
  - Limit shader compile workers (`[DevOptions.Shaders] NumUnusedShaderCompilingThreads` / a low
    worker count in Engine.ini).
  - Don't run several agents that compile at once (at most ~3 ever).
- **Kill processes** by `pgrep -x` PID, not `pkill -f` (it matched the shell's own command
  line).
- **Frame captures:** desktop screenshot tools don't work on this Wayland desktop. Use in-engine
  captures: `HighResShot`, or Movie Render Queue for stills.
- **Commits:** only when the user asks; branch first if on main.
- **Warnings:** builds should have no warnings in project code. Treat validation/ensure messages
  as bugs.

### 5.4 User preferences

- Realistic look (Trepang2/Underhell). **No body-cam/VHS gameplay look.** Grounded, not comic.
- **Lean token use:** keep agent prompts, reports and multi-agent workflows small. Ask before
  adding review passes or report sections. Short reports.
- No real brands/places/people in fiction. Short dialogue lines (< ~20 words spoken, objectives
  < 8 words).
- Many story names and decisions are **OPEN**. Ask the user rather than deciding.
- Old engine conventions the user liked (optional in UE):
  - nothing throws, errors as results
  - comments in headers only
  - braces on their own line
  - `m_`/`k_` prefixes
  - files under ~500 lines, split by responsibility
  - UE has its own conventions (`F`/`U`/`A` prefixes, `b` bools). **Ask the user** which style
    to use, and follow the Epic coding standard by default.

---

## 6. Suggested UE5 project structure

```
LockstepRAW/                       (UE project, C++)
  Source/Lockstep/
    Core/          GameInstance, settings, save, console/dev commands, logging
    Player/        ALockstepCharacter, ULockstepMovementComponent (slide/vault/mantle/lean/kick),
                   APlayerControllerBase, health/downed/revive component, head-motion camera manager
    Weapons/       AWeapon + UWeaponData (DataAsset per weapon), fire/recoil/spread/sway components,
                   hitscan + penetration, lag compensation subsystem, optics, flashlight, brass/mags
    AI/            AMercController/ASquadController, StateTree tasks, perception config, cover EQS,
                   squad follow/watch, callouts
    Gore/          dismemberment component, blood decal manager (budget), head bursts, stains
    World/         beats/checkpoints, interactables, doors, lockers, breakable glass, level flow,
                   bullet-time subsystem (replicated), interior/weapon-stance volumes, light animation,
                   weather subsystem, screens, drips
    Audio/         surface footsteps, combat audio, ambience mixer (indoor weight), bullet-time mix
    Dialogue/      subtitle subsystem, DataTable rows, barks
    UI/            CommonUI screens (main, host/join, lobby, options, credits, pause), HUD widgets
    Net/           lobby/session (direct IP), replicated state structs
  Content/
    Characters/ (SWAT, Tactical, Robber, FP arms)  Animations/ (Mixamo MM databases, FP clips)
    Weapons/  Levels/ (Apartment, District, Station, HospitalA, HospitalB)  Props/  Materials/
    VFX/ (Niagara)  Audio/  UI/  Data/ (DataTables, DataAssets: weapons, NPC settings, looks, dialogue)
    Developers/  (scratch)
  Tools/EditorScripts/  (Python: convert Blender marker names → actors, credits import)
  Docs/  (this handoff, CREDITS.md)
```

## 7. First-milestone plan ("vertical slice: Hospital A ground floor, two players")

Do these in order; each step should be playable before the next.

1. **Project setup:**
   - UE 5.6/5.7 C++ project, source control (git LFS or Perforce for binaries)
   - project CLAUDE.md with the rules from 5.3 and 5.4
   - BuildConfiguration caps and editor scalability defaults
2. **Character and movement:**
   - CMC subclass with the 2.1 numbers: walk/run/back/strafe, accel/decel, jump/coyote/buffer,
     crouch, slide, lean, mantle, vault (vault/mantle last)
   - camera head motion and weight (2.2)
   - test in 2-player PIE with network emulation
3. **Bullet-time prototype early** (the biggest risk): replicated global dilation 0.2 with player
   compensation, CMC prediction checks, the audio mix. Decide this approach before building more
   on it.
4. **Weapons:**
   - KRISS/V-45 first, from a DataAsset: fire modes, recoil, spread, hitscan server RPC with the
     deterministic spread, reload with AnimNotify cues, sway/breath, ADS sight line, flashlight
   - then M4 + holo and Glock; switching
   - penetration with Physical Materials
5. **Import Hospital A:**
   - from `hospital_a.blend` (glTF/FBX or Datasmith), with the scale and material checklist (5.2)
   - editor script that turns markers into actors (3.2)
   - sunset + rain look; Lumen/VSM on low-end settings
6. **Health:** downed/revive/checkpoint (2.4) and the minimal HUD (2.13).
7. **AI:**
   - mercs on the ground floor: perception, cover, peek, burst fire, callouts (2.7)
   - squad follow
   - beats spawn floor groups
8. **Gore and feedback:**
   - blood decals with a budget, hit reactions (physical animation), ragdolls, dismemberment
     (head first)
   - breakable glass, bullet holes, impacts, footsteps, gunshots + tails
9. **Shell:** main menu, host/join by IP, lobby with ready/start, options, credits. Intro last.

**After the milestone:**

- Station and morning levels.
- Hospital B content: garage, squad separation, annex, sewers, lab.
- The chair sequence (gas, restraints, power cut, automatic bullet-time, "monsters" = squad with
  distorted name VO).
- Story bullet-time rules and the shared meter.
- Motion-matched third-person locomotion.
- Weather polish.

**Open questions to raise with the user early:**

- Real weapon names vs the fictional rule.
- Headshot multipliers (none in old code).
- Mercs on by default (opt-in in old code).
- MetaSounds vs FMOD.
- UE coding style.
- The gunshot sound licence discrepancy.
- Licences of the Fab "to confirm" assets.
- Gas trigger, character names, mission title (story OPEN items).
- Interact/flashlight key conflict.
