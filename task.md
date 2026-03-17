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
- [x] Planet-surface transition (timed on mothership respawn)
- [x] Demo mode loop (music on, no player fire, title hide on start)
- [x] Shield/energy bar and shield drain/recovery rules
- [x] Beasties sprites on surface phase (2 active)
- [ ] Creature capture gameplay
- [ ] Full audio pass (mix/tuning)
- [ ] End-to-end gameplay polish and balancing

## Next Tasks

- [ ] Creature capture rules and scoring integration
- [ ] Surface-phase enemy/interaction behavior polish
- [ ] Audio mix pass (music + SFX balance, loop smoothness)
- [ ] Atmosphere glow palette tweak (deferred pending design)

## Progress vs Final Goal

The project is past the prototype stage and already has the main arcade loop working on RP6502: fly-in, shooting, asteroid pressure, scoring, death/reset, and controller support are in place.

The biggest missing pieces between the current build and a recognizably complete Cosmic Arc are:

1. Audio identity
2. The planet-surface phase transition
3. Creature capture gameplay
4. Final pacing, visuals, and game feel polish

## Suggested Order

1. Implement creature capture gameplay and scoring outcomes.
2. Tune surface-phase behavior (beastie motion and interaction readability).
3. Do audio mix/loop polish across demo and gameplay tracks.
4. Revisit atmosphere glow palette treatment.
5. Finish balancing and overall polish.

## Scoring Notes
1. 10 points for each asteroid hit
2. 1000 points for capturing both creatures on the planet surface
3. 10 points for only capturing one creature on the planet surface
4. start with 48 shield. lose 1 point for each laser shot, and 12 points for each asteroid hit. Game over when shield reaches 0.
5. get 1 shield back for each asteroid. full shield for capturing both creatures on the planet surface.