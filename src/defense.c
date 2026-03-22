#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "defense.h"
#include "lander.h"
#include "sound.h"

// Tower top rises from the bottom of row 22 to the top of row 18.
// As the tower sinks, rows above the current top get cleared to BG_TILE.
// Gun (col 1 / col 38) is always at the tower top row, tracking the sub-pixel.
// Pulse sweeps cols 1-38 at the current gun row when fired.

#define TOWER_ROW0        15
#define TOWER_ROW1        22
#define TOWER_COL_L        0
#define TOWER_COL_R       39
#define GUN_COL_L          1
#define GUN_COL_R         38
#define TOWER_TILE_BASE   27   // 8 sub-pixel frames (tiles 27-34)
#define PULSE_TILE_BASE   49   // 8 frames (49-56), frozen at fire moment
#define BG_TILE           87

#define TOWER_SUBPIXELS    8   // sub-pixel steps per tile row
// phase 0  = top of row 18 (fully extended)
// phase 39 = bottom of row 22 (fully retracted)
#define TOWER_PHASE_COUNT (TOWER_ROW1-TOWER_ROW0+1) * TOWER_SUBPIXELS
// Level-scaled parameters (min/max/per-level step)
#define TOWER_ANIM_TICKS_BASE       4   // frames per phase step (slowest)
#define TOWER_ANIM_TICKS_MIN        1   // fastest
#define TOWER_ANIM_TICKS_PER_LEVEL  1

#define PULSE_PERIOD_BASE       120  // ~2 s at 60 Hz between pulses
#define PULSE_PERIOD_MIN         30  // ~0.5 s
#define PULSE_PERIOD_PER_LEVEL   15

#define PULSE_DURATION     5   // frames the pulse is visible (~quick flash)

static uint8_t tower_anim_ticks = TOWER_ANIM_TICKS_BASE;
static uint8_t pulse_period     = PULSE_PERIOD_BASE;

// Gun tile lookup: index matches tower sub-pixel (0-7).
static const uint8_t gun_l_tiles[8] = {35, 213, 36, 37, 38, 39, 40, 41};
static const uint8_t gun_r_tiles[8] = {42, 214, 43, 44, 45, 46, 47, 48};

// tower_dir: false = rising (phase --), true = sinking (phase ++)
static uint8_t tower_phase;
static uint8_t tower_tick;
static bool    tower_dir;
static uint8_t pulse_cooldown;
static uint8_t pulse_timer;
static uint8_t pulse_tile;
static uint8_t pulse_row;   // row frozen at fire moment

static void dtile(uint8_t x, uint8_t y, uint8_t tile)
{
    RIA.addr0 = MAIN_MAP_TILEMAP_DATA + (unsigned)y * MAIN_MAP_WIDTH_TILES + x;
    RIA.step0 = 1;
    RIA.rw0 = tile;
}

// Pulse tiles go on the MOTHERSHIP_CONFIG layer so clearing to 0 = transparent,
// leaving the star tiles on the main map layer untouched underneath.
static void ptile(uint8_t x, uint8_t y, uint8_t tile)
{
    RIA.addr0 = MOTHERSHIP_MAP_TILEMAP_DATA + (unsigned)y * MAIN_MAP_WIDTH_TILES + x;
    RIA.step0 = 1;
    RIA.rw0 = tile;
}

static uint8_t get_top_row(void)
{
    return (uint8_t)(TOWER_ROW0 + tower_phase / TOWER_SUBPIXELS);
}

static uint8_t get_subpixel(void)
{
    return (uint8_t)(tower_phase % TOWER_SUBPIXELS);
}

static void draw_towers(void)
{
    uint8_t top_row = get_top_row();
    uint8_t tower_tile = (uint8_t)(TOWER_TILE_BASE + get_subpixel());
    uint8_t row;

    // Rows above tower top: clear to background.
    for (row = TOWER_ROW0; row < top_row; ++row) {
        dtile(TOWER_COL_L, row, BG_TILE);
        dtile(TOWER_COL_R, row, BG_TILE);
    }
    // Top tile: uses the sub-pixel frame.
    dtile(TOWER_COL_L, top_row, tower_tile);
    dtile(TOWER_COL_R, top_row, tower_tile);
    // Body tiles below: always the base frame (sub-pixel 0).
    for (row = top_row + 1u; row <= TOWER_ROW1; ++row) {
        dtile(TOWER_COL_L, row, TOWER_TILE_BASE);
        dtile(TOWER_COL_R, row, TOWER_TILE_BASE);
    }
}

static void draw_guns(void)
{
    uint8_t top_row = get_top_row();
    uint8_t sp = get_subpixel();
    uint8_t row;

    // Clear all rows the gun could previously occupy, then draw at current top.
    for (row = TOWER_ROW0; row <= TOWER_ROW1; ++row) {
        dtile(GUN_COL_L, row, BG_TILE);
        dtile(GUN_COL_R, row, BG_TILE);
    }
    dtile(GUN_COL_L, top_row, gun_l_tiles[sp]);
    dtile(GUN_COL_R, top_row, gun_r_tiles[sp]);
}

void defense_set_level(uint8_t level)
{
    uint8_t t = TOWER_ANIM_TICKS_BASE;
    uint8_t tdec = (uint8_t)(level * TOWER_ANIM_TICKS_PER_LEVEL);
    tower_anim_ticks = (t > TOWER_ANIM_TICKS_MIN + tdec)
                       ? (uint8_t)(t - tdec)
                       : TOWER_ANIM_TICKS_MIN;

    uint8_t pdec = (uint8_t)(level * PULSE_PERIOD_PER_LEVEL);
    pulse_period = (PULSE_PERIOD_BASE > PULSE_PERIOD_MIN + pdec)
                   ? (uint8_t)(PULSE_PERIOD_BASE - pdec)
                   : PULSE_PERIOD_MIN;
}

void defense_init(void)
{
    tower_phase = 0;   // start fully extended
    tower_tick = 0;
    tower_dir = true;  // begin sinking
    pulse_cooldown = pulse_period;
    pulse_timer = 0;
    pulse_tile = PULSE_TILE_BASE;
    pulse_row = TOWER_ROW0;
}

void defense_reset(void)
{
    defense_init();
    draw_towers();
    draw_guns();
}

void defense_hide(void)
{
    uint8_t row;
    uint8_t col;

    // Clear tower and gun columns for all tower rows.
    for (row = TOWER_ROW0; row <= TOWER_ROW1; ++row) {
        dtile(TOWER_COL_L, row, BG_TILE);
        dtile(TOWER_COL_R, row, BG_TILE);
        dtile(GUN_COL_L,   row, BG_TILE);
        dtile(GUN_COL_R,   row, BG_TILE);
    }
    // Clear the center of any active pulse sweep (MOTHERSHIP layer, tile 0 = transparent).
    // MOTHERSHIP layer y_pos_px=MOTHERSHIP_LAND_Y when landed, so subtract the offset.
    if (pulse_timer > 0) {
        for (col = GUN_COL_L; col <= GUN_COL_R; ++col)
            ptile(col, pulse_row - MOTHERSHIP_LAND_Y / 8u, 0);
    }
}

bool defense_update(void)
{
    int16_t lx, ly;
    bool hit = false;

    // Hit check: lander overlaps the pulse row while pulse is active.
    if (pulse_timer > 0 && lander_is_active()) {
        lander_get_pos(&lx, &ly);
        (void)lx; // horizontal zone restriction keeps lander within pulse range
        if ((ly + 5) > (int16_t)((uint16_t)pulse_row * 8u) &&
            ly < (int16_t)(((uint16_t)pulse_row + 1u) * 8u))
            hit = true;
    }

    // Advance tower animation (triangle wave with explicit direction).
    if (++tower_tick >= tower_anim_ticks) {
        tower_tick = 0;
        if (tower_dir) {
            if (tower_phase < TOWER_PHASE_COUNT - 1u) ++tower_phase;
            else tower_dir = false;
        } else {
            if (tower_phase > 0u) --tower_phase;
            else tower_dir = true;
        }
    }
    draw_towers();

    // Draw gun in sync with tower; skipped while pulse overwrites that row.
    if (pulse_timer == 0)
        draw_guns();

    // Pulse state machine.
    if (pulse_timer > 0) {
        --pulse_timer;
        if (pulse_timer == 0) {
            uint8_t top_row = get_top_row();
            uint8_t sp = get_subpixel();
            uint8_t row, col;
            // Clear all gun rows (gun may have moved during pulse), then redraw.
            for (row = TOWER_ROW0; row <= TOWER_ROW1; ++row) {
                dtile(GUN_COL_L, row, BG_TILE);
                dtile(GUN_COL_R, row, BG_TILE);
            }
            dtile(GUN_COL_L, top_row, gun_l_tiles[sp]);
            dtile(GUN_COL_R, top_row, gun_r_tiles[sp]);
            // Clear pulse row on MOTHERSHIP layer (tile 0 = transparent).
            for (col = GUN_COL_L; col <= GUN_COL_R; ++col)
                ptile(col, pulse_row - MOTHERSHIP_LAND_Y / 8u, 0);
        }
    } else if (pulse_cooldown > 0) {
        --pulse_cooldown;
    } else {
        uint8_t col;
        // Fire: draw pulse on MOTHERSHIP layer so clearing = transparent (stars preserved).
        // Subtract MOTHERSHIP layer y offset so pulse aligns with gun on MAIN_MAP layer.
        pulse_row  = get_top_row();
        pulse_tile = (uint8_t)(PULSE_TILE_BASE + get_subpixel());
        for (col = GUN_COL_L; col <= GUN_COL_R; ++col)
            ptile(col, pulse_row - MOTHERSHIP_LAND_Y / 8u, pulse_tile);
        pulse_timer    = PULSE_DURATION;
        pulse_cooldown = pulse_period;
        sound_play_defense_pulse();
    }

    return hit;
}
