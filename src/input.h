#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"

#define GAMEPAD_COUNT 4       // Support up to 4 gamepads
#define KEYBOARD_BYTES  32    // 32 bytes for 256 key states

// Hardware button bit masks - DPAD
#define GP_DPAD_UP        0x01
#define GP_DPAD_DOWN      0x02
#define GP_DPAD_LEFT      0x04
#define GP_DPAD_RIGHT     0x08
#define GP_SONY           0x40  // Sony button faces (Circle/Cross/Square/Triangle)
#define GP_CONNECTED      0x80  // Gamepad is connected

// Hardware button bit masks - ANALOG STICKS
#define GP_LSTICK_UP      0x01
#define GP_LSTICK_DOWN    0x02
#define GP_LSTICK_LEFT    0x04
#define GP_LSTICK_RIGHT   0x08
#define GP_RSTICK_UP      0x10
#define GP_RSTICK_DOWN    0x20
#define GP_RSTICK_LEFT    0x40
#define GP_RSTICK_RIGHT   0x80

// Hardware button bit masks - BTN0 (Face buttons and shoulders)
// Per RP6502 documentation: https://picocomputer.github.io/ria.html#gamepads
#define GP_BTN_A          0x01  // bit 0: A or Cross
#define GP_BTN_B          0x02  // bit 1: B or Circle
#define GP_BTN_C          0x04  // bit 2: C or Right Paddle
#define GP_BTN_X          0x08  // bit 3: X or Square
#define GP_BTN_Y          0x10  // bit 4: Y or Triangle
#define GP_BTN_Z          0x20  // bit 5: Z or Left Paddle
#define GP_BTN_L1         0x40  // bit 6: L1
#define GP_BTN_R1         0x80  // bit 7: R1

// Hardware button bit masks - BTN1 (Triggers and special buttons)
#define GP_BTN_L2         0x01  // bit 0: L2
#define GP_BTN_R2         0x02  // bit 1: R2
#define GP_BTN_SELECT     0x04  // bit 2: Select/Back
#define GP_BTN_START      0x08  // bit 3: Start/Menu
#define GP_BTN_HOME       0x10  // bit 4: Home button
#define GP_BTN_L3         0x20  // bit 5: L3
#define GP_BTN_R3         0x40  // bit 6: R3

// ============================================================================
// BUTTON MAPPING SYSTEM
// ============================================================================

// Keyboard state array and macro
extern uint8_t keystates[KEYBOARD_BYTES];
extern bool handled_key;

// Macro to check if a key is pressed
#define key(code) (keystates[code >> 3] & (1 << (code & 7)))

// Game actions that can be mapped
typedef enum {
    ACTION_THRUST,
    ACTION_REVERSE_THRUST,
    ACTION_ROTATE_LEFT,
    ACTION_ROTATE_RIGHT,
    ACTION_FIRE,
    ACTION_SUPER_FIRE,
    ACTION_ALT_FIRE,
    ACTION_RESCUE,
    ACTION_PAUSE,
    ACTION_COUNT  // Total number of actions
} GameAction;

// Gamepad structure (10 bytes per gamepad)
typedef struct {
    uint8_t dpad;      // Direction pad + status bits
    uint8_t sticks;    // Left and right stick digital directions
    uint8_t btn0;      // Face buttons and shoulders
    uint8_t btn1;      // L2/R2/Select/Start/Home/L3/R3
    int8_t lx;         // Left stick X analog (-128 to 127)
    int8_t ly;         // Left stick Y analog (-128 to 127)
    int8_t rx;         // Right stick X analog (-128 to 127)
    int8_t ry;         // Right stick Y analog (-128 to 127)
    uint8_t l2;        // Left trigger analog (0-255)
    uint8_t r2;        // Right trigger analog (0-255)
} gamepad_t;

// Button mapping structure
typedef struct {
    uint8_t keyboard_key;     // USB HID keycode
    uint8_t gamepad_button;   // Which gamepad field (0=dpad, 1=sticks, 2=btn0, 3=btn1)
    uint8_t gamepad_mask;     // Bit mask for the button
} ButtonMapping;

// Gamepad Field Offsets (for ButtonMapping.gamepad_button)
#define GP_FIELD_DPAD    0  // D-Pad and Status
#define GP_FIELD_STICKS  1  // Digital Sticks
#define GP_FIELD_BTN0    2  // Face Buttons (A,B,X,Y)
#define GP_FIELD_BTN1    3  // Triggers/Select/Start

extern void init_input_system(void);
extern void handle_input(void);
extern bool is_action_pressed(uint8_t player_id, GameAction action);
extern bool is_any_input_pressed(void);

#endif // INPUT_H
