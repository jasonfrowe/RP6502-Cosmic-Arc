#ifndef MOTHERSHIP_H
#define MOTHERSHIP_H

#include <stdbool.h>

void mothership_init(void);
void mothership_reset(void);
void mothership_reset_appear(void);
void mothership_start_departure(void);
void mothership_start_destruction(void);
bool mothership_is_descending(void);
bool mothership_is_landed(void);
bool mothership_consume_respawned_after_destruction(void);
bool mothership_consume_departed(void);
void mothership_update(void);
void mothership_set_sfx_enabled(bool enabled);

#endif // MOTHERSHIP_H
