#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite data configuration
#define SPRITE_DATA_START       0x0000U // Starting address in XRAM for sprite data
#define COSMICARC_DATA          (SPRITE_DATA_START) // Address for CosmicArc sprite data
#define COSMICARC_DATA_SIZE     0x2000U // Size of CosmicArc sprite data (4 parts * 32x32 * 2 bytes = 16KB)
#define STAR_MAP_DATA           (COSMICARC_DATA + COSMICARC_DATA_SIZE) // Address for Star Map tile data
#define STAR_MAP_DATA_SIZE      0x0300U // Size of Star Map tile data (6 tiles * 16x16 / 2 bytes = 768 bytes)
#define STAR_MAP_TILEMAP_DATA   (STAR_MAP_DATA + STAR_MAP_DATA_SIZE) // Address for Star Map tilemap data
#define STAR_MAP_TILEMAP_SIZE   0x012CU // Size of Star Map tilemap data (300 bytes)
#define SPRITE_DATA_END         (SPRITE_DATA_START + COSMICARC_DATA_SIZE + STAR_MAP_DATA_SIZE + STAR_MAP_TILEMAP_SIZE) // End address for sprite data

// Star Map configuration
#define STAR_MAP_WIDTH_TILES 20
#define STAR_MAP_HEIGHT_TILES 15



// Keyboard, Gamepad and Sound
// -------------------------------------------------------------------------
#define OPL_ADDR        0xFE00  // OPL2 Address port
#define PALETTE_ADDR    0xFF58  // XRAM address for palette data
#define GAMEPAD_INPUT   0xFF78  // XRAM address for gamepad data
#define KEYBOARD_INPUT  0xFFA0  // XRAM address for keyboard data
#define PSG_XRAM_ADDR   0xFFC0  // PSG memory location (must match sound.c)

// Palette configuration
// -------------------------------------------------------------------------
#define PALETTE_ADDR 0xFF58

extern unsigned COSMICARC_CONFIG; // Configuration byte for CosmicArc graphics asset 
extern unsigned STAR_MAP_CONFIG;  // Configuration byte for Star Map tile asset

#endif // CONSTANTS_H