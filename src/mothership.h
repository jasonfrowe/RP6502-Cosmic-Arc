#ifndef MOTHERSHIP_H
#define MOTHERSHIP_H

#include <stdint.h>

#define MOTHERSHIP_PART_COUNT 4

typedef struct {
    int16_t x_pos_px;
    int16_t y_pos_px;
} mothership_position_t;

void init_mothership(unsigned config_addr);
void update_mothership(void);

#endif
