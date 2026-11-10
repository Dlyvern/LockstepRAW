#ifndef LOCKSTEP_RENDER_SHADOW_PASS_HPP
#define LOCKSTEP_RENDER_SHADOW_PASS_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"

#include "ShadowCascades.hpp"

#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Mesh.hpp>
#include <Vela/Graphics/Pass.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace lockstep::render
{
    struct RendererState;
    struct MaterialRecord;

    // Renders the depth of shadow casters into their tiles of the fixed-size, persistent shadow
    // atlas: the sun's cascades and the local lights' tiles. A tile is re-rendered (cleared, then
    // drawn) only when its key changed (its matrix or a caster in it; FrameDrawLists::shadowKeys),
    // it holds a skinned caster, or the cache was invalidated (ShadowCache); otherwise it keeps
    // what an earlier frame rendered. Static casters sharing a mesh and shadow material are drawn
    // as one instanced batch from RendererState::shadowInstanceBuffer; skinned casters one by one.
    // Alpha-masked materials discard in their own shadow material. Every shadow material renders
    // with the hardware depth bias of the shadow settings (see updateShadowMaterials()).
    class ShadowPass final : public vela::graphics::Pass
    {
    public:
        // Creates the pass. It draws opaque casters with RendererState::shadowMaterial, which must
        // exist before the first frame.
        // state:   the renderer state whose draw lists, prepared lighting, shadow materials and
        //          shadow cache the pass uses every frame. Must outlive the pass.
        // Returns: the pass.
        [[nodiscard]] static Result<std::unique_ptr<ShadowPass>> create(RendererState& state);

        // Returns: the persistent shadow atlas as the only (depth) output, loaded every frame.
        [[nodiscard]] vela::graphics::PassDescription describe() const override;

        // Uploads the instance matrices of the tiles to re-render, then clears and draws each of
        // them, scissored to the tile.
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        // Casters drawn with one call: an instanced batch of static casters, or one skinned caster.
        // material:      the shadow material.
        // record:        the lit material of the casters (alpha mask factors).
        // firstInstance: the batch's first light matrix in the instance buffer.
        // count:         instances; 0 for a skinned caster.
        // draw:          for a skinned caster, its index in RendererState::draws.
        struct Batch
        {
            const vela::graphics::Mesh* mesh{nullptr};
            const vela::graphics::Material* material{nullptr};
            const MaterialRecord* record{nullptr};
            uint32_t firstInstance{0};
            uint32_t count{0};
            uint32_t draw{0};
        };

        explicit ShadowPass(RendererState& state);

        // Decides which tiles to re-render this frame and gathers their batches and instances.
        // recorder: the pass's recorder (whether the atlas was just reset).
        void gatherTiles(const vela::graphics::PassRecorder& recorder);

        // Adds a tile's casters to the batches and instances.
        // tile: the tile.
        void gatherCasters(uint32_t tile);

        // Clears one tile and draws its batches.
        // recorder: the pass's recorder.
        // tile:     the tile.
        void drawTile(vela::graphics::PassRecorder& recorder, uint32_t tile);

        RendererState* m_state{nullptr};
        std::vector<math::Mat4> m_instances;
        std::vector<Batch> m_batches;
        std::array<uint32_t, k_maxShadowTiles + 1> m_tileBatches{};
        std::array<bool, k_maxShadowTiles> m_isTileRendered{};
        bool m_hasInstances{false};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SHADOW_PASS_HPP
