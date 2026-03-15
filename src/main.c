#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "constants.h"
#include "starfield.h"
#include "mothership.h"
#include "laser.h"
#include "asteroid.h"
#include "palette.h"
#include "input.h"
#include "score.h"

unsigned MAIN_MAP_CONFIG;
unsigned MOTHERSHIP_CONFIG;
unsigned LASER_CONFIG;
unsigned ASTEROID_CONFIG;

static void init_graphics(void)
{
    // Select a 320x240 canvas
    if (xreg_vga_canvas(1) < 0) {
        puts("xreg_vga_canvas failed");
        return;
    }


    RIA.addr0 = PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 256; i++) {
        RIA.rw0 = tile_palette[i] & 0xFF;
        RIA.rw0 = tile_palette[i] >> 8;
    }

    // Start with one lit star color and rotate it over time.
    starfield_init();
    mothership_init();
    MAIN_MAP_CONFIG = SPRITE_DATA_END;

    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, width_tiles,  MAIN_MAP_WIDTH_TILES);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, height_tiles, MAIN_MAP_HEIGHT_TILES);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_data_ptr,    MAIN_MAP_TILEMAP_DATA); // tile ID grid
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_palette_ptr, PALETTE_ADDR);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_tile_ptr,    MAIN_MAP_DATA);        // tile bitmaps

    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=3 (8-bit color index) => 0b0011 = 3
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x03, MAIN_MAP_CONFIG, 0, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    MOTHERSHIP_CONFIG = MAIN_MAP_CONFIG + sizeof(vga_mode2_config_t);

    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_pos_px, -176);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, width_tiles,  MAIN_MAP_WIDTH_TILES);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, height_tiles, MAIN_MAP_HEIGHT_TILES);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, xram_data_ptr,    MOTHERSHIP_MAP_TILEMAP_DATA); // tile ID grid
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, xram_palette_ptr, PALETTE_ADDR);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, xram_tile_ptr,    MAIN_MAP_DATA);

    if (xreg_vga_mode(2, 0x03, MOTHERSHIP_CONFIG, 2, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    LASER_CONFIG = MOTHERSHIP_CONFIG + sizeof(vga_mode2_config_t);

    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, x_pos_px, 0);
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, y_pos_px, 0);
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, LASER_DATA);
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, log_size, LASER_SPRITE_LOG_SIZE);
    xram0_struct_set(LASER_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    ASTEROID_CONFIG = LASER_CONFIG + sizeof(vga_mode4_sprite_t);

    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, x_pos_px, 0);
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, y_pos_px, 0);
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, xram_sprite_ptr, ASTEROID_DATA);
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, log_size, 3); // 8x8 sprites  
    xram0_struct_set(ASTEROID_CONFIG, vga_mode4_sprite_t, has_opacity_metadata, false);

    // Mode 4 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(4, 0, LASER_CONFIG, 2, 1, 0, SPACE_HEIGHT) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }


    printf("MAIN_MAP DATA: 0x%04X - 0x%04X\n", MAIN_MAP_DATA, MAIN_MAP_DATA + MAIN_MAP_DATA_SIZE);
    printf("MAIN_MAP TILEMAP: 0x%04X - 0x%04X\n", MAIN_MAP_TILEMAP_DATA, MAIN_MAP_TILEMAP_DATA + MAIN_MAP_TILEMAP_SIZE);
    printf("MAIN_MAP_CONFIG: 0x%04X\n", MAIN_MAP_CONFIG);
    printf("MOTHERSHIP_CONFIG: 0x%04X\n", MOTHERSHIP_CONFIG);

}

uint8_t vsync_last = 0;

int main(void)
{
    puts("Hello from Cosmic Arc!");

    // Match RPMegaRacer input setup: map RIA keyboard/gamepad streams to XRAM buffers.
    xregn(0, 0, 0, 1, KEYBOARD_INPUT);
    xregn(0, 0, 2, 1, GAMEPAD_INPUT);

    init_graphics();
    mothership_reset();
    laser_init();
    asteroid_init();
    score_init();
    init_input_system();

    while (true) {
        // Main game loop
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        // Get Input from Keyboard/Gamepad
        handle_input();

        if (mothership_is_landed()) {
            if      (is_action_pressed(0, ACTION_THRUST))         laser_fire(LASER_UP);
            else if (is_action_pressed(0, ACTION_REVERSE_THRUST)) laser_fire(LASER_DOWN);
            else if (is_action_pressed(0, ACTION_ROTATE_LEFT))    laser_fire(LASER_LEFT);
            else if (is_action_pressed(0, ACTION_ROTATE_RIGHT))   laser_fire(LASER_RIGHT);
        }

        starfield_update();
        mothership_update();
        laser_update();

        if (mothership_is_landed()) {
            AsteroidResult result = asteroid_update();
            if (result == ASTEROID_LASER_HIT)
                score_add(10);
            else if (result == ASTEROID_MOTHERSHIP_HIT) {
                mothership_reset();
                asteroid_reset();
            }
        }

        }
    return 0;
}