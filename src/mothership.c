#include <rp6502.h>
#include <stdbool.h>
#include <stdint.h>
#include "contants.h"
#include "mothership.h"

#define MOTHERSHIP_FRAME_STRIDE 0x0800U
#define MOTHERSHIP_SPRITE_LOG_SIZE 5
#define MOTHERSHIP_PART_SIZE_PX 32

static unsigned mothership_config_addr;
static mothership_position_t mothership_pos = {96, 104};

static unsigned mothership_frame_ptr(uint8_t frame)
{
    return COSMICARC_DATA + ((unsigned)frame * MOTHERSHIP_FRAME_STRIDE);
}

static void write_mothership_part(uint8_t part_index)
{
    unsigned config_addr = mothership_config_addr +
        ((unsigned)part_index * (unsigned)sizeof(vga_mode4_sprite_t));

    xram0_struct_set(config_addr, vga_mode4_sprite_t, x_pos_px,
        (mothership_pos.x_pos_px + (part_index * MOTHERSHIP_PART_SIZE_PX)));
    xram0_struct_set(config_addr, vga_mode4_sprite_t, y_pos_px, mothership_pos.y_pos_px);
    xram0_struct_set(config_addr, vga_mode4_sprite_t, xram_sprite_ptr, mothership_frame_ptr(part_index));
    xram0_struct_set(config_addr, vga_mode4_sprite_t, log_size, MOTHERSHIP_SPRITE_LOG_SIZE);
    xram0_struct_set(config_addr, vga_mode4_sprite_t, has_opacity_metadata, false);
}

void init_mothership(unsigned config_addr)
{
    uint8_t i;

    mothership_config_addr = config_addr;

    for (i = 0; i < MOTHERSHIP_PART_COUNT; ++i) {
        write_mothership_part(i);
    }
}

void update_mothership(void)
{
    uint8_t i;

    for (i = 0; i < MOTHERSHIP_PART_COUNT; ++i) {
        write_mothership_part(i);
    }
}
