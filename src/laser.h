#ifndef LASER_H
#define LASER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LASER_NONE = 0,
    LASER_UP,
    LASER_DOWN,
    LASER_LEFT,
    LASER_RIGHT,
} LaserDirection;

void laser_init(void);
void laser_fire(LaserDirection dir);
void laser_update(void);
bool laser_check_hit(int16_t x, int16_t y, uint8_t w, uint8_t h);

#endif // LASER_H
