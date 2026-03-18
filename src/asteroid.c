#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "asteroid.h"
#include "laser.h"

#define ASTEROID_SIZE 8
#define ASTEROID_SPEED 2
#define ASTEROID_FRAME_COUNT 4
#define ASTEROID_FRAME_SIZE (ASTEROID_SIZE * ASTEROID_SIZE * 2)
#define ASTEROID_ANIM_TICKS 6
#define ASTEROID_SPAWN_DELAY 45

#define MOTHERSHIP_WIDTH 128
#define MOTHERSHIP_HEIGHT 24

static int16_t asteroid_x;
static int16_t asteroid_y;
static int8_t asteroid_dx;
static int8_t asteroid_dy;
static bool asteroid_active;
static uint8_t asteroid_frame;
static uint8_t asteroid_anim_tick;
static uint8_t asteroid_spawn_tick;
static uint16_t asteroid_rng;
static bool asteroid_planet_phase = false;

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
    asteroid_dx = 0;
    asteroid_dy = 0;
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

    switch (side) {
        case 0: // top -> down
            asteroid_x = MOTHERSHIP_X - (ASTEROID_SIZE / 2);
            asteroid_y = -ASTEROID_SIZE;
            asteroid_dx = 0;
            asteroid_dy = ASTEROID_SPEED;
            break;
        case 1: // right -> left
            asteroid_x = SCREEN_WIDTH;
            asteroid_y = MOTHERSHIP_Y - (ASTEROID_SIZE / 2);
            asteroid_dx = -ASTEROID_SPEED;
            asteroid_dy = 0;
            break;
        case 2: // bottom -> up
            asteroid_x = MOTHERSHIP_X - (ASTEROID_SIZE / 2);
            asteroid_y = SPACE_HEIGHT;
            asteroid_dx = 0;
            asteroid_dy = -ASTEROID_SPEED;
            break;
        default: // left -> right
            asteroid_x = -ASTEROID_SIZE;
            asteroid_y = MOTHERSHIP_Y - (ASTEROID_SIZE / 2);
            asteroid_dx = ASTEROID_SPEED;
            asteroid_dy = 0;
            break;
    }

    asteroid_active = true;
    write_asteroid_frame(asteroid_frame);
    write_asteroid_pos(asteroid_x, asteroid_y);
}

void asteroid_init(void)
{
    asteroid_rng = 0xA57Du;
    asteroid_spawn_tick = 0;
    deactivate_asteroid();
}

AsteroidResult asteroid_update(void)
{
    if (!asteroid_active) {
        // Stop natural asteroid spawning during the planet phase delay
        if (!asteroid_planet_phase && ++asteroid_spawn_tick >= ASTEROID_SPAWN_DELAY) {
            asteroid_spawn_tick = 0;
            spawn_asteroid();
        }
        return ASTEROID_NONE;
    }

    asteroid_x += asteroid_dx;
    asteroid_y += asteroid_dy;

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

void asteroid_force_spawn(void)
{
    spawn_asteroid();
}

bool asteroid_is_active(void)
{
    return asteroid_active;
}
