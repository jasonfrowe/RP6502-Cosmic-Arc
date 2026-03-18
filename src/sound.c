#include <stdbool.h>
#include <stdint.h>
#include "instruments.h"
#include "opl.h"
#include "sound.h"

#define SFX_DESCENT_CH 6
#define SFX_LASER_CH   7
#define SFX_EVENT_CH   8

#define DESCENT_START_DELAY_TICKS 30
#define DESCENT_RETRIGGER_TICKS   6
#define ASTEROID_RETRIGGER_TICKS  12
#define LASER_TOTAL_TICKS         10
#define DESTRUCTION_TOTAL_TICKS   180

static const OPL_Patch descent_patch = {
    .m_ave = 0x31, .m_ksl = 0x24, .m_atdec = 0xF2, .m_susrel = 0xF6, .m_wave = 0x00,
    .c_ave = 0x21, .c_ksl = 0x00, .c_atdec = 0xE2, .c_susrel = 0xF4, .c_wave = 0x00,
    .feedback = 0x0E,
};

static const OPL_Patch laser_patch = {
    .m_ave = 0x41, .m_ksl = 0x0C, .m_atdec = 0xF8, .m_susrel = 0x44, .m_wave = 0x03,
    .c_ave = 0x01, .c_ksl = 0x00, .c_atdec = 0xF4, .c_susrel = 0x32, .c_wave = 0x00,
    .feedback = 0x0E,
};

static const OPL_Patch asteroid_patch = {
    .m_ave = 0x21, .m_ksl = 0x1D, .m_atdec = 0xF3, .m_susrel = 0xF8, .m_wave = 0x00,
    .c_ave = 0x01, .c_ksl = 0x00, .c_atdec = 0xD2, .c_susrel = 0xE7, .c_wave = 0x00,
    .feedback = 0x0A,
};

static const OPL_Patch destruction_tone_patch = {
    .m_ave = 0x11, .m_ksl = 0x14, .m_atdec = 0xF4, .m_susrel = 0xC8, .m_wave = 0x00,
    .c_ave = 0x21, .c_ksl = 0x00, .c_atdec = 0xF3, .c_susrel = 0xC6, .c_wave = 0x00,
    .feedback = 0x0E,
};

static const OPL_Patch klaxon_patch = {
    .m_ave = 0x21, .m_ksl = 0x00, .m_atdec = 0xFF, .m_susrel = 0x0F, .m_wave = 0x01,
    .c_ave = 0x21, .c_ksl = 0x00, .c_atdec = 0xFF, .c_susrel = 0x0F, .c_wave = 0x01,
    .feedback = 0x0E,
};

static uint8_t descent_tick;
static uint8_t descent_phase;
static uint8_t descent_delay;
static bool descent_enabled;

static uint8_t asteroid_tick;
static uint8_t asteroid_phase;
static bool asteroid_enabled;

static uint8_t laser_timer;
static uint8_t destruction_timer;
static uint8_t destruction_phase;
static uint16_t klaxon_timer;

static void sfx_note_on(uint8_t channel, const OPL_Patch* patch, uint8_t note, uint8_t volume)
{
    OPL_NoteOff(channel);
    OPL_SetPatch(channel, patch);
    OPL_SetVolume(channel, volume);
    OPL_NoteOn(channel, note);
}

static void stop_channel(uint8_t channel)
{
    OPL_NoteOff(channel);
}

void sound_init(void)
{
    descent_tick = 0;
    descent_phase = 0;
    descent_delay = 0;
    descent_enabled = false;
    asteroid_tick = 0;
    asteroid_phase = 0;
    asteroid_enabled = false;
    laser_timer = 0;
    destruction_timer = 0;
    destruction_phase = 0;
    klaxon_timer = 0;

    stop_channel(SFX_DESCENT_CH);
    stop_channel(SFX_LASER_CH);
    stop_channel(SFX_EVENT_CH);
}

void sound_play_laser(void)
{
    laser_timer = LASER_TOTAL_TICKS;
    sfx_note_on(SFX_LASER_CH, &laser_patch, 86, 127);
}

void sound_play_destruction(void)
{
    destruction_timer = DESTRUCTION_TOTAL_TICKS;
    destruction_phase = 0;
    descent_delay = 0;
    descent_enabled = false;
    asteroid_enabled = false;
    stop_channel(SFX_DESCENT_CH);
    stop_channel(SFX_EVENT_CH);

    // Front-load the impact, then let the rest of the sequence dissolve.
    sfx_note_on(SFX_EVENT_CH, &drum_snare, 64, 127);
}

void sound_play_klaxon(void)
{
    klaxon_timer = 4 * 60; // 4 seconds at 60Hz
}

static void update_klaxon_sound(void)
{
    if (klaxon_timer == 0) return;

    if ((klaxon_timer % 15) == 0) {
        uint8_t note = ((klaxon_timer / 15) % 2 == 0) ? 60 : 63;
        sfx_note_on(SFX_EVENT_CH, &klaxon_patch, note, 127);
    }

    --klaxon_timer;
    if (klaxon_timer == 0) {
        stop_channel(SFX_EVENT_CH);
    }
}

static void update_laser_sound(void)
{
    if (laser_timer == 0)
        return;

    if (laser_timer == 7) {
        sfx_note_on(SFX_LASER_CH, &laser_patch, 74, 118);
    } else if (laser_timer == 4) {
        sfx_note_on(SFX_LASER_CH, &laser_patch, 62, 100);
    } else if (laser_timer == 1) {
        stop_channel(SFX_LASER_CH);
    }

    --laser_timer;
}

static void update_destruction_sound(void)
{
    uint8_t progress;
    uint8_t tone_volume;

    if (destruction_timer == 0)
        return;

    progress = (uint8_t)(((unsigned)(DESTRUCTION_TOTAL_TICKS - destruction_timer) * 127u) /
                         (DESTRUCTION_TOTAL_TICKS - 1u));
    tone_volume = (uint8_t)(127u - progress);

    if ((destruction_timer % 8u) == 0u) {
        uint8_t note = (uint8_t)(72u - (destruction_phase & 0x0Fu));
        if (tone_volume < 16u)
            tone_volume = 16u;
        sfx_note_on(SFX_DESCENT_CH, &destruction_tone_patch, note, tone_volume);
        ++destruction_phase;
    }

    if (destruction_timer == (DESTRUCTION_TOTAL_TICKS - 10u) ||
        destruction_timer == (DESTRUCTION_TOTAL_TICKS - 24u) ||
        destruction_timer == (DESTRUCTION_TOTAL_TICKS - 40u)) {
        uint8_t noise_volume = (uint8_t)(127u - (progress / 2u));
        const OPL_Patch* patch = (destruction_phase & 1u) ? &drum_hihat : &drum_snare;
        if (noise_volume < 24u)
            noise_volume = 24u;
        sfx_note_on(SFX_EVENT_CH, patch, (uint8_t)(60u - (destruction_phase & 0x03u)), noise_volume);
    }

    if (destruction_timer == 1u) {
        stop_channel(SFX_DESCENT_CH);
        stop_channel(SFX_EVENT_CH);
    }

    --destruction_timer;
}

static void update_descent_sound(bool mothership_descending)
{
    if (!mothership_descending) {
        if (descent_enabled) {
            stop_channel(SFX_DESCENT_CH);
            descent_enabled = false;
        }
        descent_delay = 0;
        return;
    }

    if (!descent_enabled) {
        descent_enabled = true;
        descent_tick = 0;
        descent_phase = 0;
        descent_delay = DESCENT_START_DELAY_TICKS;
    }

    if (descent_delay > 0u) {
        --descent_delay;
        return;
    }

    if (descent_tick == 0u) {
        uint8_t note = (uint8_t)(67u - (descent_phase & 0x07u));
        sfx_note_on(SFX_DESCENT_CH, &descent_patch, note, 127);
        descent_phase = (uint8_t)((descent_phase + 1u) & 0x0Fu);
    }

    descent_tick = (uint8_t)((descent_tick + 1u) % DESCENT_RETRIGGER_TICKS);
}

static void update_asteroid_sound(bool asteroid_present)
{
    if (!asteroid_present) {
        if (asteroid_enabled) {
            stop_channel(SFX_EVENT_CH);
            asteroid_enabled = false;
        }
        return;
    }

    if (!asteroid_enabled) {
        asteroid_enabled = true;
        asteroid_tick = 0;
        asteroid_phase = 0;
    }

    if (asteroid_tick == 0u) {
        uint8_t note = (uint8_t)(44u + (asteroid_phase & 0x03u));
        sfx_note_on(SFX_EVENT_CH, &asteroid_patch, note, 127);
        asteroid_phase = (uint8_t)((asteroid_phase + 1u) & 0x07u);
    }

    asteroid_tick = (uint8_t)((asteroid_tick + 1u) % ASTEROID_RETRIGGER_TICKS);
}

void sound_update(bool mothership_descending, bool asteroid_present)
{
    update_laser_sound();

    if (destruction_timer > 0u) {
        update_destruction_sound();
        return;
    }

    update_klaxon_sound();
    update_descent_sound(mothership_descending);
    update_asteroid_sound(asteroid_present);
}