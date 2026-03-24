#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "laser.h"
#include "sound.h"

#define LASER_SPEED      6
#define LASER_HALF_SIZE  16   // 32x32 sprite half-width

// Mothership body pixel edges — derived from tile layout (X0=12, X1=27, Y0=9, Y1=11, LAND_Y=16)
// Used to spawn the laser at the ship's exit edge so it emerges cleanly rather than
// travelling invisibly from the center.
#define LASER_SHIP_LEFT   96    // MOTHERSHIP_TILE_X0 * 8
#define LASER_SHIP_RIGHT  224   // (MOTHERSHIP_TILE_X1 + 1) * 8
#define LASER_SHIP_TOP    88    // MOTHERSHIP_LAND_Y + MOTHERSHIP_TILE_Y0 * 8
#define LASER_SHIP_BOTTOM 112   // MOTHERSHIP_LAND_Y + (MOTHERSHIP_TILE_Y1 + 1) * 8

// Frame 0: left/right (horizontal beam)
// Frame 1: up/down   (vertical beam)
// Each frame: 32 * 32 * 2 bytes (16-bit RGB555)
#define LASER_FRAME_HORIZ  (LASER_DATA)
#define LASER_FRAME_VERT   (LASER_DATA + 32 * 32 * 2)

static int16_t laser_x;
static int16_t laser_y;
static int8_t  laser_dx;
static int8_t  laser_dy;
static bool    laser_active   = false;

static void write_laser_pos(int16_t x, int16_t y)
{
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, x_pos_px, x);
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, y_pos_px, y);
}

static void deactivate_laser(void)
{
    laser_active = false;
    write_laser_pos(-64, -64);
}

void laser_init(void)
{
    laser_active = false;
    laser_x = -64;
    laser_y = -64;
    write_laser_pos(-64, -64);
}

bool laser_fire(LaserDirection dir)
{
    if (laser_active || dir == LASER_NONE)
        return false;

    laser_active = true;
    laser_dx = 0;
    laser_dy = 0;

    switch (dir) {
        case LASER_UP:
            // Sprite top aligned to ship top; centered horizontally — moves up
            laser_x = MOTHERSHIP_X - LASER_HALF_SIZE;
            laser_y = LASER_SHIP_TOP;
            laser_dy = -LASER_SPEED;
            xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LASER_FRAME_VERT);
            break;
        case LASER_DOWN:
            // Sprite bottom aligned to ship bottom; centered horizontally — moves down
            laser_x = MOTHERSHIP_X - LASER_HALF_SIZE;
            laser_y = LASER_SHIP_BOTTOM - LASER_HALF_SIZE * 2;
            laser_dy = LASER_SPEED;
            xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LASER_FRAME_VERT);
            break;
        case LASER_LEFT:
            // Sprite left aligned to ship left; centered vertically — moves left
            laser_x = LASER_SHIP_LEFT;
            laser_y = MOTHERSHIP_Y - LASER_HALF_SIZE;
            laser_dx = -LASER_SPEED;
            xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LASER_FRAME_HORIZ);
            break;
        case LASER_RIGHT:
            // Sprite right aligned to ship right; centered vertically — moves right
            laser_x = LASER_SHIP_RIGHT - LASER_HALF_SIZE * 2;
            laser_y = MOTHERSHIP_Y - LASER_HALF_SIZE;
            laser_dx = LASER_SPEED;
            xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LASER_FRAME_HORIZ);
            break;
        default:
            laser_active = false;
            return false;
    }

    sound_play_laser();
    write_laser_pos(laser_x, laser_y);
    return true;
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
        deactivate_laser();
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
        deactivate_laser();
        return true;
    }
    return false;
}
