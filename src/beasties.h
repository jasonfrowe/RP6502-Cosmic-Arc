#ifndef BEASTIES_H
#define BEASTIES_H

#include <stdbool.h>

void beasties_init(void);
void beasties_reset(void);
void beasties_hide_all(void);        // hide and pause both beasties immediately
void beasties_spawn(uint8_t count); // spawn 0-2 beasties; unspawned stay hidden
void beasties_update(bool enabled);

// Returns x of beastie idx (0=A, 1=B), or -1 if paused/captured.
int16_t beasties_get_x(uint8_t idx);
// Pause (hide) or resume a beastie.
void beasties_set_paused(uint8_t idx, bool paused);

#endif // BEASTIES_H
