#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define SPACE_HEIGHT 192

// Sprite data configuration
#define SPRITE_DATA_START       0x0000U // Starting address in XRAM for sprite data

#define MAIN_MAP_DATA           (SPRITE_DATA_START) // Address for main tile bitmap data
#define MAIN_MAP_DATA_SIZE      0x2FC0 // 12224 bytes (191 tiles * 8x8 * 8bpp)

#define MAIN_MAP_TILEMAP_DATA   (MAIN_MAP_DATA + MAIN_MAP_DATA_SIZE) // Address for Main Map tilemap data
#define MAIN_MAP_TILEMAP_SIZE   0x04B0U // 1200 bytes (40 * 30 tile IDs)

#define MOTHERSHIP_MAP_TILEMAP_DATA   (MAIN_MAP_TILEMAP_DATA + MAIN_MAP_TILEMAP_SIZE) // Address for Main Map tilemap data
#define MOTHERSHIP_MAP_TILEMAP_SIZE   0x04B0U // 1200 bytes (40 * 30 tile IDs)

#define LASER_DATA              (MOTHERSHIP_MAP_TILEMAP_DATA + MOTHERSHIP_MAP_TILEMAP_SIZE) // Address for laser sprite data
#define LASER_DATA_SIZE         0x1000U // Size of laser sprite data (2 parts * 32x32 * 2 bytes = 8KB)

#define ASTEROID_DATA           (LASER_DATA + LASER_DATA_SIZE) // Address for asteroid sprite data
#define ASTEROID_DATA_SIZE      0x0200U // Size of asteroid sprite data (2 parts * 8x8 * 2 bytes = 512 bytes)

#define SPRITE_DATA_END         (ASTEROID_DATA + ASTEROID_DATA_SIZE) // End address for sprite data

// Main Map configuration
#define MAIN_MAP_WIDTH_TILES 40
#define MAIN_MAP_HEIGHT_TILES 30


// Keyboard, Gamepad and Sound
// -------------------------------------------------------------------------
#define PALETTE_ADDR    0xFC00  // 256-color palette (512 bytes, 0xFC00-0xFDFF)
#define PALETTE_SIZE    0x0200

#define OPL_ADDR        0xFE00  // OPL2 register page (256 bytes, must be page-aligned)
#define OPL_SIZE        0x0100

// RIA input buffers are provided at fixed XRAM addresses.
#define GAMEPAD_INPUT   0xFF78  // 40 bytes for 4 gamepads
#define KEYBOARD_INPUT  0xFFA0  // 32 bytes keyboard bitfield

extern unsigned MAIN_MAP_CONFIG;     // Configuration struct address for Main Map
extern unsigned MOTHERSHIP_CONFIG;   // Configuration struct address for Mothership
extern unsigned LASER_CONFIG;        // Configuration struct address for laser sprite
extern unsigned ASTEROID_CONFIG;     // Configuration struct address for asteroid sprite

#define LASER_SPRITE_LOG_SIZE 5 // log2 of sprite size in pixels (32x32)

// Mothership center on screen
#define MOTHERSHIP_X     160
#define MOTHERSHIP_Y     98

#endif // CONSTANTS_H