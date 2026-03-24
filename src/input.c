#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "usb_hid_keys.h"
#include "constants.h"
#include "input.h"


// Button mapping storage
ButtonMapping button_mappings[GAMEPAD_COUNT][ACTION_COUNT];

// Gamepad state structure
gamepad_t gamepad[GAMEPAD_COUNT];

// Keyboard state (defined in definitions.h, but declared here)
uint8_t keystates[KEYBOARD_BYTES] = {0};
bool handled_key = false;

// Helper for checking if any input is pressed (mainly for demo mode)
bool is_any_input_pressed(void) {
    // Check all relevant bits
    if (is_action_pressed(0, ACTION_THRUST)) return true;
    if (is_action_pressed(0, ACTION_REVERSE_THRUST)) return true;
    if (is_action_pressed(0, ACTION_ROTATE_LEFT)) return true;
    if (is_action_pressed(0, ACTION_ROTATE_RIGHT)) return true;
    if (is_action_pressed(0, ACTION_FIRE)) return true;
    if (is_action_pressed(0, ACTION_SUPER_FIRE)) return true;
    if (is_action_pressed(0, ACTION_PAUSE)) return true;
    if (is_action_pressed(0, ACTION_RESCUE)) return true;
    return false;
}

/**
 * Reset to default button mappings for a specific player
 */
void reset_button_mappings(uint8_t player_id)
{
    if (player_id >= GAMEPAD_COUNT) return;

    // Zero out all mappings for this player so secondary fields default to 0 (disabled)
    memset(&button_mappings[player_id], 0, sizeof(button_mappings[player_id]));

    // ACTION_THRUST: Up Arrow, Left Stick Up, or D-Pad Up
    button_mappings[player_id][ACTION_THRUST].keyboard_key = KEY_UP;
    button_mappings[player_id][ACTION_THRUST].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_THRUST].gamepad_mask = GP_LSTICK_UP;
    button_mappings[player_id][ACTION_THRUST].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_THRUST].gamepad_mask2 = GP_DPAD_UP;

    // ACTION_REVERSE_THRUST: Down Arrow, Left Stick Down, or D-Pad Down
    button_mappings[player_id][ACTION_REVERSE_THRUST].keyboard_key = KEY_DOWN;
    button_mappings[player_id][ACTION_REVERSE_THRUST].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_REVERSE_THRUST].gamepad_mask = GP_LSTICK_DOWN;
    button_mappings[player_id][ACTION_REVERSE_THRUST].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_REVERSE_THRUST].gamepad_mask2 = GP_DPAD_DOWN;

    // ACTION_ROTATE_LEFT: Left Arrow, Left Stick Left, or D-Pad Left
    button_mappings[player_id][ACTION_ROTATE_LEFT].keyboard_key = KEY_LEFT;
    button_mappings[player_id][ACTION_ROTATE_LEFT].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_ROTATE_LEFT].gamepad_mask = GP_LSTICK_LEFT;
    button_mappings[player_id][ACTION_ROTATE_LEFT].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_ROTATE_LEFT].gamepad_mask2 = GP_DPAD_LEFT;

    // ACTION_ROTATE_RIGHT: Right Arrow, Left Stick Right, or D-Pad Right
    button_mappings[player_id][ACTION_ROTATE_RIGHT].keyboard_key = KEY_RIGHT;
    button_mappings[player_id][ACTION_ROTATE_RIGHT].gamepad_button = GP_FIELD_STICKS;
    button_mappings[player_id][ACTION_ROTATE_RIGHT].gamepad_mask = GP_LSTICK_RIGHT;
    button_mappings[player_id][ACTION_ROTATE_RIGHT].gamepad_button2 = GP_FIELD_DPAD;
    button_mappings[player_id][ACTION_ROTATE_RIGHT].gamepad_mask2 = GP_DPAD_RIGHT;
    
    // ACTION_FIRE: Space or A button
    button_mappings[player_id][ACTION_FIRE].keyboard_key = KEY_SPACE;
    button_mappings[player_id][ACTION_FIRE].gamepad_button = GP_FIELD_BTN0; // btn0 field
    button_mappings[player_id][ACTION_FIRE].gamepad_mask = GP_BTN_A;
    
    // ACTION_SUPER_FIRE: C key or B button (for sbullets)
    button_mappings[player_id][ACTION_SUPER_FIRE].keyboard_key = KEY_X;
    button_mappings[player_id][ACTION_SUPER_FIRE].gamepad_button = GP_FIELD_BTN0; // btn0 field
    button_mappings[player_id][ACTION_SUPER_FIRE].gamepad_mask = GP_BTN_B;

    // ACTION_ALT_FIRE: X button (for sbullets)
    button_mappings[player_id][ACTION_ALT_FIRE].keyboard_key = KEY_V;
    button_mappings[player_id][ACTION_ALT_FIRE].gamepad_button = GP_FIELD_BTN0; // btn0 field
    button_mappings[player_id][ACTION_ALT_FIRE].gamepad_mask = GP_BTN_X;
    
    // ACTION_PAUSE: START button
    button_mappings[player_id][ACTION_PAUSE].keyboard_key = KEY_ENTER;
    button_mappings[player_id][ACTION_PAUSE].gamepad_button = GP_FIELD_BTN1; // btn1 field
    button_mappings[player_id][ACTION_PAUSE].gamepad_mask = GP_BTN_START;

    // ACTION_RESCUE: B button
    button_mappings[player_id][ACTION_RESCUE].keyboard_key = KEY_C;
    button_mappings[player_id][ACTION_RESCUE].gamepad_button = GP_FIELD_BTN0; // btn0 field
    button_mappings[player_id][ACTION_RESCUE].gamepad_mask = GP_BTN_Y;
}

/**
 * Load joystick configuration from JOYSTICK_CA.DAT (preferred)
 * with fallback to JOYSTICK.DAT for compatibility.
 * Returns true if successful, false otherwise.
 */
bool load_joystick_config(void)
{
    // Ensure this struct matches the one used in gamepad_mapper.c exactly
    typedef struct {
        uint8_t action_id;  // This is now the actual GameAction enum value
        uint8_t field;      // 0=dpad, 1=sticks, 2=btn0, 3=btn1
        uint8_t mask;       // Bit mask
    } JoystickMapping;
    
    int fd = open("JOYSTICK_CA.DAT", O_RDONLY);
    if (fd < 0) {
        fd = open("JOYSTICK.DAT", O_RDONLY);
    }
    if (fd < 0) {
        return false;  // File doesn't exist
    }
    
    // Read number of mappings (first byte of the file)
    uint8_t num_mappings = 0;
    if (read(fd, &num_mappings, 1) != 1) {
        close(fd);
        return false;
    }
    
    // Read all mapping structures
    // We expect 9 mappings based on the new tool (UP, DOWN, LEFT, RIGHT, A, B, X, Y, START)
    JoystickMapping file_mappings[10]; 
    int bytes_to_read = num_mappings * sizeof(JoystickMapping);
    if (read(fd, file_mappings, bytes_to_read) != bytes_to_read) {
        close(fd);
        return false;
    }
    
    close(fd);
    
    // Initialize all to defaults first to ensure a clean state
    for (uint8_t player = 0; player < GAMEPAD_COUNT; player++) {
        reset_button_mappings(player);
    }
    
    // Apply loaded mappings for player 0
    for (uint8_t i = 0; i < num_mappings; i++) {
        uint8_t action_id = file_mappings[i].action_id;
        
        // Safety: ensure the action_id from the file is a valid index for our array
        // (Assuming ACTION_COUNT is the last element of your GameAction enum)
        if (action_id < ACTION_COUNT) {
            button_mappings[0][action_id].gamepad_button = file_mappings[i].field;
            button_mappings[0][action_id].gamepad_mask = file_mappings[i].mask;
        }
    }
    
    return true;
}

/**
 * Initialize input system with default button mappings
 */
void init_input_system(void)
{
    // Try to load joystick configuration from file
    if (!load_joystick_config()) {
        // If file doesn't exist or fails to load, use defaults
        for (uint8_t player = 0; player < GAMEPAD_COUNT; player++) {
            reset_button_mappings(player);
        }
    }
}

/**
 * Read keyboard and gamepad input
 */
void handle_input(void)
{
    // Read all keyboard state bytes
    RIA.addr0 = KEYBOARD_INPUT;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < KEYBOARD_BYTES; i++) {
        keystates[i] = RIA.rw0;
    }
    
    // Read gamepad data
    RIA.addr0 = GAMEPAD_INPUT;
    RIA.step0 = 1;
    for (uint8_t i = 0; i < GAMEPAD_COUNT; i++) {
        gamepad[i].dpad = RIA.rw0;
        gamepad[i].sticks = RIA.rw0;
        gamepad[i].btn0 = RIA.rw0;
        gamepad[i].btn1 = RIA.rw0;
        gamepad[i].lx = RIA.rw0;
        gamepad[i].ly = RIA.rw0;
        gamepad[i].rx = RIA.rw0;
        gamepad[i].ry = RIA.rw0;
        gamepad[i].l2 = RIA.rw0;
        gamepad[i].r2 = RIA.rw0;
    }
    
}

/**
 * Check if a game action's keyboard key is pressed (ignores gamepad).
 * Useful when gamepad direction is handled separately via raw analog values.
 */
bool is_keyboard_action_pressed(GameAction action)
{
    if (action >= ACTION_COUNT) return false;
    return key(button_mappings[0][action].keyboard_key) != 0;
}

/**
 * Check if a game action is active for a specific player
 */
bool is_action_pressed(uint8_t player_id, GameAction action)
{
    if (player_id >= GAMEPAD_COUNT || action >= ACTION_COUNT) {
        return false;
    }
    
    ButtonMapping* mapping = &button_mappings[player_id][action];
    
    // Check keyboard (player 0 only for now)
    if (player_id == 0) {
        if (key(mapping->keyboard_key)) {
            return true;
        }
    }
    
    // Only check gamepad if one is connected
    if (!(gamepad[player_id].dpad & GP_CONNECTED)) {
        return false;
    }
    
    // Check primary gamepad mapping
    uint8_t gamepad_value = 0;
    switch (mapping->gamepad_button) {
        case 0: gamepad_value = gamepad[player_id].dpad; break;
        case 1: gamepad_value = gamepad[player_id].sticks; break;
        case 2: gamepad_value = gamepad[player_id].btn0; break;
        case 3: gamepad_value = gamepad[player_id].btn1; break;
    }
    if (gamepad_value & mapping->gamepad_mask) return true;

    // Check secondary gamepad mapping (e.g. D-pad alongside analog stick)
    if (mapping->gamepad_mask2 != 0) {
        uint8_t gamepad_value2 = 0;
        switch (mapping->gamepad_button2) {
            case 0: gamepad_value2 = gamepad[player_id].dpad; break;
            case 1: gamepad_value2 = gamepad[player_id].sticks; break;
            case 2: gamepad_value2 = gamepad[player_id].btn0; break;
            case 3: gamepad_value2 = gamepad[player_id].btn1; break;
        }
        return (gamepad_value2 & mapping->gamepad_mask2) != 0;
    }

    return false;
}