#include <rp6502.h>
#include <stdint.h>
#include "constants.h"
#include "starfield.h"

#define STAR_FLASH_COLOR_COUNT 7
#define STAR_FLASH_TICKS 6
#define STAR_FLASH_OFF_COLOR 0x5960

static const uint8_t star_flash_indices[STAR_FLASH_COLOR_COUNT] = {
    26, 42, 58, 74, 90, 106, 122,
};

static const uint16_t star_flash_colors[STAR_FLASH_COLOR_COUNT] = {
    0xC4ED,  // medium-high
    0xFF34,  // brightest
    0x9367,  // medium-dim
    0xEE72,  // bright
    0xAC2A,  // medium
    0xD5AF,  // medium-bright
    0x9367,  // medium-dim (endpoint, keeps stars visible)
};

static uint8_t star_flash_phase = 0;
static uint8_t star_flash_tick = 0;
static int8_t star_flash_dir = 1;
static uint8_t star_flash_bright = 1;

static uint16_t mix_rgb555(uint16_t a, uint16_t b, uint8_t wa, uint8_t wb)
{
    uint16_t r = (uint16_t)((((a & 0x1Fu) * wa) + ((b & 0x1Fu) * wb)) / (wa + wb));
    uint16_t g = (uint16_t)((((((a >> 6) & 0x1Fu) * wa) + (((b >> 6) & 0x1Fu) * wb)) / (wa + wb)));
    uint16_t bl = (uint16_t)((((((a >> 11) & 0x1Fu) * wa) + (((b >> 11) & 0x1Fu) * wb)) / (wa + wb)));
    uint16_t alpha = (a & 0x20u) | (b & 0x20u);

    return (uint16_t)((bl << 11) | (g << 6) | r | alpha);
}

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
    uint8_t center_w = star_flash_bright ? 8u : 4u;
    uint8_t ring1_w = star_flash_bright ? 5u : 3u;
    uint8_t ring2_w = star_flash_bright ? 2u : 1u;

    for (i = 0; i < STAR_FLASH_COLOR_COUNT; ++i) {
        uint16_t color = STAR_FLASH_OFF_COLOR;

        if (i == star_flash_phase) {
            color = mix_rgb555(star_flash_colors[i], STAR_FLASH_OFF_COLOR, center_w, 2u);
        } else if ((i + 1u) == star_flash_phase || (star_flash_phase + 1u) == i) {
            color = mix_rgb555(star_flash_colors[star_flash_phase], STAR_FLASH_OFF_COLOR, ring1_w, 6u);
        } else if ((i + 2u) == star_flash_phase || (star_flash_phase + 2u) == i) {
            color = mix_rgb555(star_flash_colors[star_flash_phase], STAR_FLASH_OFF_COLOR, ring2_w, 8u);
        }

        palette_write_entry(star_flash_indices[i], color);
    }
}

void starfield_init(void)
{
    star_flash_phase = 0;
    star_flash_tick = 0;
    star_flash_dir = 1;
    star_flash_bright = 1;
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
    star_flash_bright ^= 1u;
    apply_star_flash_palette();
}
