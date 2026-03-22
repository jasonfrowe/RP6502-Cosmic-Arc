#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "asteroid.h"
#include "laser.h"

#define ASTEROID_SIZE 8
#define ASTEROID_FRAME_COUNT 4
#define ASTEROID_FRAME_SIZE (ASTEROID_SIZE * ASTEROID_SIZE * 2)
#define ASTEROID_ANIM_TICKS 6

// Level-scaled parameters — speed in 1/8-pixel units for smooth ramping.
// vel / 8 gives px/frame:  16=2.0, 20=2.5, 24=3.0, 28=3.5, 32=4.0
#define ASTEROID_VEL_BASE       16   // 2.0 px/frame
#define ASTEROID_VEL_MAX        32   // 4.0 px/frame
#define ASTEROID_VEL_PER_LEVEL   2   // +0.5 px/frame per level
#define ASTEROID_SPAWN_DELAY_BASE  45
#define ASTEROID_SPAWN_DELAY_MIN   20
#define ASTEROID_SPAWN_DELAY_PER_LEVEL 5

static int16_t asteroid_vel        = ASTEROID_VEL_BASE;  // 1/8-px units
static uint8_t asteroid_spawn_delay = ASTEROID_SPAWN_DELAY_BASE;

#define MOTHERSHIP_WIDTH 128
#define MOTHERSHIP_HEIGHT 24

static int16_t asteroid_x;
static int16_t asteroid_y;
static int16_t asteroid_subx;  // fractional x accumulator in 1/8-px units
static int16_t asteroid_suby;  // fractional y accumulator in 1/8-px units
static int8_t  asteroid_dir_x; // direction: -1, 0, or +1
static int8_t  asteroid_dir_y; // direction: -1, 0, or +1
static bool asteroid_active;
static uint8_t asteroid_frame;
static uint8_t asteroid_anim_tick;
static uint8_t asteroid_spawn_tick;
static uint16_t asteroid_rng;
static bool asteroid_planet_phase = false;
static bool asteroid_spawns_paused = false;

static uint16_t next_rand(void)
{
    asteroid_rng ^= (uint16_t)(asteroid_rng << 7);
    asteroid_rng ^= (uint16_t)(asteroid_rng >> 9);
    asteroid_rng ^= (uint16_t)(asteroid_rng << 8);
    return asteroid_rng;
}

static void write_asteroid_pos(int16_t x, int16_t y)
{
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, x_pos_px, x);
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, y_pos_px, y);
}

static void write_asteroid_frame(uint8_t frame)
{
    unsigned ptr = ASTEROID_DATA + ((unsigned)frame * ASTEROID_FRAME_SIZE);
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, ptr);
}

static void deactivate_asteroid(void)
{
    asteroid_active = false;
    asteroid_x = -32;
    asteroid_y = -32;
    asteroid_subx = 0;
    asteroid_suby = 0;
    asteroid_dir_x = 0;
    asteroid_dir_y = 0;
    write_asteroid_pos(-32, -32);
}

static bool asteroid_hits_mothership(void)
{
    int16_t asteroid_left = asteroid_x;
    int16_t asteroid_right = asteroid_x + ASTEROID_SIZE;
    int16_t asteroid_top = asteroid_y;
    int16_t asteroid_bottom = asteroid_y + ASTEROID_SIZE;

    int16_t mothership_left = MOTHERSHIP_X - (MOTHERSHIP_WIDTH / 2);
    int16_t mothership_right = MOTHERSHIP_X + (MOTHERSHIP_WIDTH / 2);
    int16_t mothership_top = MOTHERSHIP_Y - (MOTHERSHIP_HEIGHT / 2);
    int16_t mothership_bottom = MOTHERSHIP_Y + (MOTHERSHIP_HEIGHT / 2);

    return (asteroid_left < mothership_right) &&
           (asteroid_right > mothership_left) &&
           (asteroid_top < mothership_bottom) &&
           (asteroid_bottom > mothership_top);
}

static void spawn_asteroid(void)
{
    uint8_t side = (uint8_t)(next_rand() & 0x03u);

    if (asteroid_planet_phase && side == 2) {
        side = 0; // Remap bottom spawns to top
    }

    asteroid_frame = 0;
    asteroid_anim_tick = 0;
    asteroid_subx = 0;
    asteroid_suby = 0;

    switch (side) {
        case 0: // top -> down
            asteroid_x = MOTHERSHIP_X - (ASTEROID_SIZE / 2);
            asteroid_y = -ASTEROID_SIZE;
            asteroid_dir_x = 0;
            asteroid_dir_y = 1;
            break;
        case 1: // right -> left
            asteroid_x = SCREEN_WIDTH;
            asteroid_y = MOTHERSHIP_Y - (ASTEROID_SIZE / 2);
            asteroid_dir_x = -1;
            asteroid_dir_y = 0;
            break;
        case 2: // bottom -> up
            asteroid_x = MOTHERSHIP_X - (ASTEROID_SIZE / 2);
            asteroid_y = SPACE_HEIGHT;
            asteroid_dir_x = 0;
            asteroid_dir_y = -1;
            break;
        default: // left -> right
            asteroid_x = -ASTEROID_SIZE;
            asteroid_y = MOTHERSHIP_Y - (ASTEROID_SIZE / 2);
            asteroid_dir_x = 1;
            asteroid_dir_y = 0;
            break;
    }

    asteroid_active = true;
    write_asteroid_frame(asteroid_frame);
    write_asteroid_pos(asteroid_x, asteroid_y);
}

void asteroid_set_level(uint8_t level)
{
    int16_t v = (int16_t)(ASTEROID_VEL_BASE + (int16_t)level * ASTEROID_VEL_PER_LEVEL);
    if (v > ASTEROID_VEL_MAX) v = ASTEROID_VEL_MAX;
    asteroid_vel = v;

    uint8_t dec = (uint8_t)(level * ASTEROID_SPAWN_DELAY_PER_LEVEL);
    asteroid_spawn_delay = (ASTEROID_SPAWN_DELAY_BASE > ASTEROID_SPAWN_DELAY_MIN + dec)
                           ? (uint8_t)(ASTEROID_SPAWN_DELAY_BASE - dec)
                           : ASTEROID_SPAWN_DELAY_MIN;
}

void asteroid_init(void)
{
    asteroid_rng = 0xA57Du;
    asteroid_spawn_tick = 0;
    asteroid_set_level(0);
    deactivate_asteroid();
}

AsteroidResult asteroid_update(void)
{
    if (!asteroid_active) {
        // Stop natural asteroid spawning if paused
        if (!asteroid_spawns_paused && ++asteroid_spawn_tick >= asteroid_spawn_delay) {
            asteroid_spawn_tick = 0;
            spawn_asteroid();
        }
        return ASTEROID_NONE;
    }

    // Fixed-point motion: accumulate 1/8-px units, carry whole pixels each frame.
    asteroid_subx += (int16_t)(asteroid_dir_x * asteroid_vel);
    {
        int16_t dpx = asteroid_subx / 8;
        asteroid_subx -= dpx * 8;
        asteroid_x += dpx;
    }
    asteroid_suby += (int16_t)(asteroid_dir_y * asteroid_vel);
    {
        int16_t dpy = asteroid_suby / 8;
        asteroid_suby -= dpy * 8;
        asteroid_y += dpy;
    }

    if (++asteroid_anim_tick >= ASTEROID_ANIM_TICKS) {
        asteroid_anim_tick = 0;
        asteroid_frame = (uint8_t)((asteroid_frame + 1u) & 0x03u);
        write_asteroid_frame(asteroid_frame);
    }

    if (laser_check_hit(asteroid_x, asteroid_y, ASTEROID_SIZE, ASTEROID_SIZE)) {
        deactivate_asteroid();
        return ASTEROID_LASER_HIT;
    }

    if (asteroid_hits_mothership()) {
        deactivate_asteroid();
        return ASTEROID_MOTHERSHIP_HIT;
    }

    if (asteroid_x > SCREEN_WIDTH || asteroid_x < -ASTEROID_SIZE ||
        asteroid_y > SPACE_HEIGHT || asteroid_y < -ASTEROID_SIZE) {
        deactivate_asteroid();
        return ASTEROID_NONE;
    }

    write_asteroid_pos(asteroid_x, asteroid_y);
    return ASTEROID_NONE;
}

void asteroid_reset(void)
{
    deactivate_asteroid();
    asteroid_spawn_tick = 0;
}

void asteroid_set_planet_phase(bool active)
{
    asteroid_planet_phase = active;
}

void asteroid_set_spawns_paused(bool paused)
{
    asteroid_spawns_paused = paused;
}

void asteroid_force_spawn(void)
{
    spawn_asteroid();
}

bool asteroid_is_active(void)
{
    return asteroid_active;
}
