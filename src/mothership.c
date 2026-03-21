#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "palette.h"
#include "mothership.h"
#include "sound.h"

#define MOTHERSHIP_CYCLE_COUNT   6
#define MOTHERSHIP_TICKS         2
#define MOTHERSHIP_LAND_Y        16
#define MOTHERSHIP_START_Y       (-176)
#define MOTHERSHIP_DESCENT_SPEED 1
#define MOTHERSHIP_DESTRUCTION_FRAMES 180
#define MOTHERSHIP_DESTRUCTION_TICKS  1

#define MOTHERSHIP_TILE_X0 12
#define MOTHERSHIP_TILE_Y0 9
#define MOTHERSHIP_TILE_X1 27
#define MOTHERSHIP_TILE_Y1 11

#define MOTHERSHIP_TILE_W (MOTHERSHIP_TILE_X1 - MOTHERSHIP_TILE_X0 + 1)
#define MOTHERSHIP_TILE_H (MOTHERSHIP_TILE_Y1 - MOTHERSHIP_TILE_Y0 + 1)
#define MOTHERSHIP_TILE_COUNT (MOTHERSHIP_TILE_W * MOTHERSHIP_TILE_H)

// Y position where the mothership tilemap puts the ship graphic at the top of screen
#define MOTHERSHIP_APPEAR_Y             (-(MOTHERSHIP_TILE_Y0 * 8))
#define MOTHERSHIP_APPEAR_TICKS_PER_STEP 8
// Departure: scroll up until fully off screen, then re-enter
#define MOTHERSHIP_DEPART_SPEED         2
#define MOTHERSHIP_DEPART_OFFSCREEN_Y   (-((int16_t)(MOTHERSHIP_TILE_Y0 + MOTHERSHIP_TILE_H) * 8))

static const uint8_t mothership_indices[MOTHERSHIP_CYCLE_COUNT] = {
    16, 32, 48, 64, 80, 96,
};

static const uint8_t mothership_destruction_indices[8] = {
    4, 20, 36, 52, 68, 84, 100, 116,
};

typedef enum {
    MOTHERSHIP_APPEARING = 0,
    MOTHERSHIP_DESCENDING,
    MOTHERSHIP_LANDED,
    MOTHERSHIP_DEPARTING,
    MOTHERSHIP_DESTROYING,
} MothershipState;

static uint8_t  mothership_phase = 0;
static uint8_t  mothership_tick  = 0;
static int16_t  mothership_y     = MOTHERSHIP_START_Y;
static MothershipState mothership_state = MOTHERSHIP_DESCENDING;
static uint8_t  mothership_destroy_tick = 0;
static uint16_t mothership_destroy_timer = 0;
static uint8_t  appear_step = 0;
static uint8_t  appear_tick = 0;
static uint16_t mothership_rng = 0x6C8Du;
static uint8_t mothership_saved_tiles[MOTHERSHIP_TILE_COUNT];
static bool mothership_respawned_after_destruction = false;
static bool mothership_waiting_for_respawn = false;
static bool mothership_departed = false;

static bool mothership_sfx_enabled = false;

static uint16_t next_rand(void)
{
    mothership_rng = (uint16_t)(mothership_rng * 109u + 89u);
    return mothership_rng;
}

static unsigned mothership_tilemap_addr(uint8_t tx, uint8_t ty)
{
    return (unsigned)(MOTHERSHIP_MAP_TILEMAP_DATA + ((unsigned)ty * MAIN_MAP_WIDTH_TILES) + tx);
}

static void palette_write_entry(uint8_t index, uint16_t color)
{
    RIA.addr0 = PALETTE_ADDR + ((unsigned)index * 2u);
    RIA.step0 = 1;
    RIA.rw0 = color & 0xFF;
    RIA.rw0 = color >> 8;
}

static void apply_mothership_palette(void)
{
    uint8_t i;
    for (i = 0; i < MOTHERSHIP_CYCLE_COUNT; ++i) {
        uint8_t src = (i + mothership_phase) % MOTHERSHIP_CYCLE_COUNT;
        palette_write_entry(mothership_indices[i],
                            tile_palette[mothership_indices[src]]);
    }
}

static void restore_destruction_palette(void)
{
    uint8_t i;
    for (i = 0; i < 8; ++i) {
        uint8_t idx = mothership_destruction_indices[i];
        palette_write_entry(idx, tile_palette[idx]);
    }
}

static void apply_destruction_palette(void)
{
    uint16_t shuffled[8];
    uint8_t transparent_threshold;
    uint8_t i;

    transparent_threshold = (uint8_t)((mothership_destroy_timer * 255u) /
                                      (MOTHERSHIP_DESTRUCTION_FRAMES - 1u));

    for (i = 0; i < 8; ++i) {
        shuffled[i] = tile_palette[mothership_destruction_indices[i]];
    }

    // Fisher-Yates shuffle so each destruction tick visibly reorders colors.
    for (i = 7; i > 0; --i) {
        uint8_t j = (uint8_t)(next_rand() % (uint16_t)(i + 1u));
        uint16_t tmp = shuffled[i];
        shuffled[i] = shuffled[j];
        shuffled[j] = tmp;
    }

    for (i = 0; i < 8; ++i) {
        uint8_t idx = mothership_destruction_indices[i];
        uint16_t color;
        if ((next_rand() & 0xFFu) <= transparent_threshold) {
            color = 0x0000;
        } else {
            color = shuffled[i];
        }
        palette_write_entry(idx, color);
    }
}

static void backup_mothership_tiles(void)
{
    uint8_t x;
    uint8_t y;
    uint8_t i = 0;

    for (y = MOTHERSHIP_TILE_Y0; y <= MOTHERSHIP_TILE_Y1; ++y) {
        RIA.addr0 = mothership_tilemap_addr(MOTHERSHIP_TILE_X0, y);
        RIA.step0 = 1;
        for (x = 0; x < MOTHERSHIP_TILE_W; ++x) {
            mothership_saved_tiles[i++] = RIA.rw0;
        }
    }
}

static void restore_mothership_tiles(void)
{
    uint8_t x;
    uint8_t y;
    uint8_t i = 0;

    for (y = MOTHERSHIP_TILE_Y0; y <= MOTHERSHIP_TILE_Y1; ++y) {
        RIA.addr0 = mothership_tilemap_addr(MOTHERSHIP_TILE_X0, y);
        RIA.step0 = 1;
        for (x = 0; x < MOTHERSHIP_TILE_W; ++x) {
            RIA.rw0 = mothership_saved_tiles[i++];
        }
    }
}

static void apply_destruction_transparent_tiles(void)
{
    RIA.addr0 = mothership_tilemap_addr(12u, 10u);
    RIA.step0 = 1;
    RIA.rw0 = 0;

    RIA.addr0 = mothership_tilemap_addr(27u, 10u);
    RIA.step0 = 1;
    RIA.rw0 = 0;
}

void mothership_init(void)
{
    mothership_phase = 0;
    mothership_tick  = 0;
    mothership_state = MOTHERSHIP_DESCENDING;
    mothership_destroy_tick = 0;
    mothership_destroy_timer = 0;
    appear_step = 0;
    appear_tick = 0;
    mothership_respawned_after_destruction = false;
    mothership_waiting_for_respawn = false;
    backup_mothership_tiles();
    restore_mothership_tiles();
    restore_destruction_palette();
    apply_mothership_palette();
}

void mothership_reset(void)
{
    restore_mothership_tiles();
    restore_destruction_palette();
    mothership_y = MOTHERSHIP_START_Y;
    mothership_state = MOTHERSHIP_DESCENDING;
    mothership_destroy_tick = 0;
    mothership_destroy_timer = 0;
    mothership_respawned_after_destruction = mothership_waiting_for_respawn;
    mothership_waiting_for_respawn = false;
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_pos_px, mothership_y);
}

void mothership_reset_appear(void)
{
    uint8_t i;
    restore_mothership_tiles();
    mothership_y = MOTHERSHIP_APPEAR_Y;
    mothership_state = MOTHERSHIP_APPEARING;
    appear_step = 0;
    appear_tick = 0;
    mothership_destroy_tick = 0;
    mothership_destroy_timer = 0;
    mothership_respawned_after_destruction = mothership_waiting_for_respawn;
    mothership_waiting_for_respawn = false;
    // Zero the 8 appearance palette entries; they'll be revealed one by one
    for (i = 0; i < 8u; ++i) {
        palette_write_entry(mothership_destruction_indices[i], 0);
    }
    apply_mothership_palette();
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_pos_px, mothership_y);
    if (mothership_sfx_enabled)
        sound_play_mothership_appear();
}

void mothership_start_departure(void)
{
    mothership_state = MOTHERSHIP_DEPARTING;
    if (mothership_sfx_enabled)
        sound_play_mothership_depart();
}

void mothership_start_destruction(void)
{
    if (mothership_state == MOTHERSHIP_DESTROYING)
        return;

    mothership_state = MOTHERSHIP_DESTROYING;
    mothership_destroy_tick = 0;
    mothership_destroy_timer = 0;
    mothership_waiting_for_respawn = true;
    apply_destruction_transparent_tiles();
    apply_destruction_palette();
}

void mothership_set_sfx_enabled(bool enabled)
{
    mothership_sfx_enabled = enabled;
}

bool mothership_is_descending(void)
{
    return mothership_state == MOTHERSHIP_DESCENDING;
}

bool mothership_is_landed(void)
{
    return mothership_state == MOTHERSHIP_LANDED;
}

bool mothership_consume_respawned_after_destruction(void)
{
    bool respawned = mothership_respawned_after_destruction;
    mothership_respawned_after_destruction = false;
    return respawned;
}

bool mothership_consume_departed(void)
{
    bool d = mothership_departed;
    mothership_departed = false;
    return d;
}

void mothership_update(void)
{
    uint8_t idx;

    if (mothership_state == MOTHERSHIP_DESTROYING) {
        if (++mothership_destroy_tick >= MOTHERSHIP_DESTRUCTION_TICKS) {
            mothership_destroy_tick = 0;
            apply_destruction_palette();
        }

        if (++mothership_destroy_timer >= MOTHERSHIP_DESTRUCTION_FRAMES) {
            mothership_reset_appear();
        }
        return;
    }

    if (mothership_state == MOTHERSHIP_DEPARTING) {
        mothership_y -= MOTHERSHIP_DEPART_SPEED;
        xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_pos_px, mothership_y);
        if (mothership_y <= MOTHERSHIP_DEPART_OFFSCREEN_Y) {
            // Fully off screen — signal caller, then begin the re-entry scroll
            mothership_departed = true;
            mothership_reset();
        }
        return;
    }

    if (mothership_state == MOTHERSHIP_APPEARING) {
        if (appear_step < 8u) {
            if (++appear_tick >= MOTHERSHIP_APPEAR_TICKS_PER_STEP) {
                idx = mothership_destruction_indices[appear_step];
                palette_write_entry(idx, tile_palette[idx]);
                appear_tick = 0;
                ++appear_step;
            }
        } else {
            mothership_state = MOTHERSHIP_DESCENDING;
            if (mothership_sfx_enabled)
                sound_skip_descent_delay();
        }
    }

    if (mothership_state == MOTHERSHIP_DESCENDING) {
        mothership_y += MOTHERSHIP_DESCENT_SPEED;
        if (mothership_y >= MOTHERSHIP_LAND_Y) {
            mothership_y = MOTHERSHIP_LAND_Y;
            mothership_state = MOTHERSHIP_LANDED;
        }
        xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_pos_px, mothership_y);
    }

    if (++mothership_tick < MOTHERSHIP_TICKS)
        return;
    mothership_tick = 0;
    if (++mothership_phase >= MOTHERSHIP_CYCLE_COUNT)
        mothership_phase = 0;
    apply_mothership_palette();
}
