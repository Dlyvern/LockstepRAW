# LockstepRAW

Two-player online co-op FPS (horror + bullet-time) on a custom C++20 engine.
Rendering is done by Vela (`external/Vela`), a Vulkan render-graph library by the same author.

## Layout

```
external/   third-party submodules (Vela, imgui). Never edit files here, except the
            vela-maintainer agent, which may change external/Vela under its own rules.
Engine/     reusable engine code. Knows nothing about this game's story or rules.
Game/       the game. Built on Engine.
.claude/agents/   subagent definitions
```

Inside Engine, code is grouped by module:

```
Engine/include/Lockstep/<Module>/Name.hpp   public headers
Engine/src/<Module>/Name.cpp                implementation
Engine/src/<Module>/Name.hpp                private headers (not visible to Game), named by
                                            responsibility (RendererState.hpp, VelaConversions.hpp)
```

Modules: `Core` (Result, math, Transform, FixedTimestep), `Platform` (window, events, input),
`Render` (Vela wrapper), `World` (time domains, entities), `Net` (GameNetworkingSockets
transport, session, host-authoritative snapshot replication), `Debug` (ImGui dev tools), `App`
(run loop and the `GameApi` boundary), `Physics` (PhysX: `PhysicsWorld`, bodies, raycasts,
character controllers), `Assets` (VFS, paks, glTF import, cooked formats), `Anim` (skeletons, clips,
`Animator` blending, `SkinnedModel`/`SkinnedInstance`; GPU skinning in Render), `Audio` (FMOD Core:
buses, 3D voices, listener), `UI` (immediate-mode screen UI: `UiContext` widgets drawn through the
Renderer's overlay pass; Game's menus live in `Game/Shell/`, the startup intro in `Game/Intro/`).
Create a module folder only when there is code for it.

## Architecture rules

1. **Dependency direction: Game → Engine → external.** Engine never includes a Game header.
   Game should not include Vela or other external headers directly; it goes through Engine.
2. **Engine provides mechanisms, Game provides meaning.** Example: Engine has time domains with
   per-domain scale; Game decides that "bullet-time" sets the World domain to 0.2.
   If a feature only makes sense for this story, it belongs in Game.
3. **Errors:** nothing throws. Anything that can fail is a static `create()` returning
   `Result<T>`, or a function returning `Status`, the same model as Vela. Never ignore a
   returned `Result`/`Status`.
4. **Ownership:** RAII and `std::unique_ptr`. No raw `new`/`delete` outside factories. Objects
   holding GPU resources are destroyed before the Vela `Context`, and only after `waitIdle()`.
5. **Simulation runs at a fixed tick** (60 Hz); rendering interpolates. Simulation code takes a
   delta time and never reads the wall clock itself.
6. **No globals** for game state. Game state lives behind one pointer the Engine passes back in,
   so Game can later become a hot-reloadable shared library.
7. **Client-only vs replicated** is a first-class concept (hallucinations and local NPC
   dialogue never replicate). Keep that in mind when designing World and Net APIs.

## Decisions made

- **Math:** vectors, quaternions, matrices and their operations come from Vela's math library.
  `Lockstep/Core/Math.hpp` re-exports them as `lockstep::math::Vec3`, `Quat`, `Mat4`, `dot`,
  `slerp`, ... Never write a second vector or quaternion type and never convert between Engine
  and Vela math. Engine-only additions (`Transform`, interpolation, later things like
  rays and bounds) go in their own headers in `Lockstep/Core`, in namespace `lockstep::math`.
  Vela's math is the one Vela part Game may see, and only through `Lockstep/Core/Math.hpp`.
- **Resources:** Game holds handles (`MeshHandle`, ...), owned by the Renderer. Handles will get
  a generation counter so stale handles are caught, and destruction is deferred until the GPU
  no longer uses the resource (frames in flight).
- **Input:** owned by `Platform`. Game reads an `Input` snapshot from `TickInfo` (simulation)
  or `FrameInfo` (presentation). The run loop hides keys/mouse from Game while ImGui uses them.
  Anything the host must check (movement, look, fire) is read from `TickInfo.input`.
- **Game boundary:** `main()` calls `lockstep::run()` with a `GameApi` table of plain function
  pointers; Game state lives in `GameState`, created by `init`. Player time domain is locked
  at scale 1.
- **Render pipeline:** Engine builds its own graph instead of Vela's `forwardGraph`: shadow map
  -> lit HDR scene (RGBA16F) -> tonemap -> overlays (dev tools draw after tonemapping). Default
  material is lit (Lambert + Blinn-Phong + ambient); unlit stays for HUD elements. Game submits
  lights every frame via `Renderer::submitLight`. Anti-aliasing modes Off / FXAA / MSAA 2x/4x /
  TAA; default TAA (jitter, per-pixel velocity, variance clipping, sharpening). Planar mirrors:
  Game registers reflector instances (`Renderer::submitReflector`); render layers
  (`Lockstep/Render/Views.hpp`) control what each view sees, e.g. the local pawn is drawn only in
  mirrors (`k_mirrorOnlyLayer`); `Camera::shadowLayers` picks shadow casters independently (Game:
  default + mirror-only full body cast, the camera-only first-person body does not). First-person arms/weapons use `k_viewModelLayer` (bit 31, Engine
  reserved; Game layers are bits 1–30): camera view only, own FOV (`Camera::viewModelFov`), depth
  squeezed so it never clips into walls, no shadow casting. The lit pass also writes a small
  G-buffer (`surfaceNormal`: oct view normal, roughness, sun share of colour; `surfaceSpecular`:
  environment specular inside ambientLit, signed specular weight; layout in
  `shaders/surface_buffer.glsl`), read right after it by the indirect lighting passes
  (`IndirectLighting.cpp`, `ScreenSpaceReflections.cpp`): half-res GTAO + sun contact shadows
  (`AmbientOcclusionSettings`, `ContactShadowSettings`), half-res SSR (GGX-jittered linear
  raymarch with bisection, hits fetch last frame's half-res lit scene `ssrSource` reprojected,
  roughness-based neighbour reuse, temporal accumulation; `ScreenSpaceReflectionSettings`), then a
  full-res composite that rebuilds `ambient` from `ambientLit` (multi-bounce AO, SSR over the
  environment specular, specular occlusion; divided by the AO visibility every later reader still
  multiplies in) and darkens colour by the contact shadows. Glass keeps environment reflections.
  Volumetric fog (`VolumetricFogSettings`) runs after that: half-res jittered raymarch (sun cascades + local light tiles shape
  the shafts, analytic height-fog transmittance; per-light `volumetric` factor), temporal blend,
  depth-aware upsample alpha-blended into colour and ambient. The Engine-internal `RenderPass`
  interface is how Engine passes plug in; Game can't add passes yet. Post chain: auto exposure
  (64x64 log-luminance grid -> 8x8 -> persistent 1x1 EV; carried to tonemapping in bloomUp1's
  alpha because tonemap has no free input slot), ACES, colour grading + vignette in the tonemap
  pass (+ procedural lens dirt lit by bloom), TAA, then the output pass: FXAA/TAA sharpening,
  lens effects (depth of field, per-pixel motion blur from the velocity buffer with viewmodel
  pixels reduced and not smeared into the world, chromatic aberration; `LensPasses.cpp`,
  `shaders/lens_effects.glsl`), film grain and dither. Game drives DoF strength from bullet-time
  (`Game/Presentation/LensEffects`); `LevelLook` sets the lens defaults. DoF is bokeh-shaped
  (`DepthOfFieldSettings::apertureBlades`/`bokehHighlights`: polygon aperture, bright taps
  outweigh dim ones). Lens flares (`LensFlareSettings`, `LensFlare.cpp`, `shaders/lens_flare.frag`):
  quarter-res pass after the bloom down chain: ghosts + halo mirrored from pixels far above the
  exposure (bloomDown2; viewmodel depth masked), plus sun glare (veil + starburst, occluded by depth
  taps, sunset boost); added into bloomUp1 (`bloom_up_flare.frag`, its 4th input), so it needs bloom.
  Animated surfaces (`Lockstep/Render/SurfaceEffects.hpp`, `shaders/surface_effects.glsl`):
  `ShadingModel::Screen` materials get scanlines/rolling band/shimmer/scroll; `Renderer::submitDripPuddle`
  (8 nearest) makes a wet glossy puddle with drop ripple rings on upward floors. Shadow atlas 4096x2048:
  sun cascades left half, 16 local light tiles (512) right half. The atlas is persistent
  (`ShadowPass.cpp`, `ShadowCache.cpp`): a tile is re-rendered only when its key (tile matrix +
  every caster's mesh/material/model) changed or it holds a skinned caster; cascades 3-4 keep
  their matrix for a frame (staggered refresh) unless the camera turned/moved or the sun changed.
  Static casters are drawn instanced (one call per mesh + shadow material run per tile).
  Static casters smaller than `ShadowDetailSettings::minCasterTexels` (quality `shadowCasterTexels`)
  in a tile are skipped. Soft shadows (`SoftShadowSettings`, `shaders/soft_shadows.glsl`): PCSS
  blocker search on raw atlas depth (set 2 slot 2, so pbr.frag's BRDF LUT is material binding 21),
  penumbra from sun size / light radius, rotated Vogel-disk compare taps. Local light shapes
  (`LightShapeSettings`, `shaders/light_shapes.glsl`): `PointLight::tubeLength` line lights (exact
  unclipped line irradiance for diffuse, representative point + roughness widening for specular;
  shadow from the centre; fog and particles treat them as points) and `LightProfile` emission
  profiles (Engine-generated R8 atlas, material binding 16). Bounce light: `Renderer::setIrradianceVolume`
  takes the cooked probe grid (`Scene::irradiance`, from `--gi`; Game's `applyLevelAmbientZones` sets
  it); the CPU combines local + sun x sun radiance (only while the frame sun matches the baked one)
  + sky bounce x mean sky radiance into 3 RGBA16F L1 textures + sky visibility (bindings 17-20,
  slices along z stacked in y, re-uploaded only when sun/sky change); pbr.frag
  (`shaders/irradiance_volume.glsl`) adds it to ambient diffuse and specular, and inside the volume
  sky visibility replaces the zones' IBL dimming and only `fillKept` of the zone fill remains.
- **Particles:** `Lockstep/Render/Particles.hpp` `ParticleSystem` (CPU sim, Game-owned, budget
  20k): effects from a description (atlas frames, blend Alpha/Additive, Lit/Unlit, billboard or
  velocity-stretched, curves), emitters and `burst()`. Drawn by the `particles` graph pass after
  the lit scene and volumetric fog: soft against depth, alpha sorted back to front then additive,
  one instanced draw per texture run, lit per particle (sun + shadows, local lights, IBL, zones).
  Never in mirrors or shadow maps. Presentation only, like audio: spawn from `frame()`/confirmed
  events, simulate with the World-domain delta. Game effects in `Game/Effects/` (procedural
  sprite atlas; impacts come from `CombatAudio::impacts`/`shots`, no second ray).
- **Atmosphere** (`Lockstep/Render/AtmosphereSettings.hpp`; Game animates it in `Game/Weather/`): clouds
  (volumetric half-res raymarch through a slab with a tiling 2D noise, composited into sky pixels before
  indirect lighting; Low uses a flat noise layer; coverage greys the sky IBL), foliage wind (masked
  static draws outside interior zones sway in `pbr_vertex.glsl`/`wind.glsl`; shadows keep the rest pose),
  lightning (sky/cloud flash, optional bolt; while it outshines the sun it is the frame's directional
  light with shadows, the sky keeps `skySunDirection`), lens rain (`LensRain` CPU drop sim, Game-owned,
  drawn after the output pass; drops land only under open sky). Game: `Wind`, `Lightning` (thunder
  delayed by distance/343 s), `LensDrops`. Dev: Weather and Graphics sections, `--dev-lightning-drill`.
- **Characters:** skinned glTF (Mixamo skeleton) cooked into `.model` + skinned `.mesh` (level
  format v3). Animation is presentation-only, derived from replicated movement, never replicated.
  Source in `Content/source/characters/<name>/` (Mixamo FBX clips retargeted in Blender onto the
  model; arm offsets keep hands out of bulky gear). Skinned parts keep their material names
  (`SkinnedModelPart::materialName`, stored in `.model` files); `SkinnedInstance::submit` takes a
  `PartMask` to hide parts (head gear materials `HeadGear_NVG/_Helmet/_Headset`). The current player model is a placeholder (SWAT
  operator, `Game/Characters/PawnCharacters.hpp` names scenes and clips in one place). Run with
  `--content Content/cooked/characters.pak`. Cook `swat_armed` (the NPC body) with `--limbs`:
  without it the model has no limb regions or wound caps and dismemberment silently turns off.
  NPC bodies (`NpcBody`, `Game/Characters/NpcBodies.hpp`): squad/police alternate SWAT and
  `tactical` by marker number (`_Swat`/`_Tactical` override), mercs use `robber_armed`; all keep
  the SWAT skeleton, clips and model name "SWAT". They are built by
  `Content/source/scripts/chars/` (`fit_char.py` -> `finish_char.py` -> `merge_glb.py`, which keeps
  swat.glb's skeleton and clips byte-identical); cook them with `--limbs --wound Content/source/characters/gore/gore.glb --single-sided` too
  (`--single-sided` cooks opaque materials back-face culled whatever glTF says; the Blender export marks
  them double-sided, which cost ~1.3 ms at 1080p in close-ups; first-person models, weapons and gore
  stay double-sided).
- **Locomotion layers / motion matching** (Engine/Anim, presentation only): inertialization
  (`PoseInertializer`, `Animator::setInertialization`), model-space joint edits and two-bone IK
  (`JointEdits`), `LocomotionPose` (orientation/stride warping, foot locking, lean, foot IK with
  static-only ground rays and pelvis drop; prepare -> probe -> finish), root-yaw removal (`RootMotion`),
  motion matching (`MotionDatabase` built at load from `MM_<Rifle|Aim|Crouch|Pistol>_*` clips,
  `updateMotionMatching`, `predictTrajectory`). Game: `Characters/LocomotionSignals`, `CharacterMotion`,
  `MotionSets`, `MotionMatchedLocomotion`; dev panel Pawn characters > Locomotion layers. Data: Mixamo
  packs in `Content/source/characters/mixamo_mm/raw/` -> `scripts/mm_fbx_to_glb.py` (Blender) ->
  `scripts/mm_retarget.py <body_armed.glb> <out> mixamo_mm/glb` -> `chars/arm_clearance.py --only MM_`
  for swat/tactical/robber_armed, then recook with `--limbs --wound ... --single-sided`. Dev:
  `--dev-anim-drill x,y,z,yaw[,spacing]`, `--dev-locomotion-off`, `--dev-motion-matching-off`.
- **Audio: FMOD Core 2.03.08** in `external/fmod` (as in Velix; SDK files git-ignored, only our
  CMakeLists tracked; `LOCKSTEP_WITH_FMOD` defaults ON when the SDK exists, else a silent backend).
  FMOD is private to `Engine/src/Audio`. The World bus follows the World time-domain scale (pitch +
  low-pass in bullet-time); the Player bus stays at 1. Play sounds from `frame()` or confirmed
  events, never from `fixedTick` (client re-simulation would replay them). Sounds load through
  the VFS.
- **Materials and assets:** art direction is realistic (Trepang2-like). The material model is
  glTF 2.0 metallic-roughness, defined in `Lockstep/Render/PbrMaterial.hpp` (`PbrVertex` with
  tangents, `PbrMaterialDescription`). That header is the contract: Engine/Assets (glTF import)
  fills it, Engine/Render implements it. Test content lives outside the repo (Sponza, Bistro in
  `~/Downloads`) and is loaded with `--scene <path>`. Imported levels are walkable (PhysX
  triangle-mesh collision). Two Engine extensions on top of glTF, filled only by the cooker:
  a height map (`height`/`heightScale`, parallax occlusion mapping in pbr.frag with distance
  fade and relief self-shadowing; `shaders/surface_detail.glsl`) and a micro-detail layer
  (`SurfaceDetail` Mineral/Paint/Metal/Fibrous: Engine-generated procedural tiling normal +
  roughness + albedo variation, faded in near the camera). Toggles in `SurfaceDetailSettings`.
  Cooker rules (`Tools/Cooker/SurfaceRules`): heights come from `--height-dir <dir>` next to the
  normal map (Poly Haven `<id>_nor[_gl]_<res>` -> `<dir>/<id>/<id>_disp_<res>.jpg|png`, or
  `<name>_normal` -> `<dir>/<name>_height.*`), stretched to 0-1, BC4, depth in metres by material
  name; detail layer by material/texture name words; both scaled by measured metres per UV.
  Shading models (`MaterialShading` in PbrMaterial.hpp, not cooked; Game sets them after loading
  with `Renderer::setMaterialShading`, rules in `Game/Presentation/MaterialLooks`): Skin
  (per-light wrapped, red-shifted diffuse with softened normals and back-lit glow, only on pixels
  whose albedo looks like skin; character material names `face`/`skin`/`hand`/`head` but not
  `headgear`), Fabric (Charlie sheen; character `body`/`robber`/`tac_set`/`uniform`/`cloth`, level
  nodes `mattress`/`sofa`/`curtain`/`pillow`/`blanket`/`carpet`/`towel`/`trashbag`... not `frame`),
  and decal relief (`decalRelief`: the decal's base colour alpha is a height field; it tilts the
  decal's normal, darkens the uncovered surface below where the relief faces away from the
  lights, and writes the decal's normal/roughness into the G-buffer; blood decals raised, bullet
  holes `Game/Effects/BulletHoles` dented; per surface look `BulletHoleLooks`: concrete, plaster,
  tile, wood, metal, glass star crack, fabric, from `shotMaterialOfBody` + `surfaceOfBody`; size by
  calibre; holes also from partner/NPC shots (WeaponFx adds one muzzle ray per remote shot to
  `CombatAudio::impacts`, `isRemote`); a hole is dropped when its body is gone (broken pane) or moved). Character surfaces (`Lockstep/Render/CharacterSurface.hpp`):
  per frame `Renderer::submitCharacterSurface` (soak, grime, up to 8 bind-pose blood/dirt stains,
  stored in the joint buffer) passed to `submitSkinned`/`SkinnedInstance::submit`; with a surface
  the world wetness is replaced by max(soak, outdoor rain). Game: `Characters/CharacterStains`,
  stains from gore events (`Gore/BodyStainEvents`; dev `--dev-bloody-npcs`). Code in
  `shaders/material_shading.glsl`; toggles `MaterialShadingSettings` + GraphicsQuality.
  `Content/source/scripts/ph_fetch.py` downloads `disp` maps. Hospital cook:
  `--height-dir Content/source/hospital/ph`.
- **Shipping content:** source assets (glTF from Blender or packs) are cooked offline by
  `Tools/Cooker` (`LockstepCooker`, built when `LOCKSTEP_BUILD_TOOLS` is ON) into engine-ready
  files: GPU-layout meshes with bounds, BC7 colour / BC5 normal / BC4 mask textures with full
  mips, pre-cooked PhysX collision (shared per mesh), and a level file (instances, nodes such as
  `PlayerStart_*` / `Interact_*`, lights from KHR_lights_punctual, bounds). Cooked output is
  deterministic. Layout: `<out>/<prefix>/levels/<name>/scene`, `meshes/`, `textures/`,
  `collision/`.
  - Cook: `LockstepCooker <src.gltf> --out Content/cooked/<name> --name levels/<name>`
    Textures are capped by the size of the objects using them: `--texels-per-metre N` (default
    1024, so a 0.3 m prop gets 512 and a 2 m bed 2048), `--max-texture-size N` (default 2048;
    tiling surfaces with UVs beyond 0-1 use it), `--no-auto-cap` to disable.
    `--gi` bakes an irradiance probe grid (`<name>/irradiance`, `Tools/Cooker/IrradianceBake`) over the
    `Zone_Interior_*` boxes: `--gi-spacing m` (1), `--gi-sun <azimuthDeg>,<elevationDeg>` (match the
    level's LevelLook sun; none = no sun term), `--gi-rays n` (256), `--gi-bounces n` (3). Hospital A:
    `--gi --gi-sun -120,6` (~20 s). Recook with `--gi` after moving geometry or static lights.
  - Third-party assets: every new one gets a line in `CREDITS.md` (CC BY ones must be credited
    in the shipped game).
  - Pack: `LockstepCooker pack Content/cooked/<name> Content/cooked/<name>.pak [--zstd-level 9]`
    (19 for release paks; 9 while iterating)
  - Run: `LockstepGame --content <dir|pak> [--content ...] --level levels/<name>/scene`
    (`--scene <gltf>` still loads glTF directly through the same VFS).
  Paks: custom format, per-file zstd, 4 KB alignment, TOC keyed by XXH64 path hash, content hash
  per pak (logged at startup; to be checked in the Net handshake). Planned split: `base.pak`,
  `intro.pak`, one per zone, later `patch_*.pak` (no manifest for that split yet). Engine reads
  everything through `Lockstep/Assets/FileSystem.hpp` with virtual paths; earlier mounts win.
  No encryption. Content lives in `Content/` inside the project but is git-ignored for now:
  `Content/source/<area>/` (`.blend`, glTF exports, extracted prop packs in
  `Content/source/props/`) and `Content/cooked/` (cooker output, paks).
- **Interactions:** glTF node names drive gameplay: `Interact_*` nodes are usable (look within
  2 m + E, host-authoritative, state replicated), `PlayerStart_1/2` are spawns, `Toy_*` are
  physics toys (names containing "ball" are throwable balls), `CarSpot` marks the car,
  `Interact_Locker_1/2` are each player's locker door and `Interact_Gear_1/2` the gear inside
  (top-level nodes, not under the door). Moving parts are child objects of an `Interact_*` empty
  placed on the hinge/pivot. `Npc_*` empties are client-only placeholder people (facing the
  empty's Blender +Y), looping an idle by role (`Npc_Civilian_*`/`Npc_Survivor_*` crouch,
  `Npc_SquadOutside_*`/`Npc_CordonOfficer`/`Npc_Guard` armed, the rest idle); a name part
  `_Idle`, `_Crouch`, `_Armed` or `_Aim` picks the pose (`Npc_Civilian_2_Idle`), and `_Full`,
  `_NoNVG` or `_Balaclava` the head gear (`Npc_SquadOutside_3_Armed_Balaclava`; also on
  `Npc_Squad_<n>` and `Spawn_Merc_*`). Default gear: civilians, survivors, prisoners balaclava
  only; police and squad a mix by marker number; mercs mostly balaclava; Player1 full, Player2
  without NVG (`Game/Characters/GearLooks.hpp`).
  `Npc_Squad_<n>` are the armed squad AI, not placeholders. In Blender, every object belongs to exactly one collection (the same
  as its parent), or linked instancing duplicates it. Foliage materials must export as glTF MASK
  (Math "Round" node in front of Principled Alpha); the cooker then keeps alpha coverage in mips.
  Shader-node tricks glTF can't express (vertex-colour mixes, non-identity Mapping nodes) must be
  baked into UVs or split into materials. Cooked levels are format version 4 (v2, v3 still load);
  recook after cooker changes. The Echo recorder saves both players' movement and
  interactions (`--echo-play <file>` replays it).
- **Shooting:** a shot leaves the eye along `shotDirection()` (`Game/Weapons/ShotAim`): the
  command's look (which already holds the camera recoil, `Weapons/Recoil`) turned by
  `PlayerCommand::aimOffset`, the sight line the shooter saw (front post as drawn while aiming
  down sights, the look/crosshair from the hip; sway, breathing, weapon kick, head motion), which
  the host clamps to `SpreadSettings::maxAimOffset`, plus a spread cone hashed from shooter and shot
  number, so client impacts match host hits. Fire modes Semi/Burst/Auto (B) run in the client's
  trigger; the host refuses shots faster than the cyclic rate. Penetration (`Weapons/Penetration`):
  material from node names, thickness by an inside/back trace, at most 2 surfaces (glass free),
  impacts on both faces (`CombatImpact::isExit`). World-space points of the drawn viewmodel
  (muzzle, sights, light rail; viewmodel FOV accounted for) come from `Characters/ViewModelSights`.
  F toggles the weapon flashlight when nothing can be interacted with; its state (and a coarse
  pitch) reaches the partner in `PlayerStatus`. Shift while aiming holds the breath. Dev:
  `--dev-fire-drill`. Loadout (`Weapons/WeaponCatalog`, `WeaponKind`): 1 M4, 2 Glock 17, 3 KRISS,
  mouse wheel cycles; per-weapon stats (ammo, reload times matching the clips, fire rate and
  modes, recoil, spread, penetration, damage, holster/draw times) in `Settings::weapons`; the
  wanted weapon rides in `PlayerCommand::weapon`, the host switches (`Weapons/Loadout`, no shots
  during holster + draw) and reports `PlayerStatus::weapon`. First-person models
  `characters/swat_fp_<m4|glock|kriss>` (the M4/Glock ones put the eye at the origin; clips
  FPAim, ReloadTactical, ReloadEmpty, Draw, Holster, retargeted by
  `Content/source/scripts/fp_weapon.py`); socket points per weapon in `Weapons/WeaponSockets`
  (W_Main joint space; M4 has an `Optic` rail socket); the partner holds `weapons/<m4|glock>`
  static guns at its right hand (`Characters/HeldGuns`). Dev: `--dev-weapon <m4|glock|kriss>`,
  `--dev-switch-drill <seconds>`, `--dev-capture-on <reload|switch> <step>:<count> <prefix>`.
- **Level lighting and surfaces (authoring):** `Zone_Interior_*` cube empties (display size 1,
  scaled so the cube covers the interior including its outer walls and roof; boxes are made
  axis-aligned) dim the sky's image-based light inside and add a fill light (per pixel; blends in
  over `blendDistance` inside the faces). Point/spot lights whose object name contains `_Shadow`
  cast shadows (nearest 4 per frame; a point light costs 6 atlas tiles of 16, a spot 1).
  Light shapes (`Game/Presentation/LevelLights`): a point light named `_Tube` is a fluorescent tube
  along the object's local X, its X scale the length in metres (Blender: rotate the light so X runs
  along the fixture, scale X to the tube length); `_Downlight`, `_WallWash`, `_Grille`,
  `_Fluorescent` pick an emission profile around the light's local -Z (+X is the profile's side;
  tubes default to `_Fluorescent`). Hospital A's ceiling troffers are 1.1 m tubes
  (`Content/source/scripts/hosp_tubes.py` converts a hospital .blend and exports it). The GI bake
  skips `_Flicker`/`_Siren`/`_Pulse`/`_NoBounce` lights (animated lights don't bounce).
  Animated lights (tags combine with `_Shadow`, e.g. `L_Ward_Flicker_Shadow`; `Game/Lights/`,
  presentation only, World time so bullet-time slows them): `_Flicker` a failing fluorescent tube
  (mostly on, dropouts/stutters, sometimes off 1-2 s, quiet 3D buzz + crackles),
  `_FlickerHard` a dying tube (mostly off, bursts), `_SirenRed` / `_SirenBlue` a police lightbar
  colour (double flashes, red then blue; colour is overridden; siren lights within 3 m of each
  other are one car and share a phase, cars are offset and drift), `_Pulse` slow breathing. Emissive
  meshes (lamps) whose bounds are within 1 m of an animated light, and nearer to it than to any
  other light, dim with it (`SceneInstance::emissiveScale`, one frame behind the light). Objects
  named `Decal_*` (or under such a node) are decals: thin meshes 2-5 mm off a surface with an
  Alpha Blend material, drawn right after opaque geometry in scene order. Glass: Principled
  Transmission > 0 (KHR_materials_transmission) or Alpha Blend with low alpha renders as glass
  with Fresnel reflections; an opaque dark "glass" material stays a dark panel. Breakable glass: every object whose name starts with `Breakable_Glass` is one pane (its own object, origin at the pane centre, thin box, glass material); it gets a collider although glass is alpha-blended, and one bullet (player, partner or NPC) breaks it: host-authoritative, broken set replicated (late join included), reset on level load, kept over checkpoint restarts; the pane stops drawing, its collider is removed and the bullet flies on (`Game/Breakables/`; shards, floor debris and sound in `Game/Effects/GlassShatter`; dev: `--dev-break-glass <n>`). Time of day: one
  empty named `Level_Look_Day|Evening|Night` picks a Game preset (`Game/Presentation/LevelLook`:
  sun, sky, IBL, exposure range, grade); none means Day. Custom properties (glTF extras) are not
  cooked, so these conventions are all names and materials. Weather: an empty named
  `Level_Weather_Rain` makes a level rainy (`Game/Weather/`: streaks, splashes, wet surfaces and
  puddles outdoors, rain sound, overcast look; presentation only, World time); `--weather rain|clear`
  overrides it, and the Game dev panel has a Weather section. Particle markers: empties named
  `Fx_Steam_*`, `Fx_Smoke_*`, `Fx_Dust_*` (ceiling dust trickle) emit along their Blender +Z. Cloth: objects named `Cloth_*` (e.g. `Cloth_Curtain_07`) are hanging cloth
  panels (thin vertical sheet, origin at the top edge's centre): Game hides the mesh and draws a
  simulated pleated sheet (`Game/Cloth/`, Verlet, top row pinned, presentation only, within 14 m of the
  camera); people push through, the local player's bullets kick it, wind moves outdoor ones; their
  colliders are removed at load on every machine. Hospital A's curtains come from
  `Content/source/scripts/split_curtains.py` (don't rerun the old `level_fix2.py`). Dev: `--dev-shake-cloth`.
  `Fx_Drip_*` empties 2-3 cm under a ceiling are leaks (`Game/Effects/Drips`): drops fall to the floor
  found by a ray, splash, plink and ripple a small puddle. Glowing screens: an empty named
  `Screen_*` at the middle of a display, facing its Blender -Y (Front view looks at the screen);
  emissive meshes within 0.35 m (or under it) become the screen (`ShadingModel::Screen`, flicker via
  emissive scale) and the nearest ones get an unshadowed spot light (`Game/Lights/Screens`, budget
  `GraphicsQuality::screenLights`, reserved before the level lights). Tags: `_Flicker`, `_Off`,
  `_Scroll`, `_Image` (keep the authored image; default is a procedural terminal tinted by it), `_Panel` (vending machine/sign: light, no scanlines), colour `_Blue`/`_Green`/`_Amber`/
  `_Red`/`_White`. Hospital A markers come from `Content/source/scripts/hosp_a_screens_drips.py`.
- **Levels:** `Content/source/apartment/` (the flat) is linked into
  `Content/source/district/district.blend` (building, stairs, street, park, car) at second-floor
  height; export the district by making the linked instances real, exporting, and reloading.
- **Game-specific shaders/materials:** not designed yet; decide when the first one is needed.
- **Physics: PhysX 5.6.1** (submodule `external/physx/PhysX`), built exactly as in Velix:
  `external/physx/CMakeLists.txt` runs `external/physx/scripts/build_physx_{linux.sh,windows.bat}`
  once and links the prebuilt static libs from `external/physx/lib/<os>/<type>` (git-ignored).
  `PHYSX_BUILD_TYPE` defaults to `checked` for Debug and `release` otherwise; CI uses `checked`.
  CPU-only (no CUDA). MSVC builds use the static runtime (`/MT`) to match PhysX; on Windows the
  PhysX DLLs are copied next to the executable. PhysX is private to Engine (`Engine/src/Physics`);
  no PhysX types in public headers. Design: the PhysX scene steps with the World time-domain
  delta (bullet-time slows it); character controllers (`PxController`) move with the Player delta,
  independently of the scene step, so a client can re-simulate its own pawn for prediction.
- **Dev tools:** an ImGui overlay toggled in-game, no separate editor (levels come from
  Blender). Engine/Debug owns the ImGui integration and engine panels. Game draws its own panels
  by calling ImGui directly; this is the one external header Game may include, and only inside
  `#if LOCKSTEP_DEV_TOOLS` blocks, so the `shipping` preset builds without ImGui.
  Frame captures (screen capture tools don't work on this Wayland desktop):
  `Renderer::captureNextFrame(path)` saves the next presented frame (tonemapped, with overlays) as a
  PNG without stalling; `--dev-capture <frame> <file.png>` (repeatable) or
  `--dev-capture <first>:<step>:<count> <prefix>` (writes `<prefix>_<frame>.png`) does it from the
  command line in dev builds. Captures need ~3 frames between them (later ones wait). Use the
  `release` build for captures: the debug build shows the dev overlay by default. Combine with
  `--dev-fire-drill` to look at weapon effects.
- **Graphics quality:** `GraphicsQuality` (`Lockstep/Render/GraphicsQuality.hpp`) is separate from
  `RenderSettings`: Game/LevelLook set the look in `RenderSettings`, and the Renderer renders it
  limited by the quality (`limitedByQuality()` each frame: features off, step counts and
  resolutions scaled; AA mode, cascades and shadowed light count come from the quality). Presets
  Low / Medium / High (default, the full look) / Ultra; Low = 960x540 window, 0.75 render scale
  (bilinear upscale in the output pass), FXAA, 2048 shadow atlas, top 2 texture mips dropped at
  `createTexture` (Medium 1; never below 128 px; applies to textures created later, so a level
  reload). Chosen by `--graphics low|medium|high|ultra` (wins, ignores file overrides), else the
  settings file (`--settings <file>`, default `~/.config/LockstepRAW/settings.ini`: `preset = ...`
  plus per-field overrides and `window_width`/`window_height`), else High. Dev Graphics panel >
  Quality: preset dropdown (live), every field, "Save to settings file". Engine logs GPU memory at
  startup, after Game init and after 30 frames. Group D fields: `lensFlares`, `bokeh`, `decalBudget`
  (bullet holes kept), `screenLights`, `waterRipples` (drips). Scene passes go through `addScaledPass()`
  (`RenderScale.cpp`) so their outputs follow the render scale.

## Code style

Braces always on their own line, for every block:

```cpp
if (condition)
{
}

class Renderer
{
};

void function()
{
}
```

- Members: `m_` prefix (`m_window`, `m_scene`).
- Constants: `k_` prefix (`k_maxFramesInFlight`).
- Types `PascalCase`, functions and variables `camelCase`, namespaces lowercase (`lockstep`).
- Header guards: `#ifndef LOCKSTEP_<MODULE>_<NAME>_HPP`.
- Closing namespace comment: `} //namespace lockstep`.
- `[[nodiscard]]` on functions whose result must be used.

## Comments

- **Comments go in headers only.** Each public function gets a comment that says what it does,
  what each argument is, and what it returns:

  ```cpp
  // Creates a GPU mesh from vertex and index data.
  // context:  Vela context that owns the GPU memory.
  // vertices: vertex data in StaticVertex layout.
  // indices:  triangle list indices into `vertices`.
  // Returns:  the mesh, or an error if GPU allocation failed.
  [[nodiscard]] static Result<Mesh> create(Context& context, std::span<const StaticVertex> vertices,
      std::span<const uint32_t> indices);
  ```

- Classes and structs in headers get a short comment saying what they are for.
- `.cpp` files have no comments. Make the code clear through naming instead.

## File size

- Split by responsibility: usually one main class per header/source pair.
- Keep files roughly under 500 lines. Past that, split along a real seam.
- Don't create a file for every tiny struct either. Small related types can share a header.

## Building and checking

```sh
cmake --preset debug
cmake --build --preset debug --parallel
timeout 5 ./build/debug/bin/LockstepGame     # exit code 124 = ran until timeout, which is fine
```

- **Test runs on the user's laptop (RTX 3050 Laptop, 4 GB VRAM) must stay light; GPU memory
  exhaustion has crashed the machine.** Every agent test run uses `--graphics low` (smallest
  window, lowest preset, reduced texture resolution), no exceptions: never `ultra` or `high`,
  graphics agents included (judge looks from `low` captures or ask the coordinator), and always
  `--present vsync`: dev builds otherwise render unthrottled (LowLatency falls back to mailbox),
  hundreds to 1000+ fps per instance, which pins the GPU and has frozen the laptop. Several
  instances at once are fine as long as every one of them is `--graphics low --present vsync`;
  always run them under `timeout` and don't leave instances running. Only a perf measurement
  that needs raw frame time may use `--present uncapped`, one short single instance.
- **Builds are serialized and capped**: several agents compiling three presets at full
  parallelism has frozen the machine (22 GB RAM, 12 threads). Always build through the lock with
  at most 4 jobs: `flock /tmp/lockstep-build.lock cmake --build --preset <p> --parallel 4`, one
  preset per command, and build `shipping` only at the end of a task.
- Debug builds enable the Vulkan validation layers. Validation messages count as bugs.
- Work is not done until it builds without warnings in our code and runs without validation errors.
- Presets: `debug`, `release`, `shipping` (`shipping` turns `LOCKSTEP_DEV_TOOLS` off).
- Don't commit unless the user asks.

## Vela notes

- Read Vela's public headers in `external/Vela/*/include/Vela/` and its README before using it.
  `external/Vela/Examples/LittleEngine` is the most complete usage example (glTF, lighting,
  shadows, ImGui).
- Known Vela gaps, candidates for vela-maintainer:
  1. (fixed in 6e13b93: `vela_stage_shaders(TARGET FROM)`, used by `lockstep_stage_engine_shaders`)
  2. (fixed in 6e13b93: shader `#include` tracking and restaging on shader-only edits)
  3. A pass can't render into a persistent `Texture` (the sky IBL atlas is rebuilt every
     frame). Persistent attachments + `InputUsage::History` exist since a601b8d (used by TAA);
     persistent depth with `LoadOp::Load` works too, and Vela branch `lockstep/shadow-caching`
     adds `PassRecorder::clearDepth(rect)`, `wasAttachmentReset(name)` and instanced mesh draws
     `drawInstanced(mesh, material, count, firstInstance)` (used by the shadow atlas cache).
  4. No cubemap, array or per-mip attachments.
  5. No `PassRecorder::setViewport` (only scissor).
  6. Only 4 pass-input slots (auto exposure reaches tonemapping through bloomUp1's alpha; lens flares
     through bloomUp1's colour).
  7. No pre-record upload hook (may be moot: `DynamicBuffer::update` before `execute()` now
     lands in that frame; check before asking).
  8. A depth prepass read by SSAO and then loaded by the lit pass forms a graph cycle.
  9. `ComputeRecorder` can't write images and only takes `Mat4` constants.
  10. No RG16F / R16F formats (TAA velocity uses RGBA16F, half wasted).
  11. One camera UBO per graph (the mirror view reads its matrix from the Engine lighting buffer).
  12. PBR push constants are 208 B, above Vulkan's guaranteed 128 B minimum (fine on current
      NVIDIA/AMD/Intel, which allow 256 B). No per-material uniform data, so material
      parameters ride in push constants.
  13. One blend mode and write mask per material for all colour outputs (the G-buffer stores data
      in alpha as "one minus" values so glass/decal blending stays consistent). With gap 4 this
      also rules out a hierarchical-depth SSR trace (no depth mip chain).
  14. No graph-wide resolution scale: attachment scale is relative to the swapchain only, so Engine
      wraps every scene pass (`addScaledPass`) and redescribes them all when the render scale changes.
