# Cosmic Arc RP6502 Roadmap

## Current Status

- [x] Core video setup and tile/sprite memory layout
- [x] Palette loading and mothership palette animation
- [x] Keyboard and gamepad input pipeline
- [x] Laser firing with cooldown
- [x] Asteroid spawning, movement, and collision
- [x] Score display and scoring for asteroid hits
- [x] Game-start mothership fly-in
- [x] Mothership destruction sequence and respawn loop
- [ ] Planet-surface transition
- [ ] Creature capture gameplay
- [ ] Full audio pass
- [ ] End-to-end gameplay polish and balancing

## Next Tasks

- [ ] Create OPL2 music
- [ ] Add OPL2 sound effects
  - [ ] Laser
  - [ ] Destruction
  - [ ] Asteroid
  - [ ] Mothership flying
- [ ] Add transition to planet surface

## Progress vs Final Goal

The project is past the prototype stage and already has the main arcade loop working on RP6502: fly-in, shooting, asteroid pressure, scoring, death/reset, and controller support are in place.

The biggest missing pieces between the current build and a recognizably complete Cosmic Arc are:

1. Audio identity
2. The planet-surface phase transition
3. Creature capture gameplay
4. Final pacing, visuals, and game feel polish

## Suggested Order

1. Finish OPL2 sound effects first so moment-to-moment gameplay gets feedback.
2. Add OPL2 music once timing and sound channels are understood.
3. Build the transition to planet surface.
4. Implement creature capture and the second gameplay phase.
5. Do balancing and visual/audio polish last.

## Scoring Notes
1. 10 points for each asteroid hit
2. 1000 points for capturing both creatures on the planet surface
3. 10 points for only capturing one creature on the planet surface
4. start with 40 energy.  lose 1 point for each laser shot, and 10 points for each asteroid hit.  Game over when energy reaches 0.
5. get 1 energy back for each asteroid.  full energy for capturing both creatures on the planet surface.