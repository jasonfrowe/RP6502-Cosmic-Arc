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


#endif // SOUND_H