#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "palette.h"
#include "mothership.h"

#define MOTHERSHIP_CYCLE_COUNT   6
#define MOTHERSHIP_TICKS         4
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

static const uint8_t mothership_indices[MOTHERSHIP_CYCLE_COUNT] = {
    16, 32, 48, 64, 80, 96,
};

static const uint8_t mothership_destruction_indices[8] = {
    4, 20, 36, 52, 68, 84, 100, 116,
};

typedef enum {
    MOTHERSHIP_DESCENDING = 0,
    MOTHERSHIP_LANDED,
    MOTHERSHIP_DESTROYING,
} MothershipState;

static uint8_t  mothership_phase = 0;
static uint8_t  mothership_tick  = 0;
static int16_t  mothership_y     = MOTHERSHIP_START_Y;
static MothershipState mothership_state = MOTHERSHIP_DESCENDING;
static uint8_t  mothership_destroy_tick = 0;
static uint16_t mothership_destroy_timer = 0;
static uint16_t mothership_rng = 0x6C8Du;
static uint8_t mothership_saved_tiles[MOTHERSHIP_TILE_COUNT];

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
    uint8_t i;

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
        if (next_rand() & 1u) {
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

static void apply_destruction_tiles(void)
{
    uint8_t x;
    uint8_t y;

    for (y = 0; y < MOTHERSHIP_TILE_H; ++y) {
        RIA.addr0 = mothership_tilemap_addr((uint8_t)(MOTHERSHIP_TILE_X0), (uint8_t)(MOTHERSHIP_TILE_Y0 + y));
        RIA.step0 = 1;
        for (x = 0; x < MOTHERSHIP_TILE_W; ++x) {
            uint8_t tile = (uint8_t)(103 + (next_rand() & 0x07u));
            RIA.rw0 = tile;
        }
    }
}

void mothership_init(void)
{
    mothership_phase = 0;
    mothership_tick  = 0;
    mothership_state = MOTHERSHIP_DESCENDING;
    mothership_destroy_tick = 0;
    mothership_destroy_timer = 0;
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
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_pos_px, mothership_y);
}

void mothership_start_destruction(void)
{
    if (mothership_state == MOTHERSHIP_DESTROYING)
        return;

    mothership_state = MOTHERSHIP_DESTROYING;
    mothership_destroy_tick = 0;
    mothership_destroy_timer = 0;
    apply_destruction_tiles();
    apply_destruction_palette();
}

bool mothership_is_landed(void)
{
    return mothership_state == MOTHERSHIP_LANDED;
}

void mothership_update(void)
{
    if (mothership_state == MOTHERSHIP_DESTROYING) {
        if (++mothership_destroy_tick >= MOTHERSHIP_DESTRUCTION_TICKS) {
            mothership_destroy_tick = 0;
            apply_destruction_tiles();
            apply_destruction_palette();
        }

        if (++mothership_destroy_timer >= MOTHERSHIP_DESTRUCTION_FRAMES) {
            mothership_reset();
        }
        return;
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
