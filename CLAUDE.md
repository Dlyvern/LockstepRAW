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
character controllers). Later: `Audio`, `Anim`, `Assets`. Create a module folder only when there is code
for it.

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
  lights every frame via `Renderer::submitLight`. The Engine-internal `RenderPass` interface is
  how Engine passes plug in; Game can't add passes yet.
- **Materials and assets:** art direction is realistic (Trepang2-like). The material model is
  glTF 2.0 metallic-roughness, defined in `Lockstep/Render/PbrMaterial.hpp` (`PbrVertex` with
  tangents, `PbrMaterialDescription`). That header is the contract: Engine/Assets (glTF import)
  fills it, Engine/Render implements it. Test content lives outside the repo (Sponza, Bistro in
  `~/Downloads`) and is loaded with `--scene <path>`. Imported levels are walkable (PhysX
  triangle-mesh collision).
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

- Debug builds enable the Vulkan validation layers. Validation messages count as bugs.
- Work is not done until it builds without warnings in our code and runs without validation errors.
- Presets: `debug`, `release`, `shipping` (`shipping` turns `LOCKSTEP_DEV_TOOLS` off).
- Don't commit unless the user asks.

## Vela notes

- Read Vela's public headers in `external/Vela/*/include/Vela/` and its README before using it.
  `external/Vela/Examples/LittleEngine` is the most complete usage example (glTF, lighting,
  shadows, ImGui).
- Known Vela gaps, candidates for vela-maintainer:
  1. `vela_compile_shaders` can't stage shaders of a static library for the executables
     linking it (Engine works around it with `lockstep_stage_engine_shaders`).
  2. `vela_compile_shaders` doesn't restage on shader-only edits and doesn't track `#include`.
  3. Attachments don't persist contents between frames, and a pass can't render into a
     persistent `Texture` (the sky IBL atlas is rebuilt every frame).
  4. No cubemap, array or per-mip attachments.
  5. No `PassRecorder::setViewport` (only scissor).
  6. Only 4 pass-input slots.
  7. No pre-record upload hook (may be moot: `DynamicBuffer::update` before `execute()` now
     lands in that frame; check before asking).
  8. A depth prepass read by SSAO and then loaded by the lit pass forms a graph cycle.
  9. `ComputeRecorder` can't write images and only takes `Mat4` constants.
