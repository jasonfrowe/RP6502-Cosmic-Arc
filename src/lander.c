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

static int16_t lander_x;
static int16_t lander_y;
static bool lander_active;
static uint8_t launch_delay;
static uint8_t anim_tick;
static uint8_t frame;

static bool lander_in_zone(int16_t x, int16_t y)
{
    if (x >= ZONE_SURFACE_X_MIN && x <= ZONE_SURFACE_X_MAX &&
        y >= ZONE_SURFACE_Y_MIN && y <= ZONE_SURFACE_Y_MAX)
        return true;
    if (x == ZONE_TUBE_X && y >= ZONE_TUBE_Y_MIN && y <= ZONE_TUBE_Y_MAX)
        return true;
    return false;
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
    }

    if (++anim_tick >= 4) {
        anim_tick = 0;
        frame ^= 1u;
        write_lander_frame(frame);
    }

    if (lander_active) {
        write_lander_pos(lander_x, lander_y);
    } else {
        write_lander_pos(-32, -32);
    }
}