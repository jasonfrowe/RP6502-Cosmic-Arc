#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite data configuration
#define SPRITE_DATA_START       0x0000U // Starting address in XRAM for sprite data

#define MAIN_MAP_DATA           (SPRITE_DATA_START) // Address for main tile bitmap data
#define MAIN_MAP_DATA_SIZE      0x19C0U // 6592 bytes (103 tiles * 8x8 * 8bpp)

#define MAIN_MAP_TILEMAP_DATA   (MAIN_MAP_DATA + MAIN_MAP_DATA_SIZE) // Address for Main Map tilemap data
#define MAIN_MAP_TILEMAP_SIZE   0x04B0U // 1200 bytes (40 * 30 tile IDs)

#define MOTHERSHIP_MAP_TILEMAP_DATA   (MAIN_MAP_TILEMAP_DATA + MAIN_MAP_TILEMAP_SIZE) // Address for Main Map tilemap data
#define MOTHERSHIP_MAP_TILEMAP_SIZE   0x04B0U // 1200 bytes (40 * 30 tile IDs)

#define SPRITE_DATA_END         (MOTHERSHIP_MAP_TILEMAP_DATA + MOTHERSHIP_MAP_TILEMAP_SIZE) // End address for sprite data

// Main Map configuration
#define MAIN_MAP_WIDTH_TILES 40
#define MAIN_MAP_HEIGHT_TILES 30


// Keyboard, Gamepad and Sound
// -------------------------------------------------------------------------
#define PALETTE_ADDR    0xFC00  // 256-color palette (512 bytes, 0xFC00-0xFDFF)
#define PALETTE_SIZE    0x0200

#define OPL_ADDR        0xFE00  // OPL2 register page (256 bytes, must be page-aligned)
#define OPL_SIZE        0x0100

// Input buffers can live anywhere in XRAM; keep them away from palette/OPL regions.
#define GAMEPAD_INPUT   0xFA00  // 40 bytes for 4 gamepads
#define KEYBOARD_INPUT  0xFA40  // 32 bytes keyboard bitfield

extern unsigned MAIN_MAP_CONFIG;  // Configuration byte for Main Map tile asset

#endif // CONSTANTS_H