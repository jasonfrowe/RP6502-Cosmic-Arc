#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>

void sound_init(void);
void sound_update(bool mothership_descending, bool asteroid_present);
void sound_play_laser(void);
void sound_play_destruction(void);
void sound_play_klaxon(void);
// Lander surface sounds — call every frame with current state
void sound_set_lander_motor(bool on);
void sound_set_beam(bool on);
void sound_play_beastie_aboard(void);
void sound_play_mothership_appear(void);
void sound_play_mothership_depart(void);
void sound_skip_descent_delay(void);
void sound_play_defense_pulse(void);     // sharp hit when tower fires
void sound_play_lander_death(void);      // quick descending sweep on lander kill


#endif // SOUND_H