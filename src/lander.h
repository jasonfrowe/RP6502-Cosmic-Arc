#ifndef LANDER_H
#define LANDER_H

#include <stdbool.h>

void lander_init(void);
void lander_reset(void);
void lander_update(bool planet_phase);
bool lander_is_active(void);
void lander_get_pos(int16_t *x, int16_t *y);
// Returns true once after the lander docks carrying beasties.
// *count receives the number of beasties fully delivered to the mothership.
bool lander_consume_docked_beasties(uint8_t *count);

#endif // LANDER_H