#ifndef LOCKSTEP_GAME_EFFECTS_VIEW_MODEL_FLASH_HPP
#define LOCKSTEP_GAME_EFFECTS_VIEW_MODEL_FLASH_HPP

#include "Weapons/WeaponSockets.hpp"

#include <Lockstep/Render/Resources.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // Look of the local weapon's muzzle flash, editable from the dev panel.
    // enabled:     draw it.
    // size:        diameter of the front star, metres (viewmodel space).
    // length:      length of the side flame along the barrel, metres.
    // intensity:   emissive HDR brightness of the first frame (keep it low: bloom and auto
    //              exposure react to it).
    // secondFrame: brightness of the second frame relative to the first.
    // frames:      frames it shows (1 or 2).
    // aimScale:    factor on size and brightness fully aimed down sights.
    struct ViewModelFlashTuning
    {
        bool enabled{true};
        float size{0.085f};
        float length{0.1f};
        float intensity{2.5f};
        float secondFrame{0.35f};
        int frames{2};
        float aimScale{0.45f};
    };

    // The local first-person weapon's muzzle flash: a small crossed-quad flash mesh (a front star
    // and two side flames, three star variants) drawn on render::k_viewModelLayer at the muzzle of
    // the drawn weapon, with its field of view and squeezed depth, so it sits exactly on the
    // muzzle and follows sway, kick and the reload clip. Emissive with an alpha cutout (a blended
    // material would take the glass path and catch the weapon light as a pale square); bloom
    // softens its edges. Presentation only.
    // texture, material, meshes: the look, once created (meshes per star variant).
    // isTried:    creating them was attempted.
    // frame:      frames shown of the current flash; frames or more means none shows.
    // variant, roll, scale: this flash's star, its turn about the barrel and its size factor.
    // random:     presentation-only random state.
    // tuning:     the look.
    struct ViewModelFlash
    {
        lockstep::render::TextureHandle texture{};
        lockstep::render::MaterialHandle material{};
        std::array<lockstep::render::MeshHandle, 3> meshes{};
        bool isTried{false};
        int frame{1000};
        uint32_t variant{0};
        float roll{0.0f};
        float scale{1.0f};
        uint32_t random{0x7FEB352Du};
        ViewModelFlashTuning tuning{};
    };

    // Creates the flash texture, material and meshes once; failures are logged.
    // flash:    the flash.
    // renderer: the renderer.
    void createViewModelFlash(ViewModelFlash& flash, lockstep::render::Renderer& renderer);

    // Starts a flash for a shot of the local weapon (a new star, turn and size).
    // flash: the flash.
    // size:  factor on its size (the weapon's WeaponStats::flashScale).
    void fireViewModelFlash(ViewModelFlash& flash, float size);

    // Draws the current flash, if one shows, at the drawn muzzle, and advances it by one frame.
    // Call once per frame after the viewmodel was drawn.
    // flash:    the flash.
    // renderer: the renderer.
    // socket:   the weapon in viewmodel-layer space (viewModelLayerSocket()), or nothing while
    //           hidden.
    // aimed:    0 from the hip to 1 fully aimed down sights.
    void drawViewModelFlash(ViewModelFlash& flash, lockstep::render::Renderer& renderer,
        const std::optional<WeaponSocket>& socket, float aimed);
} //namespace game

#endif //LOCKSTEP_GAME_EFFECTS_VIEW_MODEL_FLASH_HPP
