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
- [x] Creature capture gameplay
- [x] Full audio pass (mix/tuning)
- [x] End-to-end gameplay polish and balancing

## Next Tasks

- [x] Creature capture rules and scoring integration
- [x] Surface-phase enemy/interaction behavior polish
- [x] Audio mix pass (music + SFX balance, loop smoothness)
- [ ] Atmosphere glow palette tweak (deferred pending design)

## Scoring Notes
1. 10 points for each asteroid hit
2. 1000 points for capturing both creatures on the planet surface
3. 10 points for only capturing one creature on the planet surface
4. start with 48 shield. lose 1 point for each laser shot, and 12 points for each asteroid hit. Game over when shield reaches 0.
5. get 1 shield back for each asteroid. full shield for capturing both creatures on the planet surface.