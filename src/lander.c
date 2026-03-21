#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "lander.h"
#include "input.h"

#define LANDER_START_X 152
#define LANDER_START_Y 88
#define LANDER_SPEED 1

// Allowed movement zones (8px tiles, 16x16 sprite top-left bounds)
// Surface: tiles (0,13)-(39,21)
#define ZONE_SURFACE_X_MIN  0
#define ZONE_SURFACE_X_MAX  (SCREEN_WIDTH - 16)  // 304: right edge of col 39
#define ZONE_SURFACE_Y_MIN  (14 * 8) - 2            // 112: top of row 14
#define ZONE_SURFACE_Y_MAX  (22 * 8 - 16)        // 160: keeps sprite within row 21
// Launch tube: tiles (19,11)-(20,12) — exactly 16px wide, one sprite wide
#define ZONE_TUBE_X         (19 * 8)             // 152
#define ZONE_TUBE_Y_MIN     (11 * 8)             // 88: dock/start position
#define ZONE_TUBE_Y_MAX     (14 * 8 - 3)         // 111: bottom of row 13

// Beam tile layout: 4px-wide beam at 8 sub-pixel offsets within an 8px tile column.
// Blue set (main beam): tile IDs BEAM_FIRST_BLUE + index (0-10)
// Green set (ground contact): tile IDs BEAM_FIRST_GREEN + index (0-10)
// Tile set indices:
//   0:[xooooooo] 1:[xxoooooo] 2:[xxxooooo]  <- right-column overflow (offsets 7,6,5)
//   3:[xxxxoooo] 4:[oxxxxooo] 5:[ooxxxxoo]  <- full beam, offsets 0-2
//   6:[oooxxxxo] 7:[ooooxxxx]               <- full beam, offsets 3-4
//   8:[oooooxxx] 9:[ooooooxx] 10:[ooooooox] <- left-column overflow (offsets 5,6,7)
#define BEAM_FIRST_BLUE          191u
#define BEAM_FIRST_GREEN         202u
#define BEAM_GREEN_ROW_START      20    // rows >= 20 use green tiles (near ground)
#define BEAM_TILE_ROW_END         21    // last MOTHERSHIP_MAP row ((191-16)/8 = 21)
#define BEAM_FRAME_SIZE          (8u * 8u * 2u)   // bytes per 8x8 sprite frame
#define BEAM_ANIM_TICKS           4

// sub-tile offset → left column tile index and right column tile index (0xFF = none)
static const uint8_t beam_left_idx[8]  = { 3, 4, 5, 6, 7, 8, 9, 10 };
static const uint8_t beam_right_idx[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 1, 2 };

static int16_t lander_x;
static int16_t lander_y;
static bool lander_active;
static uint8_t launch_delay;
static uint8_t anim_tick;
static uint8_t frame;

static bool    beam_active;
static int16_t prev_beam_left_px;   // -1 = no beam currently drawn
static uint8_t prev_beam_row_start;
static bool    prev_had_right;
static uint8_t beam_anim_frame;
static uint8_t beam_anim_tick;

static bool lander_in_zone(int16_t x, int16_t y)
{
    if (x >= ZONE_SURFACE_X_MIN && x <= ZONE_SURFACE_X_MAX &&
        y >= ZONE_SURFACE_Y_MIN && y <= ZONE_SURFACE_Y_MAX)
        return true;
    if (x == ZONE_TUBE_X && y >= ZONE_TUBE_Y_MIN && y <= ZONE_TUBE_Y_MAX)
        return true;
    return false;
}

static void beam_tile_write(uint8_t tx, uint8_t ty, uint8_t tile)
{
    RIA.addr0 = MOTHERSHIP_MAP_TILEMAP_DATA + (unsigned)ty * MAIN_MAP_WIDTH_TILES + tx;
    RIA.step0 = 1;
    RIA.rw0 = tile;
}

static uint8_t beam_tile_id(uint8_t idx, uint8_t row)
{
    unsigned base = (row >= BEAM_GREEN_ROW_START) ? BEAM_FIRST_GREEN : BEAM_FIRST_BLUE;
    return (uint8_t)(base + idx);
}

static void beam_erase(void)
{
    uint8_t col, row;
    if (prev_beam_left_px < 0) return;
    col = (uint8_t)((uint16_t)prev_beam_left_px / 8u);
    for (row = prev_beam_row_start; row <= BEAM_TILE_ROW_END; ++row)
        beam_tile_write(col, row, 0);
    if (prev_had_right) {
        for (row = prev_beam_row_start; row <= BEAM_TILE_ROW_END; ++row)
            beam_tile_write(col + 1u, row, 0);
    }
    prev_beam_left_px = -1;
}

static void beam_draw_tiles(void)
{
    int16_t beam_left = lander_x + 6;
    uint8_t tile_col  = (uint8_t)((uint16_t)beam_left / 8u);
    uint8_t offset    = (uint8_t)((uint16_t)beam_left % 8u);
    uint8_t row_start = (uint8_t)((uint16_t)lander_y / 8u);
    uint8_t left_idx  = beam_left_idx[offset];
    uint8_t right_idx = beam_right_idx[offset];
    bool    has_right = (right_idx != 0xFF);
    uint8_t row;

    if (prev_beam_left_px != beam_left || prev_beam_row_start != row_start)
        beam_erase();

    for (row = row_start; row <= BEAM_TILE_ROW_END; ++row) {
        beam_tile_write(tile_col, row, beam_tile_id(left_idx, row));
        if (has_right)
            beam_tile_write(tile_col + 1u, row, beam_tile_id(right_idx, row));
    }
    prev_beam_left_px   = beam_left;
    prev_beam_row_start = row_start;
    prev_had_right      = has_right;
}

static void write_lander_pos(int16_t x, int16_t y)
{
    xram0_struct_set(LANDER_CONFIG, vga_mode4_sprite_t, x_pos_px, x);
    xram0_struct_set(LANDER_CONFIG, vga_mode4_sprite_t, y_pos_px, y);
}

static void write_lander_frame(uint8_t f)
{
    unsigned ptr = LANDER_DATA + ((unsigned)f * 16u * 16u * 2u);
    xram0_struct_set(LANDER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, ptr);
}

void lander_init(void)
{
    lander_reset();
}

void lander_reset(void)
{
    if (beam_active) {
        beam_erase();
        xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, x_pos_px, -32);
        xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, y_pos_px, -32);
    }
    beam_active = false;
    prev_beam_left_px = -1;
    beam_anim_frame = 0;
    beam_anim_tick = 0;

    lander_x = LANDER_START_X;
    lander_y = LANDER_START_Y;
    lander_active = false;
    launch_delay = 0;
    anim_tick = 0;
    frame = 0;
    write_lander_pos(-32, -32);
}

bool lander_is_active(void)
{
    return lander_active;
}

void lander_update(bool planet_phase)
{
    if (!planet_phase) {
        if (lander_active || launch_delay > 0) lander_reset();
        return;
    }

    if (!lander_active) {
        // Parked - hold DOWN for 0.5s (30 ticks) to release the lander
        if (is_action_pressed(0, ACTION_REVERSE_THRUST)) {
            if (++launch_delay >= 30) {
                lander_active = true;
                lander_x = LANDER_START_X;
                lander_y = LANDER_START_Y;
            }
        } else {
            launch_delay = 0;
        }
    } else {
        // Active movement constrained to allowed zones
        int16_t new_x = lander_x;
        int16_t new_y = lander_y;
        if (is_action_pressed(0, ACTION_THRUST))         new_y -= LANDER_SPEED;
        if (is_action_pressed(0, ACTION_REVERSE_THRUST)) new_y += LANDER_SPEED;
        if (is_action_pressed(0, ACTION_ROTATE_LEFT))    new_x -= LANDER_SPEED;
        if (is_action_pressed(0, ACTION_ROTATE_RIGHT))   new_x += LANDER_SPEED;

        if (lander_in_zone(new_x, new_y)) {
            lander_x = new_x;
            lander_y = new_y;
        } else {
            // Slide along boundaries by trying each axis independently
            if (lander_in_zone(new_x, lander_y)) lander_x = new_x;
            if (lander_in_zone(lander_x, new_y)) lander_y = new_y;
        }

        // Dock when returned to the top of the launch tube
        if (lander_y <= ZONE_TUBE_Y_MIN) {
            lander_active = false;
            launch_delay = 0;
        }

        // Beam: hold FIRE button while on the surface to project beam to ground
        if (lander_y >= ZONE_SURFACE_Y_MIN && is_action_pressed(0, ACTION_FIRE)) {
            beam_active = true;
            beam_draw_tiles();
            if (++beam_anim_tick >= BEAM_ANIM_TICKS) {
                beam_anim_tick = 0;
                if (++beam_anim_frame >= 4u) beam_anim_frame = 0;
            }
            unsigned beam_ptr = BEAM_DATA + (unsigned)beam_anim_frame * BEAM_FRAME_SIZE;
            xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, beam_ptr);
            xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, x_pos_px, (int16_t)(lander_x + 4));
            xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, y_pos_px, (int16_t)(BEASTIE_GROUND_Y + 1));
        } else if (beam_active) {
            beam_active = false;
            beam_erase();
            xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, x_pos_px, -32);
            xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, y_pos_px, -32);
        }
    }

    if (++anim_tick >= 4) {
        anim_tick = 0;
        frame ^= 1u;
        write_lander_frame(beam_active ? (frame + 2u) : frame);
    }

    if (lander_active) {
        write_lander_pos(lander_x, lander_y);
    } else {
        write_lander_pos(-32, -32);
    }
}