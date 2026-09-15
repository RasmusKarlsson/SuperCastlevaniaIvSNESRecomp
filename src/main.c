/*
 * Super Castlevania IV — desktop host shim.
 *
 * Identity and hooks, nothing else. The host itself is the framework's
 * (snesrecomp/runner/src/desktop/host_main.h): the pre-boot launcher, ROM
 * resolution and digest checks, config.ini and keybinds.ini, the window and
 * the SDL / OpenGL presenters, audio, gamepads, the in-game save-state
 * browser and rewind filmstrip, the OSD, the pacing clock, crash handlers and
 * the post-mortem report, mod packages, Generate & rebuild, and the netplay
 * barrier when the project is built with it. A fix there reaches this
 * project on a submodule pull; nothing in this file needs to change for it.
 *
 * This file used to BE the host -- a ~700-line copy made at scaffold time --
 * so every project scaffolded before a fix kept the bug. Two ports of the
 * same game, a week apart, had different hosts.
 *
 * Nothing here ever ships a ROM. The player supplies one; the host makes
 * that easy (launcher, then positional argument, then a copy beside the
 * executable, then the rom.cfg cache, then a file picker) and checks what it
 * is handed against the digests in rom_identity.txt.
 *
 * Grow this file only with what is specific to THIS title: a custom
 * presenter (prepare_frame / draw_frame), an SPC player, a pacing rule
 * (keep_pacing_debt), a Mods provider. See SnesDesktopHostGame for the
 * complete list of hooks and what each is for.
 */

#include <stdio.h>

#include "host_main.h"
#include "game_rtl.h"
#include "snesrecomp_rom_identity.h"  /* generated from rom_identity.txt */
#include "desktop/config.h"
#include "desktop/display_aspect.h"
#include "guarded_patch.h"
#include "snes/ppu.h"
#include "simon_spritesheet.h"

#ifndef __ANDROID__
#define SDL_MAIN_HANDLED 1
#endif
#include "desktop/sdl_compat.h"
#ifdef __ANDROID__
/* SDLActivity loads libmain.so and dlsyms its SDL_main. */
#define main SDL_main
#endif

#ifndef SNES_GAME_VERSION
#define SNES_GAME_VERSION "dev"
#endif

extern Ppu *g_ppu;
extern uint8 g_ram[0x20000];

/* The stock object handler keeps a 256-pixel-oriented activation/despawn
 * rectangle. These two immediates are the horizontal inset and span used by
 * that rectangle (ROM $80:C98B and $80:C991). A 48-pixel extension covers
 * our 43-pixel margins and gives large sprites five pixels of slop.
 *
 * Keep this as byte-checked host policy, not a modified ROM: a wrong revision
 * must fail closed, savestates can temporarily suspend registered patches,
 * and 4:3 continues to execute the retail bounds. The locations and wider
 * values are independently documented by bogaa's SCIV event-handler notes. */
enum { kCv4WidePatchCount = 6 };
static GuardedPatch g_cv4_wide_patches[kCv4WidePatchCount];

static void Cv4SyncWidescreenObjectBounds(void)
{
    for (unsigned i = 0; i < kCv4WidePatchCount; ++i) {
        GuardedPatch *patch = &g_cv4_wide_patches[i];
        if (!patch->target)
            continue;
        if (g_config.widescreen && !patch->applied)
            (void)guarded_patch_apply(patch);
        else if (!g_config.widescreen && patch->applied)
            (void)guarded_patch_revert(patch);
    }
}

static void Cv4InitWidescreenObjectBounds(const uint8_t *rom, size_t size)
{
    static const struct {
        size_t pc;
        const char *name;
        uint8_t original[3];
        uint8_t wide[3];
    } kPatches[kCv4WidePatchCount] = {
        { 0x498bu, "SCIV widescreen object left bound",
          { 0xe9, 0x60, 0x00 }, { 0xe9, 0x30, 0x00 } },
        { 0x4991u, "SCIV widescreen object span",
          { 0x69, 0xc0, 0x01 }, { 0x69, 0xf0, 0x01 } },
        /* The event stream has its own camera-relative read window. Without
         * widening both forward and reverse traversal, scripted enemies,
         * doors, candles and moving set pieces still appear/disappear at the
         * retail 256-pixel edge even though their object slots can survive. */
        { 0x557eu, "SCIV widescreen event right trailing edge",
          { 0xe9, 0x20, 0x00 }, { 0xe9, 0x50, 0x00 } },
        { 0x55a1u, "SCIV widescreen event right leading edge",
          { 0x69, 0x20, 0x01 }, { 0x69, 0x50, 0x01 } },
        { 0x55d4u, "SCIV widescreen event left leading edge",
          { 0x69, 0x20, 0x01 }, { 0x69, 0x50, 0x01 } },
        { 0x55f2u, "SCIV widescreen event left trailing edge",
          { 0xe9, 0x20, 0x00 }, { 0xe9, 0x50, 0x00 } },
    };

    if (!rom)
        return;
    for (unsigned i = 0; i < kCv4WidePatchCount; ++i) {
        if (size < kPatches[i].pc + sizeof(kPatches[i].original))
            continue;
        GuardedPatchStatus status = guarded_patch_init(
            &g_cv4_wide_patches[i], kPatches[i].name,
            (uint8_t *)rom + kPatches[i].pc, sizeof(kPatches[i].original),
            kPatches[i].original, kPatches[i].wide);
        if (status == kGuardedPatch_Ok)
            status = guarded_patch_register(&g_cv4_wide_patches[i]);
        if (status != kGuardedPatch_Ok)
            fprintf(stderr, "[widescreen] patch refused: %s: %s\n",
                    kPatches[i].name, guarded_patch_status_name(status));
    }
    Cv4SyncWidescreenObjectBounds();
}

/* SCIV's normal field is 256x224.  The runner's native widescreen PPU can
 * rasterize real background and OBJ columns outside that field, rather than
 * stretching the original picture.  342 is the nearest centered, even 4/3
 * expansion of 256; with the authentic 7:6 SNES pixel aspect it presents as
 * 16:9.  BG3 remains clamped by the PPU's default policy because SCIV uses it
 * for the status display and text. */
static void Cv4PrepareFrame(int drawable_w, int drawable_h,
                            int *frame_w, int *frame_h)
{
    (void)drawable_w;
    (void)drawable_h;
    *frame_w = g_config.widescreen
        ? SnesDisplayAspect_ComputeWideFrameWidth(256)
        : 256;
    *frame_h = 224;
}

static void Cv4BeginSimFrame(unsigned number)
{
    (void)number;
    Cv4SyncWidescreenObjectBounds();
    /* Be explicit about the conservative first-pass policy. World BG1/BG2
     * use the wider tilemap raster, while BG3 stays in the authored field.
     * This is reapplied after PpuSetExtraSpace resets per-frame policies. */
    PpuSetWidescreenLayerClamp(g_ppu, 1u << 2);
    /* SCIV uses hardware windows for masks and color math throughout normal
     * scenes. Treat authored edges at the native screen boundary as edges of
     * the widened field too, otherwise valid BG margin pixels are masked back
     * to the fixed-color backdrop. This changes host presentation only. */
    PpuSetWidescreenWindowExpansion(g_ppu, 0x3fu, 0x03u);
    /* Stage 1 has uninitialised tilemap data immediately left of its opening
     * room. Preserve the original widescreen policy: spend the full margin
     * on the valid right side there, and use centred 16:9 everywhere else. */
    if (g_config.widescreen && g_ram[0x0032] == 0x04 && g_ram[0x0086] == 0x00)
        PpuSetExtraSideSpace(g_ppu, 0, g_ppu->extraLeftRight, 0);
    Cv4SimonSpritesheetBeginFrame(g_ppu, g_ram);
}

static const SnesDesktopHostGame kGameHost = {
    .display_name        = "Super Castlevania IV",
    .window_title        = "Super Castlevania IV",
    .region              = SNESRECOMP_ROM_REGION,
    .rom_file            = SNESRECOMP_ROM_FILE,
    .expected_sha256_hex = SNESRECOMP_ROM_EXPECTED_SHA256,
    .expected_crc32_hex  = SNESRECOMP_ROM_EXPECTED_CRC32,
    .game_id             = SNESRECOMP_ROM_GAME_ID,
    .build_version       = SNES_GAME_VERSION,
    .game_info           = &kGameInfo,
    .num_players         = 1,
    .widescreen_supported = 1,
    .display_aspect_supported = 1,
    .shader_supported     = 1,
    .state_menu_hotkeys   = 1,
    .escape_settings     = 1,
    .native_widescreen   = 1,
    .frame_width         = 256,
    .frame_height        = 224,
    .on_rom_loaded       = &Cv4InitWidescreenObjectBounds,
    .prepare_frame       = &Cv4PrepareFrame,
    .begin_sim_frame     = &Cv4BeginSimFrame,
    /* Battery-backed SRAM shows the launcher's SAVES panel. Leave NULL for a
     * title without one. The path is exe-relative. */
    .sram_path           = "saves/save.srm",
};

#ifndef __ANDROID__
#undef main   /* desktop: keep plain main() even if SDL_main.h remapped it */
#endif
int main(int argc, char **argv)
{
    return snesrecomp_desktop_main(&kGameHost, argc, argv);
}
