#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "defense.h"
#include "lander.h"

// Tower top rises from the bottom of row 22 to the top of row 18.
// As the tower sinks, rows above the current top get cleared to BG_TILE.
// Gun (col 1 / col 38) is always at the tower top row, tracking the sub-pixel.
// Pulse sweeps cols 1-38 at the current gun row when fired.

#define TOWER_ROW0        18
#define TOWER_ROW1        22
#define TOWER_COL_L        0
#define TOWER_COL_R       39
#define GUN_COL_L          1
#define GUN_COL_R         38
#define TOWER_TILE_BASE   27   // 8 sub-pixel frames (tiles 27-34)
#define GUN_L_TILE_BASE   35   // 7 frames (35-41); frame 0 = gun at tile top
#define GUN_R_TILE_BASE   42   // 7 frames (42-48)
#define PULSE_TILE_BASE   49   // 8 frames (49-56), frozen at fire moment
#define BG_TILE           87

#define TOWER_SUBPIXELS    8   // sub-pixel steps per tile row
// phase 0  = top of row 18 (fully extended)
// phase 39 = bottom of row 22 (fully retracted)
#define TOWER_PHASE_COUNT 40   // (TOWER_ROW1-TOWER_ROW0+1) * TOWER_SUBPIXELS
#define GUN_FRAME_COUNT    7   // frames 0-6; clamped from subpixels 0-7
#define TOWER_ANIM_TICKS   4   // frames per phase step
#define PULSE_PERIOD      180  // ~3 s at 60 Hz between pulses
#define PULSE_DURATION     20  // frames the pulse is visible

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

static uint8_t get_top_row(void)
{
    return (uint8_t)(TOWER_ROW0 + tower_phase / TOWER_SUBPIXELS);
}

static uint8_t get_subpixel(void)
{
    return (uint8_t)(tower_phase % TOWER_SUBPIXELS);
}

// Gun frame 0 = gun at top of tile, frame 6 = gun at bottom; tracks sub-pixel.
static uint8_t get_gun_frame(void)
{
    uint8_t sp = get_subpixel();
    return (sp < GUN_FRAME_COUNT) ? sp : (uint8_t)(GUN_FRAME_COUNT - 1u);
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
    uint8_t gf = get_gun_frame();
    uint8_t row;

    // Clear all rows the gun could previously occupy, then draw at current top.
    for (row = TOWER_ROW0; row <= TOWER_ROW1; ++row) {
        dtile(GUN_COL_L, row, BG_TILE);
        dtile(GUN_COL_R, row, BG_TILE);
    }
    dtile(GUN_COL_L, top_row, (uint8_t)(GUN_L_TILE_BASE + gf));
    dtile(GUN_COL_R, top_row, (uint8_t)(GUN_R_TILE_BASE + gf));
}

void defense_init(void)
{
    tower_phase = 0;   // start fully extended
    tower_tick = 0;
    tower_dir = true;  // begin sinking
    pulse_cooldown = PULSE_PERIOD;
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
    // Clear the center of any active pulse sweep.
    if (pulse_timer > 0) {
        for (col = GUN_COL_L + 1u; col < GUN_COL_R; ++col)
            dtile(col, pulse_row, BG_TILE);
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
        if ((ly + 16) > (int16_t)((uint16_t)pulse_row * 8u) &&
            ly < (int16_t)(((uint16_t)pulse_row + 1u) * 8u))
            hit = true;
    }

    // Advance tower animation (triangle wave with explicit direction).
    if (++tower_tick >= TOWER_ANIM_TICKS) {
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
            uint8_t gf = get_gun_frame();
            uint8_t row, col;
            // Clear all gun rows (gun may have moved during pulse), then redraw.
            for (row = TOWER_ROW0; row <= TOWER_ROW1; ++row) {
                dtile(GUN_COL_L, row, BG_TILE);
                dtile(GUN_COL_R, row, BG_TILE);
            }
            dtile(GUN_COL_L, top_row, (uint8_t)(GUN_L_TILE_BASE + gf));
            dtile(GUN_COL_R, top_row, (uint8_t)(GUN_R_TILE_BASE + gf));
            // Clear the center columns of the pulse row.
            for (col = GUN_COL_L + 1u; col < GUN_COL_R; ++col)
                dtile(col, pulse_row, BG_TILE);
        }
    } else if (pulse_cooldown > 0) {
        --pulse_cooldown;
    } else {
        uint8_t col;
        // Fire: freeze pulse at current gun row and sub-pixel tile.
        pulse_row  = get_top_row();
        pulse_tile = (uint8_t)(PULSE_TILE_BASE + get_subpixel());
        for (col = GUN_COL_L; col <= GUN_COL_R; ++col)
            dtile(col, pulse_row, pulse_tile);
        pulse_timer    = PULSE_DURATION;
        pulse_cooldown = PULSE_PERIOD;
    }

    return hit;
}
