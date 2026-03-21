#ifndef LANDER_H
#define LANDER_H

#include <stdbool.h>

void lander_init(void);
void lander_reset(void);
void lander_update(bool planet_phase);
bool lander_is_active(void);
// Returns true once after a beastie has been successfully captured.
bool lander_consume_beastie_captured(void);

#endif // LANDER_H