#ifndef DEFENSE_H
#define DEFENSE_H

#include <stdbool.h>

// One-time state clear (called implicitly by defense_reset).
void defense_init(void);
// Set difficulty level: faster towers and more frequent pulses.
void defense_set_level(uint8_t level);
// Draw towers and guns after terrain tiles are restored on planet entry.
void defense_reset(void);
// Erase all defense tiles when leaving the planet phase.
void defense_hide(void);
// Per-frame update: animates towers, guns, and pulse.
// Returns true on the frame the pulse hits an active lander.
bool defense_update(void);

#endif // DEFENSE_H
