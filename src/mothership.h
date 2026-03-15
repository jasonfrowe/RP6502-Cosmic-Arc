#ifndef MOTHERSHIP_H
#define MOTHERSHIP_H

#include <stdbool.h>

void mothership_init(void);
void mothership_reset(void);
bool mothership_is_landed(void);
void mothership_update(void);

#endif // MOTHERSHIP_H
