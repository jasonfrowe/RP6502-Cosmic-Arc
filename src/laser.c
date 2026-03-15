#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "laser.h"

#define LASER_SPEED      6
#define LASER_HALF_SIZE  16   // 32x32 sprite half-width

// Frame 0: left/right (horizontal beam)
// Frame 1: up/down   (vertical beam)
// Each frame: 32 * 32 * 2 bytes (16-bit RGB555)
#define LASER_FRAME_HORIZ  (LASER_DATA)
#define LASER_FRAME_VERT   (LASER_DATA + 32 * 32 * 2)

static int16_t laser_x;
static int16_t laser_y;
static int8_t  laser_dx;
static int8_t  laser_dy;
static bool    laser_active = false;

static void write_laser_pos(int16_t x, int16_t y)
{
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, x_pos_px, x);
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, y_pos_px, y);
}

void laser_init(void)
{
    laser_active = false;
    laser_x = -64;
    laser_y = -64;
    write_laser_pos(-64, -64);
}

void laser_fire(LaserDirection dir)
{
    if (laser_active || dir == LASER_NONE)
        return;

    laser_active = true;
    laser_x = MOTHERSHIP_X - LASER_HALF_SIZE;
    laser_y = MOTHERSHIP_Y - LASER_HALF_SIZE;
    laser_dx = 0;
    laser_dy = 0;

    switch (dir) {
        case LASER_UP:
            laser_dy = -LASER_SPEED;
            xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LASER_FRAME_VERT);
            break;
        case LASER_DOWN:
            laser_dy = LASER_SPEED;
            xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LASER_FRAME_VERT);
            break;
        case LASER_LEFT:
            laser_dx = -LASER_SPEED;
            xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LASER_FRAME_HORIZ);
            break;
        case LASER_RIGHT:
            laser_dx = LASER_SPEED;
            xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LASER_FRAME_HORIZ);
            break;
        default:
            laser_active = false;
            return;
    }

    write_laser_pos(laser_x, laser_y);
}

void laser_update(void)
{
    if (!laser_active)
        return;

    laser_x += laser_dx;
    laser_y += laser_dy;

    // Deactivate once the sprite is fully off any edge
    if (laser_x > SCREEN_WIDTH || laser_x < -32 ||
        laser_y > SPACE_HEIGHT || laser_y < -32) {
        laser_active = false;
        write_laser_pos(-64, -64);
        return;
    }

    write_laser_pos(laser_x, laser_y);
}

bool laser_check_hit(int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    if (!laser_active)
        return false;

    if ((int16_t)(x + w) > laser_x && x < laser_x + 32 &&
        (int16_t)(y + h) > laser_y && y < laser_y + 32) {
        laser_active = false;
        write_laser_pos(-64, -64);
        return true;
    }
    return false;
}
