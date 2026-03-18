#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>

void sound_init(void);
void sound_update(bool mothership_descending, bool asteroid_present);
void sound_play_laser(void);
void sound_play_destruction(void);
void sound_play_klaxon(void);


#endif // SOUND_H