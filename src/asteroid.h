#ifndef ASTEROID_H
#define ASTEROID_H

typedef enum {
    ASTEROID_NONE = 0,
    ASTEROID_LASER_HIT,
    ASTEROID_MOTHERSHIP_HIT,
} AsteroidResult;

void asteroid_init(void);
void asteroid_reset(void);
bool asteroid_is_active(void);
AsteroidResult asteroid_update(void);

void asteroid_set_planet_phase(bool active);
void asteroid_set_spawns_paused(bool paused);
void asteroid_force_spawn(void);

#endif // ASTEROID_H
