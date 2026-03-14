#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Sprite data configuration
#define SPRITE_DATA_START       0x0000U // Starting address in XRAM for sprite data
#define COSMICARC_DATA          (SPRITE_DATA_START) // Address for CosmicArc sprite data
#define COSMICARC_DATA_SIZE     0x2000U // Size of CosmicArc sprite data (8KB)
#define SPRITE_DATA_END         (SPRITE_DATA_START + COSMICARC_DATA_SIZE) // End address for sprite data

// 5. Keyboard, Gamepad and Sound
// -------------------------------------------------------------------------
#define OPL_ADDR        0xFE00  // OPL2 Address port
#define PALETTE_ADDR    0xFF58  // XRAM address for palette data
#define GAMEPAD_INPUT   0xFF78  // XRAM address for gamepad data
#define KEYBOARD_INPUT  0xFFA0  // XRAM address for keyboard data
#define PSG_XRAM_ADDR   0xFFC0  // PSG memory location (must match sound.c)

extern unsigned COSMICARC_CONFIG; // Configuration byte for CosmicArc graphics asset 

#endif // CONSTANTS_H