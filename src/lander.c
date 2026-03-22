#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "lander.h"
#include "input.h"
#include "palette.h"
#include "beasties.h"
#include "sound.h"

#define LANDER_START_X 152
#define LANDER_START_Y (112 + 3) 
#define LANDER_SPEED 1

// Allowed movement zones (8px tiles, 16x16 sprite top-left bounds)
// Surface: tiles (0,13)-(39,21) — narrowed 12px each side to keep clear of tower cols 0/39
#define DEATH_FRAME_BASE   4   // first death sprite frame index
#define DEATH_FRAME_COUNT  3   // frames 4, 5, 6
#define DEATH_FRAME_TICKS  8   // display ticks per death frame

#define ZONE_SURFACE_X_MIN  12
#define ZONE_SURFACE_X_MAX  (SCREEN_WIDTH - 16 - 12)  // 292: right edge stays clear of col 39
#define ZONE_SURFACE_Y_MIN  (15 * 8) - 0              // 120: top of row 15
#define ZONE_SURFACE_Y_MAX  (22 * 8 - 0)              // 176: keeps sprite within row 22
// Launch tube: tiles (19,11)-(20,12) — exactly 16px wide, one sprite wide
#define ZONE_TUBE_X         (19 * 8)                  // 152
#define ZONE_TUBE_Y_MIN     (14 * 8 + 3)              // 116: dock/start position
#define ZONE_TUBE_Y_MAX     (15 * 8 - 1)              // 119: bottom of row 14

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
#define BEAM_GREEN_ROW_START      22    // only the bottom row uses green tiles
#define BEAM_TILE_ROW_END         22    // BEASTIE_GROUND_Y=191 → screen row 22 ((191-16)/8=21.9)
#define BEAM_FRAME_SIZE          (8u * 8u * 2u)   // bytes per 8x8 sprite frame
#define BEAM_ANIM_TICKS           6
#define BEAM_FLICKER_TICKS        2
#define BEAM_PALETTE_COUNT        8
#define BEAM_FLICKER_COUNT       16
#define BEAM_PALETTE_BLUE       105
#define BEAM_PALETTE_GREEN      125

static const uint8_t beam_blue_pal[BEAM_PALETTE_COUNT]  = { 9, 25, 41, 57, 73, 89, 105, 121 };
static const uint8_t beam_green_pal[BEAM_PALETTE_COUNT] = { 13, 29, 45, 61, 77, 93, 109, 125 };

// Flicker pattern: 1=on, 0=off. Mostly on with short semi-random off bursts.
static const uint8_t beam_flicker[BEAM_FLICKER_COUNT] = {
    1,1,1,1,0,1,1,1,1,1,0,1,1,0,1,1
};

// sub-tile offset → left column tile index and right column tile index (0xFF = none)
static const uint8_t beam_left_idx[8]  = { 3, 4, 5, 6, 7, 8, 9, 10 };
static const uint8_t beam_right_idx[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 1, 2 };

static int16_t lander_x;
static int16_t lander_y;
static bool lander_active;
static bool    lander_dying;
static uint8_t death_frame;
static uint8_t death_tick;
static uint8_t launch_delay;
static uint8_t anim_tick;
static uint8_t frame;

static bool    beam_active;
static bool    beam_flicker_on;
static int16_t prev_beam_left_px;   // -1 = no beam currently drawn
static uint8_t prev_beam_row_start;
static bool    prev_had_right;
static uint8_t beam_anim_frame;
static uint8_t beam_anim_tick;
static uint8_t beam_flicker_tick;
static uint8_t beam_pal_phase;
static bool    beam_has_beastie;
static uint8_t beam_beastie_idx;
static int16_t beam_beastie_y;
static uint8_t beasties_aboard;       // beamed up but not yet docked
static uint8_t aboard_indices[2];     // beastie sprite index for each aboard slot
static uint8_t lander_docked_delivery; // set on dock, consumed once by main.c
static bool lander_docking_pending;   // reached top; waiting for thrust release to dock

static void beam_palette_write(uint8_t index, uint16_t color)
{
    RIA.addr0 = PALETTE_ADDR + ((unsigned)index * 2u);
    RIA.step0 = 1;
    RIA.rw0 = color & 0xFF;
    RIA.rw0 = color >> 8;
}

static void beam_apply_palette(bool on)
{
    uint16_t blue_color  = on ? tile_palette[BEAM_PALETTE_BLUE]  : 0u;
    uint16_t green_color = on ? tile_palette[BEAM_PALETTE_GREEN] : 0u;
    beam_palette_write(BEAM_PALETTE_BLUE,  blue_color);
    beam_palette_write(BEAM_PALETTE_GREEN, green_color);
}

static void beam_restore_palette(void)
{
    beam_palette_write(BEAM_PALETTE_BLUE,  tile_palette[BEAM_PALETTE_BLUE]);
    beam_palette_write(BEAM_PALETTE_GREEN, tile_palette[BEAM_PALETTE_GREEN]);
}

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

    {
        // If a beastie is rising, clear that tile row so the beam sprite shows through.
        // beam_beastie_y is a screen pixel y; convert to tilemap row by subtracting
        // the MOTHERSHIP tilemap y_pos_px offset (MOTHERSHIP_LAND_Y = 16).
        uint8_t sprite_row = beam_has_beastie
            ? (uint8_t)((int16_t)(beam_beastie_y - MOTHERSHIP_LAND_Y) / 8)
            : 0xFFu;
        for (row = row_start; row <= BEAM_TILE_ROW_END; ++row) {
            uint8_t lt = (row == sprite_row) ? 0u : beam_tile_id(left_idx, row);
            uint8_t rt = (row == sprite_row) ? 0u : beam_tile_id(right_idx, row);
            beam_tile_write(tile_col, row, lt);
            if (has_right)
                beam_tile_write(tile_col + 1u, row, rt);
        }
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
        if (beam_has_beastie) {
            beasties_set_paused(beam_beastie_idx, false);
            beam_has_beastie = false;
        }
        beam_erase();
        xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, x_pos_px, -32);
        xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, y_pos_px, -32);
    }
    beam_active = false;
    beam_flicker_on = false;
    beam_has_beastie = false;
    sound_set_lander_motor(false);
    sound_set_beam(false);
    {
        uint8_t i;
        for (i = 0; i < beasties_aboard; ++i)
            beasties_set_paused(aboard_indices[i], false);
    }
    beasties_aboard = 0;
    lander_docked_delivery = 0;
    prev_beam_left_px = -1;
    beam_anim_frame = 0;
    beam_anim_tick = 0;
    beam_flicker_tick = 0;
    beam_pal_phase = 0;
    beam_restore_palette();

    lander_x = LANDER_START_X;
    lander_y = LANDER_START_Y;
    lander_active = false;
    lander_dying = false;
    lander_docking_pending = false;
    death_frame = 0;
    death_tick = 0;
    launch_delay = 0;
    anim_tick = 0;
    frame = 0;
    write_lander_pos(-32, -32);
}

bool lander_is_active(void)
{
    return lander_active && !lander_dying;
}

void lander_update(bool planet_phase)
{
    if (!planet_phase) {
        if (lander_active || lander_dying || launch_delay > 0) lander_reset();
        return;
    }

    if (lander_dying) {
        if (++death_tick >= DEATH_FRAME_TICKS) {
            death_tick = 0;
            if (++death_frame >= DEATH_FRAME_COUNT) {
                // Animation complete — actually respawn.
                lander_reset();
                lander_active = true;
                lander_y = LANDER_START_Y + 8;
                write_lander_pos(lander_x, lander_y);
            } else {
                write_lander_frame((uint8_t)(DEATH_FRAME_BASE + death_frame));
            }
        }
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
        // Active movement constrained to allowed zones (locked when beam is on)
        if (!beam_active) {
            bool any_dir;
            int16_t new_x = lander_x;
            int16_t new_y = lander_y;
            if (is_action_pressed(0, ACTION_THRUST))         new_y -= LANDER_SPEED;
            if (is_action_pressed(0, ACTION_REVERSE_THRUST)) new_y += LANDER_SPEED;
            if (is_action_pressed(0, ACTION_ROTATE_LEFT))    new_x -= LANDER_SPEED;
            if (is_action_pressed(0, ACTION_ROTATE_RIGHT))   new_x += LANDER_SPEED;

            any_dir = (new_x != lander_x || new_y != lander_y);
            sound_set_lander_motor(any_dir);

            if (lander_in_zone(new_x, new_y)) {
                lander_x = new_x;
                lander_y = new_y;
            } else {
                // Slide along boundaries by trying each axis independently
                if (lander_in_zone(new_x, lander_y)) lander_x = new_x;
                if (lander_in_zone(lander_x, new_y)) lander_y = new_y;
            }
        } else {
            sound_set_lander_motor(false);
        }

        // Dock when returned to the top of the launch tube — wait for thrust release
        if (lander_y <= ZONE_TUBE_Y_MIN)
            lander_docking_pending = true;
        if (lander_docking_pending && !is_action_pressed(0, ACTION_THRUST)) {
            lander_active = false;
            lander_docking_pending = false;
            launch_delay = 0;
            sound_set_lander_motor(false);
            sound_set_beam(false);
            // Beasties aboard transfer to delivery slot — consumed once by main.c
            lander_docked_delivery = beasties_aboard;
            beasties_aboard = 0;
        }

        // Beam: hold FIRE button while on the surface to project beam to ground
        if (lander_y >= ZONE_SURFACE_Y_MIN && is_action_pressed(0, ACTION_FIRE)) {
            if (!beam_active) {
                beam_active = true;
                beam_pal_phase = 0;
                beam_flicker_on = true;
                beam_apply_palette(true);
                sound_set_beam(true);
            }
            // Every frame: try to lock onto a beastie walking under the beam
            if (!beam_has_beastie) {
                uint8_t i;
                for (i = 0; i < 2u; ++i) {
                    int16_t bx = beasties_get_x(i);
                    if (bx >= 0) {
                        int16_t diff = (bx + 4) - (lander_x + 8);
                        if (diff < 0) diff = -diff;
                        if (diff <= 8) {
                            beam_has_beastie = true;
                            beam_beastie_idx = i;
                            // Start tile-aligned at the bottom beam row so the sprite
                            // always occupies exactly one tile row as it rises.
                            beam_beastie_y = (int16_t)(MOTHERSHIP_LAND_Y + BEAM_TILE_ROW_END * 8);
                            beasties_set_paused(i, true);
                            break;
                        }
                    }
                }
            }
            beam_draw_tiles();
            if (++beam_anim_tick >= BEAM_ANIM_TICKS) {
                beam_anim_tick = 0;
                if (++beam_anim_frame >= 4u) {
                    beam_anim_frame = 0;
                    if (beam_has_beastie) {
                        beam_beastie_y -= 8;
                        if (beam_beastie_y <= lander_y + 8) {
                            // Beastie reaches the lander — now aboard
                            aboard_indices[beasties_aboard] = beam_beastie_idx;
                            ++beasties_aboard;
                            beam_has_beastie = false;
                            sound_play_beastie_aboard();
                        }
                    }
                }
            }
            if (++beam_flicker_tick >= BEAM_FLICKER_TICKS) {
                beam_flicker_tick = 0;
                if (++beam_pal_phase >= BEAM_FLICKER_COUNT) beam_pal_phase = 0;
                beam_flicker_on = (bool)beam_flicker[beam_pal_phase];
                beam_apply_palette(beam_flicker_on);
            }
            // BEAM_CONFIG sprite is only visible while a beastie is rising.
            // When idle (no beastie), the tile beam is sufficient — the sprite
            // must be hidden or it peeks below the last tile row.
            if (beam_has_beastie) {
                unsigned beam_ptr = BEAM_DATA + (unsigned)beam_anim_frame * BEAM_FRAME_SIZE;
                xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, beam_ptr);
                xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, x_pos_px, (int16_t)(lander_x + 4));
                xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, y_pos_px, beam_beastie_y);
            } else {
                xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, x_pos_px, -32);
                xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, y_pos_px, -32);
            }
        } else if (beam_active) {
            beam_active = false;
            beam_flicker_on = false;
            sound_set_beam(false);
            if (beam_has_beastie) {
                beasties_set_paused(beam_beastie_idx, false);
                beam_has_beastie = false;
            }
            beam_erase();
            beam_restore_palette();
            xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, x_pos_px, -32);
            xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, y_pos_px, -32);
        }
    }

    if (++anim_tick >= 2) {
        anim_tick = 0;
        frame ^= 1u;
        write_lander_frame((beam_active && beam_flicker_on) ? (frame + 2u) : frame);
    }

    if (lander_active) {
        write_lander_pos(lander_x, lander_y);
    } else {
        write_lander_pos(-32, -32);
    }
}

void lander_get_pos(int16_t *x, int16_t *y)
{
    *x = lander_x;
    *y = lander_y;
}

void lander_respawn(void)
{
    // Stop sounds and beam immediately but keep sprite visible for death animation.
    sound_set_lander_motor(false);
    sound_set_beam(false);
    if (beam_active) {
        if (beam_has_beastie) {
            beasties_set_paused(beam_beastie_idx, false);
            beam_has_beastie = false;
        }
        beam_erase();
        beam_restore_palette();
        xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, x_pos_px, -32);
        xram0_struct_set(BEAM_CONFIG, vga_mode4_sprite_t, y_pos_px, -32);
        beam_active = false;
    }
    lander_dying = true;
    death_frame = 0;
    death_tick = 0;
    write_lander_frame(DEATH_FRAME_BASE);
    sound_play_lander_death();
}

bool lander_is_beam_active(void)
{
    return beam_active;
}

bool lander_consume_docked_beasties(uint8_t *count)
{
    if (lander_docked_delivery == 0) return false;
    *count = lander_docked_delivery;
    lander_docked_delivery = 0;
    return true;
}