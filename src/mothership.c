#include <rp6502.h>
#include <stdint.h>
#include "constants.h"
#include "palette.h"
#include "mothership.h"

#define MOTHERSHIP_CYCLE_COUNT 6
#define MOTHERSHIP_TICKS       4

static const uint8_t mothership_indices[MOTHERSHIP_CYCLE_COUNT] = {
    16, 32, 48, 64, 80, 96,
};

static uint8_t mothership_phase = 0;
static uint8_t mothership_tick  = 0;

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

void mothership_init(void)
{
    mothership_phase = 0;
    mothership_tick  = 0;
    apply_mothership_palette();
}

void mothership_update(void)
{
    if (++mothership_tick < MOTHERSHIP_TICKS)
        return;
    mothership_tick = 0;
    if (++mothership_phase >= MOTHERSHIP_CYCLE_COUNT)
        mothership_phase = 0;
    apply_mothership_palette();
}
