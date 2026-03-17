#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "beasties.h"

#define BEASTIE_SIZE 8
#define BEASTIE_FRAME_SIZE (BEASTIE_SIZE * BEASTIE_SIZE * 2)
#define BEASTIE_TYPES 5
#define BEASTIE_FRAMES_PER_DIR 2
#define BEASTIE_GROUND_Y 191
#define BEASTIE_LEFT_LIMIT 0
#define BEASTIE_RIGHT_LIMIT (SCREEN_WIDTH - BEASTIE_SIZE)
#define BEASTIE_ANIM_TICKS 8

typedef struct {
    int16_t x;
    int8_t dx;
    uint8_t type;
    uint8_t frame;
    uint8_t tick;
    bool visible;
} Beastie;

static Beastie beastie_a;
static Beastie beastie_b;

static unsigned frame_ptr(uint8_t type, bool moving_right, uint8_t frame)
{
    unsigned index = (unsigned)type * 4u;
    if (moving_right)
        index += 2u;
    index += (unsigned)(frame & 1u);
    return BEASTIES_DATA + (index * BEASTIE_FRAME_SIZE);
}

static void write_beastie(unsigned config, int16_t x, int16_t y, unsigned ptr)
{
    xram0_struct_set(config, vga_mode4_sprite_t, x_pos_px, x);
    xram0_struct_set(config, vga_mode4_sprite_t, y_pos_px, y);
    xram0_struct_set(config, vga_mode4_sprite_t, xram_sprite_ptr, ptr);
}

static void hide_beastie(unsigned config)
{
    xram0_struct_set(config, vga_mode4_sprite_t, x_pos_px, -32);
    xram0_struct_set(config, vga_mode4_sprite_t, y_pos_px, -32);
}

static void reset_one(Beastie* beastie, int16_t x, int8_t dx, uint8_t type)
{
    beastie->x = x;
    beastie->dx = dx;
    beastie->type = type;
    beastie->frame = 0;
    beastie->tick = 0;
    beastie->visible = false;
}

static void update_one(Beastie* beastie, unsigned config, bool enabled)
{
    bool moving_right;

    if (!enabled) {
        if (beastie->visible) {
            hide_beastie(config);
            beastie->visible = false;
        }
        return;
    }

    beastie->x += beastie->dx;
    if (beastie->x <= BEASTIE_LEFT_LIMIT) {
        beastie->x = BEASTIE_LEFT_LIMIT;
        beastie->dx = 1;
        beastie->type = (uint8_t)((beastie->type + 1u) % BEASTIE_TYPES);
    } else if (beastie->x >= BEASTIE_RIGHT_LIMIT) {
        beastie->x = BEASTIE_RIGHT_LIMIT;
        beastie->dx = -1;
        beastie->type = (uint8_t)((beastie->type + 1u) % BEASTIE_TYPES);
    }

    if (++beastie->tick >= BEASTIE_ANIM_TICKS) {
        beastie->tick = 0;
        beastie->frame ^= 1u;
    }

    moving_right = (beastie->dx > 0);
    write_beastie(config,
                  beastie->x,
                  BEASTIE_GROUND_Y,
                  frame_ptr(beastie->type, moving_right, beastie->frame));
    beastie->visible = true;
}

void beasties_init(void)
{
    reset_one(&beastie_a, 48, 1, 0);
    reset_one(&beastie_b, 248, -1, 2);

    hide_beastie(BEASTIE1_CONFIG);
    hide_beastie(BEASTIE2_CONFIG);
}

void beasties_reset(void)
{
    reset_one(&beastie_a, 48, 1, 0);
    reset_one(&beastie_b, 248, -1, 2);

    hide_beastie(BEASTIE1_CONFIG);
    hide_beastie(BEASTIE2_CONFIG);
}

void beasties_update(bool enabled)
{
    update_one(&beastie_a, BEASTIE1_CONFIG, enabled);
    update_one(&beastie_b, BEASTIE2_CONFIG, enabled);
}
