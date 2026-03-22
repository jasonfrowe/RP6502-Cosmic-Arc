#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "beasties.h"
#include "lander.h"

#define BEASTIE_SIZE 8
#define BEASTIE_FRAME_SIZE (BEASTIE_SIZE * BEASTIE_SIZE * 2)
#define BEASTIE_TYPES 5
#define BEASTIE_FRAMES_PER_DIR 2
// BEASTIE_GROUND_Y is defined in constants.h
// Inner limits match the lander's surface zone so beasties can't escape to
// corners the lander can't reach.  Lander: x_min=12, x_max=308 (screen is 320x240).
#define BEASTIE_LEFT_LIMIT  12
#define BEASTIE_RIGHT_LIMIT (308 - BEASTIE_SIZE)  // 308: matches lander X max
#define BEASTIE_ANIM_TICKS 8

// Smart AI constants
#define LANDER_EVADE_RANGE  48   // beastie starts reacting this many px from lander
#define BEAM_EVADE_RANGE    64   // wider reaction zone when beam is active
#define SEP_MIN             10   // minimum px separation between beasties
#define ERRATIC_MIN          4   // min ticks between spontaneous direction changes
#define ERRATIC_RANGE       12   // random extra ticks added to min
#define WALL_ZONE           60   // px from wall where centre-drift bias kicks in

typedef struct {
    int16_t x;
    int8_t dx;
    uint8_t speed;           // 1=normal, 2=fast (alternates 2px/1px = 1.5px avg)
    uint8_t half_tick;       // LSB alternates to produce 1.5px movement
    uint8_t type;
    uint8_t frame;
    uint8_t tick;
    bool visible;
    bool paused;       // true while beam is capturing or after capture
    bool flee_locked;  // true while committed to a flee direction inside evade range
    uint8_t erratic_tick;   // countdown to next direction change
} Beastie;

static Beastie beastie_a;
static Beastie beastie_b;
static uint8_t spawn_type = 0;  // cycles through BEASTIE_TYPES across surface visits

// Simple LCG pseudo-random — drives erratic AI
static uint16_t prng_state = 0x1337u;
static uint8_t prng_next(void)
{
    prng_state = (uint16_t)(prng_state * 25173u + 13849u);
    return (uint8_t)(prng_state >> 4);
}

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
    beastie->paused = false;
    beastie->flee_locked = false;
    beastie->speed = 1;
    beastie->half_tick = 0;
    beastie->erratic_tick = 0;
}

// Demo-mode update: simple constant-speed wall-bounce.
static void update_one(Beastie* beastie, unsigned config, bool enabled)
{
    bool moving_right;

    if (!enabled || beastie->paused) {
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
    } else if (beastie->x >= BEASTIE_RIGHT_LIMIT) {
        beastie->x = BEASTIE_RIGHT_LIMIT;
        beastie->dx = -1;
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

// Planet-phase smart update: erratic wander, lander/beam flee, beastie separation.
static void update_smart(Beastie *beastie, unsigned config,
                         bool enabled, int16_t lander_cx,
                         bool beam_active, Beastie *other)
{
    bool moving_right;
    int16_t new_x;
    bool fleeing = false;

    if (!enabled || beastie->paused) {
        if (beastie->visible) {
            hide_beastie(config);
            beastie->visible = false;
        }
        return;
    }

    // Lander / beam avoidance — highest priority, always evaluated.
    if (lander_cx >= 0) {
        int16_t center = (int16_t)(beastie->x + 4);
        int16_t dist   = (int16_t)(center - lander_cx);
        if (dist < 0) dist = -dist;
        int16_t range  = beam_active ? BEAM_EVADE_RANGE : LANDER_EVADE_RANGE;
        if (dist < range) {
            if (!beastie->flee_locked) {
                // First frame inside range: pick one of three dodge behaviours.
                int8_t flee_away = (center < lander_cx) ? (int8_t)-1 : (int8_t)1;
                uint8_t r = prng_next() & 0x03u;
                if (r < 2u) {
                    // 50%: run away from lander; half the time at dash speed
                    beastie->dx = flee_away;
                    beastie->speed = (prng_next() & 1u) ? 2u : 1u;
                } else if (r == 2u) {
                    // 25%: cross-dash TOWARD lander to escape to the opposite side
                    beastie->dx = (int8_t)-flee_away;
                    beastie->speed = 2u;
                } else {
                    // 25%: direction reversal (erratic feint)
                    beastie->dx = (int8_t)-beastie->dx;
                    beastie->speed = (prng_next() & 1u) ? 2u : 1u;
                }
                beastie->erratic_tick = (uint8_t)(ERRATIC_MIN + (prng_next() % 8u));
                beastie->flee_locked = true;
            }
            // Hold committed direction until we escape — don't re-roll.
            fleeing = true;
        } else {
            beastie->flee_locked = false;  // exited range, allow re-commit next entry
        }
    }

    // Erratic wander: spontaneous direction changes when not actively fleeing.
    if (!fleeing) {
        if (beastie->erratic_tick == 0) {
            beastie->erratic_tick = (uint8_t)(ERRATIC_MIN + (prng_next() % ERRATIC_RANGE));
            // Bias toward centre when hugging a wall
            bool near_left  = (beastie->x < BEASTIE_LEFT_LIMIT  + WALL_ZONE);
            bool near_right = (beastie->x > BEASTIE_RIGHT_LIMIT - WALL_ZONE);
            uint8_t rw = prng_next();
            if (near_left && !near_right)
                beastie->dx = (rw < 192u) ? (int8_t)1 : (int8_t)-1;   // 75% toward centre
            else if (near_right && !near_left)
                beastie->dx = (rw < 192u) ? (int8_t)-1 : (int8_t)1;   // 75% toward centre
            else
                beastie->dx = (rw & 1u) ? (int8_t)1 : (int8_t)-1;     // centre zone: 50/50
            // Occasional short sprint during wander (~19% chance)
            beastie->speed = (prng_next() < 48u) ? 2u : 1u;
        } else {
            --beastie->erratic_tick;
        }
    }

    // Separation: don't let beasties stack.
    if (!other->paused) {
        int16_t sep = (int16_t)(beastie->x - other->x);
        if (sep < 0) sep = -sep;
        if (sep < SEP_MIN)
            beastie->dx = (beastie->x <= other->x) ? (int8_t)-1 : (int8_t)1;
    }

    // Apply movement with wall bounce.
    // speed==2: alternate 2px/1px each frame for a smooth ~1.5px average
    uint8_t move_px = (beastie->speed == 2u && (beastie->half_tick & 1u)) ? 2u : 1u;
    beastie->half_tick++;
    new_x = (int16_t)(beastie->x + beastie->dx * move_px);
    if (new_x <= BEASTIE_LEFT_LIMIT) {
        new_x = BEASTIE_LEFT_LIMIT;
        beastie->dx = 1;
        beastie->speed = 1;
        beastie->erratic_tick = ERRATIC_MIN;  // hold direction briefly; prevent wall jitter
    } else if (new_x >= BEASTIE_RIGHT_LIMIT) {
        new_x = BEASTIE_RIGHT_LIMIT;
        beastie->dx = -1;
        beastie->speed = 1;
        beastie->erratic_tick = ERRATIC_MIN;  // hold direction briefly; prevent wall jitter
    }
    beastie->x = new_x;

    if (++beastie->tick >= BEASTIE_ANIM_TICKS) {
        beastie->tick = 0;
        beastie->frame ^= 1u;
    }

    moving_right = (beastie->dx > 0);
    write_beastie(config, beastie->x, BEASTIE_GROUND_Y,
                  frame_ptr(beastie->type, moving_right, beastie->frame));
    beastie->visible = true;
}

void beasties_init(void)
{
    spawn_type = 0;
    reset_one(&beastie_a, 48, 1, 0);
    reset_one(&beastie_b, 248, -1, 0);

    hide_beastie(BEASTIE1_CONFIG);
    hide_beastie(BEASTIE2_CONFIG);
}

void beasties_reset(void)
{
    spawn_type = 0;
    reset_one(&beastie_a, 48, 1, 0);
    reset_one(&beastie_b, 248, -1, 0);

    hide_beastie(BEASTIE1_CONFIG);
    hide_beastie(BEASTIE2_CONFIG);
}

void beasties_update(bool enabled, bool smart, int16_t beam_center_x)
{
    if (smart) {
        int16_t lx, ly;
        bool beam = lander_is_beam_active();
        lander_get_pos(&lx, &ly);
        int16_t lander_cx = (int16_t)(lx + 8);
        update_smart(&beastie_a, BEASTIE1_CONFIG, enabled, lander_cx, beam, &beastie_b);
        update_smart(&beastie_b, BEASTIE2_CONFIG, enabled, lander_cx, beam, &beastie_a);
    } else {
        update_one(&beastie_a, BEASTIE1_CONFIG, enabled);
        update_one(&beastie_b, BEASTIE2_CONFIG, enabled);
    }
}

void beasties_hide_all(void)
{
    beastie_a.paused = true;
    beastie_b.paused = true;
    hide_beastie(BEASTIE1_CONFIG);
    hide_beastie(BEASTIE2_CONFIG);
    beastie_a.visible = false;
    beastie_b.visible = false;
}

// Spawn 'count' beasties (0-2). Unspawned beasties stay hidden/paused.
void beasties_spawn(uint8_t count)
{
    beastie_a.paused = true;
    beastie_b.paused = true;
    hide_beastie(BEASTIE1_CONFIG);
    hide_beastie(BEASTIE2_CONFIG);
    beastie_a.visible = false;
    beastie_b.visible = false;
    if (count >= 1) reset_one(&beastie_a, 48,  1, spawn_type);
    if (count >= 2) reset_one(&beastie_b, 248, -1, spawn_type);
}

void beasties_advance_type(void)
{
    spawn_type = (uint8_t)((spawn_type + 1u) % BEASTIE_TYPES);
}

// Returns x of beastie 0=A or 1=B, or -1 if paused/captured.
int16_t beasties_get_x(uint8_t idx)
{
    Beastie *b = (idx == 0) ? &beastie_a : &beastie_b;
    return b->paused ? (int16_t)-1 : b->x;
}

void beasties_set_paused(uint8_t idx, bool paused)
{
    Beastie *b = (idx == 0) ? &beastie_a : &beastie_b;
    b->paused = paused;
}
