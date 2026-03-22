#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "constants.h"
#include "starfield.h"
#include "mothership.h"
#include "laser.h"
#include "asteroid.h"
#include "beasties.h"
#include "palette.h"
#include "input.h"
#include "score.h"
#include "opl.h"
#include "lander.h"
#include "sound.h"
#include "defense.h"

unsigned MAIN_MAP_CONFIG;
unsigned MOTHERSHIP_CONFIG;
unsigned LASER_CONFIG;
unsigned ASTEROID_CONFIG;
unsigned BEASTIE1_CONFIG;
unsigned BEASTIE2_CONFIG;
unsigned LANDER_CONFIG;
unsigned BEAM_CONFIG;

#define TITLE_X0 8
#define TITLE_X1 32
#define TITLE_Y0 4
#define TITLE_Y1 7
#define TITLE_W  (TITLE_X1 - TITLE_X0 + 1)
#define TITLE_H  (TITLE_Y1 - TITLE_Y0 + 1)
#define TITLE_TILE_COUNT (TITLE_W * TITLE_H)

#define SHIELD_START 48
#define SHIELD_HIT_LOSS 12
#define SHIELD_FIRE_LOSS 1
#define SHIELD_ASTEROID_REWARD 1
#define SHIELD_BEASTIE_REWARD 12
#define SHIELD_SEGMENTS 6
#define SHIELD_POINTS_PER_SEGMENT 8
#define SHIELD_BAR_X0 17
#define SHIELD_BAR_Y 27
#define SHIELD_TILE_FULL 18
#define SHIELD_TILE_EMPTY 26

#define TERRAIN_Y0 18
#define TERRAIN_Y1 24
#define TERRAIN_ROW_COUNT (TERRAIN_Y1 - TERRAIN_Y0 + 1)
#define TERRAIN_TILE_COUNT (MAIN_MAP_WIDTH_TILES * TERRAIN_ROW_COUNT)
#define DEEP_SPACE_TILE 87
#define DEEP_SPACE_ROW24_TILE 1

typedef enum {
    GAME_MODE_DEMO = 0,
    GAME_MODE_PLAYING,
    GAME_MODE_GAME_OVER,
} GameMode;

static GameMode game_mode = GAME_MODE_DEMO;
static int16_t shield_points = SHIELD_START;
static bool fire_start_armed = false;
static uint8_t title_tiles_backup[TITLE_TILE_COUNT];
static uint8_t terrain_rows_backup[TERRAIN_TILE_COUNT];
static bool planet_surface_phase = false;
static uint16_t planet_timer = 0;
static bool game_music_started = false;
static uint8_t space_kills = 0;
static bool laser_fire_held = false;
static uint8_t permanent_captures = 0;  // beasties captured from previous visits this cycle
static uint8_t beasties_delivered = 0;  // docked this visit, committed to permanent on departure
static uint8_t game_level = 0;          // increases with each full capture cycle
static bool full_rescue_bonus = false;  // true: both beasties rescued same trip before klaxon

#define SONG_HZ 60
uint8_t vsync_last = 0;
uint16_t timer_accumulator = 0;
bool music_enabled = true;

static unsigned tilemap_addr(unsigned tilemap_base, uint8_t x, uint8_t y)
{
    return (unsigned)(tilemap_base + ((unsigned)y * MAIN_MAP_WIDTH_TILES) + x);
}

static void tilemap_write(unsigned tilemap_base, uint8_t x, uint8_t y, uint8_t tile)
{
    RIA.addr0 = tilemap_addr(tilemap_base, x, y);
    RIA.step0 = 1;
    RIA.rw0 = tile;
}

static uint8_t tilemap_read(unsigned tilemap_base, uint8_t x, uint8_t y)
{
    RIA.addr0 = tilemap_addr(tilemap_base, x, y);
    RIA.step0 = 1;
    return RIA.rw0;
}

static void tilemap_fill_row(unsigned tilemap_base, uint8_t y, uint8_t tile)
{
    RIA.addr0 = tilemap_addr(tilemap_base, 0, y);
    RIA.step0 = 1;
    for (uint8_t x = 0; x < MAIN_MAP_WIDTH_TILES; ++x)
        RIA.rw0 = tile;
}

static void backup_title_tiles(void)
{
    uint8_t x;
    uint8_t y;
    uint8_t i = 0;

    for (y = TITLE_Y0; y <= TITLE_Y1; ++y) {
        for (x = TITLE_X0; x <= TITLE_X1; ++x) {
            title_tiles_backup[i++] = tilemap_read(MOTHERSHIP_MAP_TILEMAP_DATA, x, y);
        }
    }
}

static void backup_terrain_rows(void)
{
    uint8_t y;
    uint8_t x;
    uint16_t i = 0;

    for (y = TERRAIN_Y0; y <= TERRAIN_Y1; ++y) {
        for (x = 0; x < MAIN_MAP_WIDTH_TILES; ++x) {
            terrain_rows_backup[i++] = tilemap_read(MAIN_MAP_TILEMAP_DATA, x, y);
        }
    }
}

static void set_terrain_rows_tile(uint8_t tile)
{
    uint8_t y;
    uint8_t x;

    for (y = TERRAIN_Y0; y <= TERRAIN_Y1; ++y) {
        for (x = 0; x < MAIN_MAP_WIDTH_TILES; ++x) {
            tilemap_write(MAIN_MAP_TILEMAP_DATA, x, y, tile);
        }
    }
}

static void set_deep_space_terrain(void)
{
    // Keep rows 18-22 as authored terrain; deep-space styling uses rows 23-24.
    tilemap_fill_row(MAIN_MAP_TILEMAP_DATA, 23, DEEP_SPACE_TILE);
    tilemap_fill_row(MAIN_MAP_TILEMAP_DATA, 24, DEEP_SPACE_ROW24_TILE);
}

static void update_launch_tube(void)
{
    // Open the launch tube at the planet, otherwise transparent
    uint8_t tile = planet_surface_phase ? 78 : 0;
    tilemap_write(MOTHERSHIP_MAP_TILEMAP_DATA, 19, 12, tile);
    tilemap_write(MOTHERSHIP_MAP_TILEMAP_DATA, 20, 12, tile);
}

static void restore_terrain_rows(void)
{
    uint8_t y;
    uint8_t x;
    uint16_t i = 0;

    for (y = TERRAIN_Y0; y <= TERRAIN_Y1; ++y) {
        for (x = 0; x < MAIN_MAP_WIDTH_TILES; ++x) {
            tilemap_write(MAIN_MAP_TILEMAP_DATA, x, y, terrain_rows_backup[i++]);
        }
    }
}

static void draw_shield_bar(void);

static void toggle_surface_phase(void)
{
    planet_surface_phase = !planet_surface_phase;
    if (planet_surface_phase) {
        // Entering planet: commit nothing yet — terrain/beastie work deferred until
        // the ship clears the screen (complete_phase_tile_work fires on departure).
        beasties_delivered = 0;
        beasties_hide_all(); // keep them hidden until tiles swap after departure
    } else {
        // Leaving the planet: beasties that docked are now fully captured.
        // Full energy refill only if both were rescued this trip before the klaxon.
        permanent_captures += beasties_delivered;
        if (game_mode == GAME_MODE_PLAYING) {
            if (full_rescue_bonus) {
                shield_points = SHIELD_START;
            } else {
                shield_points += (int16_t)(beasties_delivered * SHIELD_BEASTIE_REWARD);
                if (shield_points > SHIELD_START) shield_points = SHIELD_START;
            }
            draw_shield_bar();
        }
        full_rescue_bonus = false;
        beasties_delivered = 0;
        if (permanent_captures >= 2) {
            permanent_captures = 0;
            beasties_advance_type();
            if (game_mode == GAME_MODE_PLAYING) {
                score_add(1000);
                ++game_level;
                asteroid_set_level(game_level);
                defense_set_level(game_level);
            }
        }
        defense_hide();
    }

    asteroid_set_planet_phase(planet_surface_phase);
    asteroid_set_spawns_paused(planet_surface_phase && game_mode == GAME_MODE_PLAYING);
    if (mothership_is_landed()) mothership_start_departure();
}

static void hide_title_tiles(void)
{
    uint8_t x;
    uint8_t y;

    for (y = TITLE_Y0; y <= TITLE_Y1; ++y) {
        for (x = TITLE_X0; x <= TITLE_X1; ++x) {
            tilemap_write(MOTHERSHIP_MAP_TILEMAP_DATA, x, y, 0);
        }
    }
}

static void restore_title_tiles(void)
{
    uint8_t x;
    uint8_t y;
    uint8_t i = 0;

    for (y = TITLE_Y0; y <= TITLE_Y1; ++y) {
        for (x = TITLE_X0; x <= TITLE_X1; ++x) {
            tilemap_write(MOTHERSHIP_MAP_TILEMAP_DATA, x, y, title_tiles_backup[i++]);
        }
    }
}

static void draw_shield_bar(void)
{
    int16_t clamped_shield = shield_points;

    if (clamped_shield < 0)
        clamped_shield = 0;
    if (clamped_shield > SHIELD_START)
        clamped_shield = SHIELD_START;

    for (uint8_t segment = 0; segment < SHIELD_SEGMENTS; ++segment) {
        int16_t segment_points = clamped_shield - (int16_t)(segment * SHIELD_POINTS_PER_SEGMENT);
        uint8_t fill;
        uint8_t tile;

        if (segment_points <= 0)
            fill = 0;
        else if (segment_points >= SHIELD_POINTS_PER_SEGMENT)
            fill = SHIELD_POINTS_PER_SEGMENT;
        else
            fill = (uint8_t)segment_points;

        // Tiles 18..26 map from full(8/8) to empty(0/8).
        tile = (uint8_t)(SHIELD_TILE_EMPTY - fill);
        tilemap_write(MAIN_MAP_TILEMAP_DATA, (uint8_t)(SHIELD_BAR_X0 + segment), SHIELD_BAR_Y, tile);
    }
}

// ── Game-over sequence ────────────────────────────────────────────────────────

#define GAMEOVER_ANIM_FRAMES        7   // animation color groups
#define GAMEOVER_INDICES_PER_FRAME  7   // palette indices per group
#define GAMEOVER_ANIM_TICKS         6   // vsync frames per animation step
#define GAMEOVER_TIMEOUT            (10 * 60)  // 10 s at 60 Hz

// 7 groups of 7 palette indices (stride-16 within each group)
static const uint8_t gameover_frame_indices[GAMEOVER_ANIM_FRAMES][GAMEOVER_INDICES_PER_FRAME] = {
    {  31,  47,  63,  79,  95, 111, 127 },
    {  18,  34,  50,  66,  82,  98, 114 },
    {  20,  36,  52,  68,  84, 100, 116 },
    {  21,  37,  53,  69,  85, 101, 117 },
    {  22,  38,  54,  70,  86, 102, 118 },
    {  23,  39,  55,  71,  87, 103, 119 },
    {  24,  40,  56,  72,  88, 104, 120 },
};

// Palette indices to keep at authored colors during the blackout
static const uint8_t gameover_preserve_indices[] = {
    10,                                       // background
    15,                                       // ground
    125,                                      // lander
    107, 113,                                 // score
    4, 20, 36, 52, 68, 84, 100, 116,         // mothership destruction
    26, 42, 58, 74, 90, 106, 122,            // starfield
};
#define GAMEOVER_PRESERVE_COUNT \
    ((uint8_t)(sizeof(gameover_preserve_indices) / sizeof(gameover_preserve_indices[0])))

// Full MAIN_MAP snapshot taken before game-over tile replacement
#define MAIN_MAP_TILE_COUNT (MAIN_MAP_WIDTH_TILES * MAIN_MAP_HEIGHT_TILES)
static uint8_t gameover_main_map_snapshot[MAIN_MAP_TILE_COUNT];
static bool    gameover_snapshot_valid = false;

static uint8_t  gameover_stage;      // 0=destroying  1=build-up  2=rotating
static uint16_t gameover_timer;      // frames since stage-1 start
static uint8_t  gameover_anim_tick;  // countdown to next animation step
static uint8_t  gameover_anim_step;  // current step within build phase
static bool     gameover_exit_armed; // true once a button was pressed (debounce)
static uint16_t gameover_colors[GAMEOVER_ANIM_FRAMES][GAMEOVER_INDICES_PER_FRAME];

// Lander escape sub-state (runs during stages 1+)
#define GAMEOVER_ESCAPE_START_X   152  // pixel x for top-left of 16px sprite at screen centre
#define GAMEOVER_ESCAPE_START_Y   112  // pixel y
#define GAMEOVER_ESCAPE_ZIGZAG_PERIOD 14  // frames per horizontal direction flip
#define GAMEOVER_ESCAPE_MOVE_TICKS     2   // advance position every N frames
static bool    gameover_escape_active;
static int16_t gameover_escape_x;
static int16_t gameover_escape_y;
static int8_t  gameover_escape_zigzag_dir;  // +1 or -1
static uint8_t gameover_escape_zigzag_tick;
static uint8_t gameover_escape_move_tick;

static void palette_write_go(uint8_t index, uint16_t color)
{
    RIA.addr0 = PALETTE_ADDR + ((unsigned)index * 2u);
    RIA.step0 = 1;
    RIA.rw0 = color & 0xFF;
    RIA.rw0 = color >> 8;
}

static void restore_full_palette(void)
{
    int i;
    RIA.addr0 = PALETTE_ADDR;
    RIA.step0 = 1;
    for (i = 0; i < 256; i++) {
        RIA.rw0 = tile_palette[i] & 0xFF;
        RIA.rw0 = tile_palette[i] >> 8;
    }
}

static void gameover_replace_tiles(void)
{
    uint8_t  x, y;
    uint16_t i = 0;

    // Snapshot the entire MAIN_MAP before making any changes
    for (y = 0; y < MAIN_MAP_HEIGHT_TILES; ++y)
        for (x = 0; x < MAIN_MAP_WIDTH_TILES; ++x)
            gameover_main_map_snapshot[i++] = tilemap_read(MAIN_MAP_TILEMAP_DATA, x, y);
    gameover_snapshot_valid = true;

    // Apply replacement: 101 or 102 → 215; tile to the right → 216
    i = 0;
    for (y = 0; y < MAIN_MAP_HEIGHT_TILES; ++y) {
        for (x = 0; x < MAIN_MAP_WIDTH_TILES; ++x) {
            if (gameover_main_map_snapshot[i++] == 101 ||
                gameover_main_map_snapshot[i - 1u] == 102) {
                tilemap_write(MAIN_MAP_TILEMAP_DATA, x, y, 215);
                if (x + 1u < MAIN_MAP_WIDTH_TILES)
                    tilemap_write(MAIN_MAP_TILEMAP_DATA, (uint8_t)(x + 1u), y, 216);
            }
        }
    }
}

static void gameover_restore_tiles(void)
{
    uint8_t  x, y;
    uint16_t i = 0;

    if (!gameover_snapshot_valid) return;
    for (y = 0; y < MAIN_MAP_HEIGHT_TILES; ++y)
        for (x = 0; x < MAIN_MAP_WIDTH_TILES; ++x)
            tilemap_write(MAIN_MAP_TILEMAP_DATA, x, y, gameover_main_map_snapshot[i++]);
    gameover_snapshot_valid = false;
}

#define GAMEOVER_TEXT_X0  11
#define GAMEOVER_TEXT_COL_COUNT 18

static const uint8_t gameover_text_row18[GAMEOVER_TEXT_COL_COUNT] = {
    217, 218, 219, 220, 183, 221, 222, 223, 224, 225,
    226, 227, 228, 229, 230, 231, 232, 233
};
static const uint8_t gameover_text_row19[GAMEOVER_TEXT_COL_COUNT] = {
    234, 235, 236, 237, 238, 239, 240, 241, 242, 234,
    243, 244, 245, 246, 247, 248, 249, 250
};

static void gameover_write_text_tiles(void)
{
    uint8_t c;
    for (c = 0; c < GAMEOVER_TEXT_COL_COUNT; ++c) {
        tilemap_write(MOTHERSHIP_MAP_TILEMAP_DATA,
                      (uint8_t)(GAMEOVER_TEXT_X0 + c), 18,
                      gameover_text_row18[c]);
        tilemap_write(MOTHERSHIP_MAP_TILEMAP_DATA,
                      (uint8_t)(GAMEOVER_TEXT_X0 + c), 19,
                      gameover_text_row19[c]);
    }
}

static void gameover_clear_text_tiles(void)
{
    uint8_t c;
    for (c = 0; c < GAMEOVER_TEXT_COL_COUNT; ++c) {
        tilemap_write(MOTHERSHIP_MAP_TILEMAP_DATA,
                      (uint8_t)(GAMEOVER_TEXT_X0 + c), 18, 0);
        tilemap_write(MOTHERSHIP_MAP_TILEMAP_DATA,
                      (uint8_t)(GAMEOVER_TEXT_X0 + c), 19, 0);
    }
}

static void gameover_palette_blackout(void)
{
    uint16_t i;
    uint8_t  p;
    bool     preserve;
    for (i = 0; i < 256u; ++i) {
        preserve = false;
        for (p = 0; p < GAMEOVER_PRESERVE_COUNT; ++p) {
            if (gameover_preserve_indices[p] == (uint8_t)i) {
                preserve = true;
                break;
            }
        }
        if (!preserve)
            palette_write_go((uint8_t)i, 0);
    }
}

static void gameover_init_colors(void)
{
    uint8_t f, j;
    for (f = 0; f < GAMEOVER_ANIM_FRAMES; ++f)
        for (j = 0; j < GAMEOVER_INDICES_PER_FRAME; ++j)
            gameover_colors[f][j] = tile_palette[gameover_frame_indices[f][j]];
}

static void gameover_write_frame_colors(uint8_t frame)
{
    uint8_t j;
    for (j = 0; j < GAMEOVER_INDICES_PER_FRAME; ++j)
        palette_write_go(gameover_frame_indices[frame][j], gameover_colors[frame][j]);
}

static void gameover_rotate_colors(void)
{
    uint16_t saved[GAMEOVER_INDICES_PER_FRAME];
    uint8_t  f, j;
    // Shift each frame's colors one step forward (wrapping last → first)
    for (j = 0; j < GAMEOVER_INDICES_PER_FRAME; ++j)
        saved[j] = gameover_colors[GAMEOVER_ANIM_FRAMES - 1u][j];
    for (f = GAMEOVER_ANIM_FRAMES - 1u; f > 0u; --f)
        for (j = 0; j < GAMEOVER_INDICES_PER_FRAME; ++j)
            gameover_colors[f][j] = gameover_colors[f - 1u][j];
    for (j = 0; j < GAMEOVER_INDICES_PER_FRAME; ++j)
        gameover_colors[0][j] = saved[j];
    // Write all updated groups to the palette
    for (f = 0; f < GAMEOVER_ANIM_FRAMES; ++f)
        gameover_write_frame_colors(f);
}

static void start_game_over_sequence(void)
{
    game_mode           = GAME_MODE_GAME_OVER;
    gameover_stage      = 0;
    gameover_timer      = 0;
    gameover_anim_tick  = 0;
    gameover_anim_step  = 0;
    gameover_exit_armed = false;
    gameover_escape_active   = false;
    gameover_escape_move_tick = 0;
    music_enabled       = false;
    starfield_freeze();
    sound_play_destruction();
    mothership_start_destruction();
    asteroid_reset();
    laser_init();
    lander_reset();
    beasties_hide_all();
    defense_hide();
}

static void start_demo_mode(void)
{
    game_mode = GAME_MODE_DEMO;
    gameover_restore_tiles();
    gameover_clear_text_tiles();
    restore_full_palette();
    starfield_init();
    fire_start_armed = false;
    laser_fire_held = false;
    shield_points = SHIELD_START;
    planet_surface_phase = false;
    draw_shield_bar();

    restore_title_tiles();
    asteroid_reset();
    laser_init();
    beasties_reset();
    mothership_reset();
    mothership_set_sfx_enabled(false);
    set_deep_space_terrain();
    update_launch_tube();
    asteroid_set_planet_phase(false);
    asteroid_set_spawns_paused(false);
    permanent_captures = 0;
    beasties_delivered = 0;
    full_rescue_bonus = false;

    opl_silence_all();
    sound_init();
    music_enabled = true;
    music_init(DEMO_MUSIC_FILENAME);
}

static void start_gameplay_mode(void)
{
    game_mode = GAME_MODE_PLAYING;
    shield_points = SHIELD_START;
    planet_timer = 0;
    planet_surface_phase = false;
    space_kills = 0;
    laser_fire_held = false;
    permanent_captures = 0;
    beasties_delivered = 0;
    game_level = 0;
    full_rescue_bonus = false;
    asteroid_set_level(0);
    defense_set_level(0);
    draw_shield_bar();
    fire_start_armed = false;

    hide_title_tiles();

    opl_silence_all();
    sound_init();
    music_enabled = false;
    game_music_started = false;

    lander_reset();
    asteroid_reset();
    laser_init();
    beasties_reset();
    mothership_reset_appear();
    mothership_set_sfx_enabled(true);
    mothership_consume_respawned_after_destruction(); // discard any stale demo respawn
    score_init();
    set_deep_space_terrain();
    update_launch_tube();
    asteroid_set_planet_phase(false);
    asteroid_set_spawns_paused(false);
}

static void apply_starting_tilemap_layout(void)
{
    uint8_t y;

    update_launch_tube();

    // Deal with corners.
    tilemap_write(MOTHERSHIP_MAP_TILEMAP_DATA, 0 , 0 , 0);
    tilemap_write(MOTHERSHIP_MAP_TILEMAP_DATA, 39, 29, 0);

    // // MAIN_MAP_TILEMAP_DATA: make (1,18) and (38,18) background.
    // tilemap_write(MAIN_MAP_TILEMAP_DATA, 1, 18, 87);
    // tilemap_write(MAIN_MAP_TILEMAP_DATA, 38, 18, 87);

    // // MAIN_MAP_TILEMAP_DATA: make edge columns background for rows 18-22.
    // for (y = 18; y <= 22; ++y) {
    //     tilemap_write(MAIN_MAP_TILEMAP_DATA, 0, y, 87);
    //     tilemap_write(MAIN_MAP_TILEMAP_DATA, 39, y, 87);
    // }

    // MAIN_MAP_TILEMAP_DATA: row 23 to index 87, row 24 to tile 1.
    tilemap_fill_row(MAIN_MAP_TILEMAP_DATA, 23, 87);
    tilemap_fill_row(MAIN_MAP_TILEMAP_DATA, 24, 1);

    // MAIN_MAP_TILEMAP_DATA: deep-space phase styles rows 23-24 only.
    set_deep_space_terrain();
}

static void init_graphics(void)
{
    // Select a 320x240 canvas
    if (xreg_vga_canvas(1) < 0) {
        puts("xreg_vga_canvas failed");
        return;
    }


    RIA.addr0 = PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 256; i++) {
        RIA.rw0 = tile_palette[i] & 0xFF;
        RIA.rw0 = tile_palette[i] >> 8;
    }

    // Start with one lit star color and rotate it over time.
    starfield_init();
    mothership_init();
    MAIN_MAP_CONFIG = SPRITE_DATA_END;

    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, width_tiles,  MAIN_MAP_WIDTH_TILES);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, height_tiles, MAIN_MAP_HEIGHT_TILES);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_data_ptr,    MAIN_MAP_TILEMAP_DATA); // tile ID grid
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_palette_ptr, PALETTE_ADDR);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_tile_ptr,    MAIN_MAP_DATA);        // tile bitmaps

    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=3 (8-bit color index) => 0b0011 = 3
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x03, MAIN_MAP_CONFIG, 0, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    MOTHERSHIP_CONFIG = MAIN_MAP_CONFIG + sizeof(vga_mode2_config_t);

    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_pos_px, -176);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, width_tiles,  MAIN_MAP_WIDTH_TILES);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, height_tiles, MAIN_MAP_HEIGHT_TILES);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, xram_data_ptr,    MOTHERSHIP_MAP_TILEMAP_DATA); // tile ID grid
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, xram_palette_ptr, PALETTE_ADDR);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, xram_tile_ptr,    MAIN_MAP_DATA);

    if (xreg_vga_mode(2, 0x03, MOTHERSHIP_CONFIG, 2, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    LASER_CONFIG = MOTHERSHIP_CONFIG + sizeof(vga_mode2_config_t);

    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, x_pos_px, 0);
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, y_pos_px, 0);
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LASER_DATA);
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, log_size, LASER_SPRITE_LOG_SIZE);
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    ASTEROID_CONFIG = LASER_CONFIG + sizeof(vga_mode4_sprite_t);

    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, x_pos_px, 0);
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, y_pos_px, 0);
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, ASTEROID_DATA);
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, log_size, 3); // 8x8 sprites  
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    BEASTIE1_CONFIG = ASTEROID_CONFIG + sizeof(vga_mode4_sprite_t);

    xram0_struct_set(BEASTIE1_CONFIG, vga_mode4_sprite_t, x_pos_px, -32);
    xram0_struct_set(BEASTIE1_CONFIG, vga_mode4_sprite_t, y_pos_px, -32);
    xram0_struct_set(BEASTIE1_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, BEASTIES_DATA);
    xram0_struct_set(BEASTIE1_CONFIG, vga_mode4_sprite_t, log_size, 3); // 8x8 sprites
    xram0_struct_set(BEASTIE1_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    BEASTIE2_CONFIG = BEASTIE1_CONFIG + sizeof(vga_mode4_sprite_t);

    xram0_struct_set(BEASTIE2_CONFIG, vga_mode4_sprite_t, x_pos_px, -32);
    xram0_struct_set(BEASTIE2_CONFIG, vga_mode4_sprite_t, y_pos_px, -32);
    xram0_struct_set(BEASTIE2_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, BEASTIES_DATA);
    xram0_struct_set(BEASTIE2_CONFIG, vga_mode4_sprite_t, log_size, 3); // 8x8 sprites
    xram0_struct_set(BEASTIE2_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    LANDER_CONFIG = BEASTIE2_CONFIG + sizeof(vga_mode4_sprite_t);

    xram0_struct_set(LANDER_CONFIG, vga_mode4_sprite_t, x_pos_px, -32);
    xram0_struct_set(LANDER_CONFIG, vga_mode4_sprite_t, y_pos_px, -32);
    xram0_struct_set(LANDER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LANDER_DATA);
    xram0_struct_set(LANDER_CONFIG, vga_mode4_sprite_t, log_size, 4); // 16x16 sprites
    xram0_struct_set(LANDER_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    BEAM_CONFIG = LANDER_CONFIG + sizeof(vga_mode4_sprite_t);

    xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, x_pos_px, -32);
    xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, y_pos_px, -32);
    xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, BEAM_DATA);
    xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, log_size, 3); // 8x8 sprites
    xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    // Mode 4 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(4, 0, LASER_CONFIG, 6, 1, 0, SPACE_HEIGHT) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    backup_terrain_rows();
    apply_starting_tilemap_layout();


    printf("MAIN_MAP DATA: 0x%04X - 0x%04X\n", MAIN_MAP_DATA, MAIN_MAP_DATA + MAIN_MAP_DATA_SIZE);
    printf("MAIN_MAP TILEMAP: 0x%04X - 0x%04X\n", MAIN_MAP_TILEMAP_DATA, MAIN_MAP_TILEMAP_DATA + MAIN_MAP_TILEMAP_SIZE);
    printf("MAIN_MAP_CONFIG: 0x%04X\n", MAIN_MAP_CONFIG);
    printf("MOTHERSHIP_CONFIG: 0x%04X\n", MOTHERSHIP_CONFIG);

}

void process_audio_frame(void) {
    if (!music_enabled) return;
    
    timer_accumulator += SONG_HZ;
    while (timer_accumulator >= 60) {
        update_music();
        timer_accumulator -= 60;
    }
}

static bool game_over_if_shields_depleted(void)
{
    if (shield_points > 0)
        return false;

    start_game_over_sequence();
    return true;
}

int main(void)
{
    puts("Hello from Cosmic Arc!");

    // Match RPMegaRacer input setup: map RIA keyboard/gamepad streams to XRAM buffers.
    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);

    init_graphics();
    mothership_reset();
    laser_init();
    asteroid_init();
    beasties_init();
    lander_init();
    score_init();
    init_input_system();

    // Audio Setup
    OPL_Config(1, OPL_ADDR);
    opl_init();
    sound_init();
    backup_title_tiles();
    start_demo_mode();

    while (true) {
        // Main game loop
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        // 2. AUDIO
        process_audio_frame();

        // Get Input from Keyboard/Gamepad
        handle_input();

        if (game_mode == GAME_MODE_DEMO) {
            bool fire_pressed = is_action_pressed(0, ACTION_FIRE);

            // Debounce the start action: require a press, then a release.
            if (fire_pressed) {
                fire_start_armed = true;
            } else if (fire_start_armed) {
                start_gameplay_mode();
            }
        }

        if (game_mode == GAME_MODE_GAME_OVER) {
            if (gameover_stage == 0) {
                mothership_update();
                if (mothership_consume_respawned_after_destruction()) {
                    gameover_replace_tiles();
                    gameover_write_text_tiles();
                    gameover_palette_blackout();
                    gameover_init_colors();
                    // Start lander escape from screen centre
                    gameover_escape_x          = GAMEOVER_ESCAPE_START_X;
                    gameover_escape_y          = GAMEOVER_ESCAPE_START_Y;
                    gameover_escape_zigzag_dir  = 1;
                    gameover_escape_zigzag_tick = 0;
                    gameover_escape_move_tick   = 0;
                    gameover_escape_active      = true;
                    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_pos_px, 16);
                    lander_place(gameover_escape_x, gameover_escape_y);
                    sound_set_lander_motor(true);
                    gameover_stage      = 1;
                    gameover_anim_step  = 0;
                    gameover_anim_tick  = 0;
                    gameover_timer      = 0;
                }
            } else {
                // Drive lander escape animation
                if (gameover_escape_active) {
                    // Advance position every GAMEOVER_ESCAPE_MOVE_TICKS frames
                    if (++gameover_escape_move_tick >= GAMEOVER_ESCAPE_MOVE_TICKS) {
                        gameover_escape_move_tick = 0;
                        gameover_escape_y -= 2;
                        gameover_escape_x += (int16_t)(gameover_escape_zigzag_dir * 3 + 1);
                        if (++gameover_escape_zigzag_tick >= GAMEOVER_ESCAPE_ZIGZAG_PERIOD) {
                            gameover_escape_zigzag_tick = 0;
                            gameover_escape_zigzag_dir  = (int8_t)(-gameover_escape_zigzag_dir);
                        }
                        if (gameover_escape_y < -16 ||
                            gameover_escape_x < -16 ||
                            gameover_escape_x > 320) {
                            // Off-screen — hide sprite and silence motor
                            lander_place(-32, -32);
                            sound_set_lander_motor(false);
                            gameover_escape_active = false;
                        } else {
                            lander_place(gameover_escape_x, gameover_escape_y);
                        }
                    }
                    sound_update(false, false);
                }

                if (++gameover_anim_tick >= GAMEOVER_ANIM_TICKS) {
                    gameover_anim_tick = 0;
                    if (gameover_stage == 1) {
                        gameover_write_frame_colors(gameover_anim_step);
                        if (++gameover_anim_step >= GAMEOVER_ANIM_FRAMES)
                            gameover_stage = 2;
                    } else {
                        gameover_rotate_colors();
                    }
                }
                ++gameover_timer;
                bool any = is_action_pressed(0, ACTION_FIRE)         ||
                           is_action_pressed(0, ACTION_THRUST)        ||
                           is_action_pressed(0, ACTION_REVERSE_THRUST)||
                           is_action_pressed(0, ACTION_ROTATE_LEFT)   ||
                           is_action_pressed(0, ACTION_ROTATE_RIGHT);
                if (any) {
                    gameover_exit_armed = true;
                } else if (gameover_exit_armed || gameover_timer >= GAMEOVER_TIMEOUT) {
                    start_demo_mode();
                }
            }
            continue;
        }

        if (game_mode == GAME_MODE_PLAYING && mothership_is_landed()) {
            if (!game_music_started) {
                game_music_started = true;
                music_init(GAME_MUSIC_FILENAME);
                music_enabled = true;
            }

            if (planet_surface_phase) {
                planet_timer++;
                if (planet_timer == 12 * 60) {
                    sound_play_klaxon();
                } else if (planet_timer == 16 * 60) {
                    if (!asteroid_is_active()) asteroid_force_spawn();
                }
            }

            bool fired = false;

            // Laser fires only when the direction button is freshly pressed (not held
            // from the previous shot) and no laser is already active on screen.
            if (!lander_is_active()) {
                bool any_dir = is_action_pressed(0, ACTION_THRUST) ||
                               (!planet_surface_phase && is_action_pressed(0, ACTION_REVERSE_THRUST)) ||
                               is_action_pressed(0, ACTION_ROTATE_LEFT) ||
                               is_action_pressed(0, ACTION_ROTATE_RIGHT);

                if (!any_dir) {
                    laser_fire_held = false;
                } else if (!laser_fire_held) {
                    if      (is_action_pressed(0, ACTION_THRUST))         fired = laser_fire(LASER_UP);
                    else if (is_action_pressed(0, ACTION_REVERSE_THRUST)) {
                        if (!planet_surface_phase) fired = laser_fire(LASER_DOWN);
                    }
                    else if (is_action_pressed(0, ACTION_ROTATE_LEFT))    fired = laser_fire(LASER_LEFT);
                    else if (is_action_pressed(0, ACTION_ROTATE_RIGHT))   fired = laser_fire(LASER_RIGHT);

                    if (fired) laser_fire_held = true;
                }
            }

            if (fired) {
                shield_points -= SHIELD_FIRE_LOSS;
                draw_shield_bar();
                if (game_over_if_shields_depleted())
                    continue;
            }
        }

        starfield_update();
        mothership_update();
        if (mothership_consume_departed()) {
            // Tiles swap NOW  — after the ship has cleared the screen.
            if (planet_surface_phase) {
                restore_terrain_rows();
                planet_timer = 0;
                beasties_spawn((uint8_t)(2u - permanent_captures));
                defense_reset();
            } else {
                set_deep_space_terrain();
            }
            update_launch_tube();
        }
        if (mothership_consume_respawned_after_destruction()) {
            if (game_mode == GAME_MODE_DEMO) {
                // Demo: use normal toggle (departure already bypassed by destruction).
                planet_surface_phase = !planet_surface_phase;
                if (!planet_surface_phase) {
                    defense_hide();
                    set_deep_space_terrain();
                } else restore_terrain_rows();
                update_launch_tube();
                asteroid_set_planet_phase(planet_surface_phase);
                asteroid_set_spawns_paused(false);
            } else if (planet_surface_phase) {
                // Destroyed on planet surface — return to deep space immediately;
                // no departure scroll since destruction already played.
                permanent_captures += beasties_delivered;
                beasties_delivered = 0;
                if (permanent_captures >= 2) permanent_captures = 0;
                planet_surface_phase = false;
                defense_hide();
                set_deep_space_terrain();
                update_launch_tube();
                beasties_hide_all();
                asteroid_set_planet_phase(false);
                asteroid_set_spawns_paused(false);
            }
            // In gameplay deep space: ship respawns in deep space, no change needed.
        }
        laser_update();
        {
            bool on_planet = planet_surface_phase && mothership_is_landed();
            bool smart = (game_mode == GAME_MODE_PLAYING) && on_planet;
            beasties_update(on_planet, smart, 0);
        }
        lander_update(planet_surface_phase && mothership_is_landed() && game_mode == GAME_MODE_PLAYING);
        if (planet_surface_phase && mothership_is_landed() && game_mode == GAME_MODE_PLAYING) {
            if (defense_update())
                lander_respawn();
        }

        if (game_mode == GAME_MODE_PLAYING && planet_surface_phase) {
            // Accumulate beasties docked this visit; fully captured only when we leave
            uint8_t docked;
            if (lander_consume_docked_beasties(&docked)) {
                beasties_delivered += docked;
            }

            // Return to space when enough are docked and alarm hasn't fired
            if ((uint8_t)(beasties_delivered + permanent_captures) >= 2u
                    && planet_timer < 12 * 60) {
                full_rescue_bonus = (beasties_delivered >= 2u);
                lander_reset();
                toggle_surface_phase();
            }
        }

        if (mothership_is_landed()) {
            AsteroidResult result = asteroid_update();
            if (result == ASTEROID_LASER_HIT) {
                score_add(10);
                if (game_mode == GAME_MODE_PLAYING && shield_points < SHIELD_START) {
                    ++shield_points;
                    draw_shield_bar();
                }

                if (game_mode == GAME_MODE_PLAYING && !planet_surface_phase) {
                    // Count kills in deep space; transition to planet after 8.
                    if (++space_kills >= 8) {
                        space_kills = 0;
                        toggle_surface_phase();
                    }
                } else if (planet_surface_phase) {
                    toggle_surface_phase();
                }
            } else if (result == ASTEROID_MOTHERSHIP_HIT) {
                if (game_mode == GAME_MODE_PLAYING) {
                    shield_points -= SHIELD_HIT_LOSS;
                    draw_shield_bar();
                    if (game_over_if_shields_depleted())
                        continue;

                    sound_play_destruction();
                }

                mothership_start_destruction();
                asteroid_reset();
                laser_init();
                beasties_delivered = 0; // beasties aboard are lost with the ship
            }
        }

        if (game_mode == GAME_MODE_PLAYING) {
            sound_update(mothership_is_descending(), asteroid_is_active());
        }

        }
    return 0;
}