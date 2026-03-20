#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "lander.h"
#include "input.h"

#define LANDER_START_X 152
#define LANDER_START_Y 88
#define LANDER_SPEED 1

static int16_t lander_x;
static int16_t lander_y;
static bool lander_active;
static uint8_t launch_delay;
static uint8_t anim_tick;
static uint8_t frame;

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
        // Active movement
        if (is_action_pressed(0, ACTION_THRUST)) lander_y -= LANDER_SPEED;
        if (is_action_pressed(0, ACTION_REVERSE_THRUST)) lander_y += LANDER_SPEED;
        if (is_action_pressed(0, ACTION_ROTATE_LEFT)) lander_x -= LANDER_SPEED;
        if (is_action_pressed(0, ACTION_ROTATE_RIGHT)) lander_x += LANDER_SPEED;

        // Clamp lander position to screen boundaries
        if (lander_x < 0) lander_x = 0;
        if (lander_x > 320 - 16) lander_x = 320 - 16;
        if (lander_y < 0) lander_y = 0;
        if (lander_y > 240 - 16) lander_y = 240 - 16;

        // Check docking condition: Return to (151..153, <=88)
        if (lander_x >= 151 && lander_x <= 153 && lander_y <= 88) {
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