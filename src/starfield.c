#include <rp6502.h>
#include <stdint.h>
#include "contants.h"
#include "starfield.h"

#define STAR_FLASH_PALETTE_START 2
#define STAR_FLASH_COLOR_COUNT 4
#define STAR_FLASH_TICKS 6
#define STAR_FLASH_OFF_COLOR 0x3124

static const uint16_t star_flash_colors[STAR_FLASH_COLOR_COUNT] = {
    0xFFFF,
    0x5FFF,
    0x94BF,
    0xFE32,
};

static uint8_t star_flash_phase = 0;
static uint8_t star_flash_tick = 0;
static int8_t star_flash_dir = 1;

static void palette_write_entry(uint8_t index, uint16_t color)
{
    RIA.addr0 = PALETTE_ADDR + ((unsigned)index * 2u);
    RIA.step0 = 1;
    RIA.rw0 = color & 0xFF;
    RIA.rw0 = color >> 8;
}

static void apply_star_flash_palette(void)
{
    uint8_t i;

    for (i = 0; i < STAR_FLASH_COLOR_COUNT; ++i) {
        uint16_t color = (i == star_flash_phase) ? star_flash_colors[i] : STAR_FLASH_OFF_COLOR;
        palette_write_entry(STAR_FLASH_PALETTE_START + i, color);
    }
}

void starfield_init(void)
{
    star_flash_phase = 0;
    star_flash_tick = 0;
    star_flash_dir = 1;
    apply_star_flash_palette();
}

void starfield_update(void)
{
    if (++star_flash_tick < STAR_FLASH_TICKS) {
        return;
    }

    star_flash_tick = 0;

    if (star_flash_phase == (STAR_FLASH_COLOR_COUNT - 1)) {
        star_flash_dir = -1;
    } else if (star_flash_phase == 0) {
        star_flash_dir = 1;
    }

    star_flash_phase = (uint8_t)(star_flash_phase + star_flash_dir);
    apply_star_flash_palette();
}
