#ifndef LASER_H
#define LASER_H

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

#endif // LASER_H
