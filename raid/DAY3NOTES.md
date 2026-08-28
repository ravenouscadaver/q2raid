# Q2Raid Day 3 Notes — Phase 8 Feature Proofs

Date: 2026-08-26  
Status: reconstructed draft from repository history and retained decisions.

## Outcome

Day 3 produced a broad Phase 8 proof batch: observer-locked Strogg, movement
permutations, shield/name presentation, charging, reconstruction, and Director
screen effects. It also revealed that breadth was outpacing integration tests.

## Implemented proofs

- Targeted Raid Hat names and health presentation.
- Observer-locked monsters: watched freezes, unobserved moves.
- JSON/targetable inversion concept recorded for later implementation.
- Optional double jump, coyote timing, and crouch long-jump/Strogg actuator.
- Raid shield capacity exposed through the wrapper.
- Physical core charging beam and charged presentation.
- Reconstruction chambers and helper objectives.
- Director-controlled screen flash overlays.
- Test maps for observer lock and other isolated systems.

## Accepted presentation

- Raid Hat target/name/health presentation was accepted as a useful proof.
- Name colour identifies rank and must remain independent from shield colour.
- Segmented or text-based shield treatment remained a separate presentation
  layer, not part of the name tag.

## Retained Weeping Strogg direction

- Base rule: watched monsters freeze; unobserved monsters move.
- Evaluate observation across all living players.
- Permit a Director-controlled inverse permutation.
- Later accepted additions: a short turn-only warning window after freezing,
  no frozen falling/landing attack activation, retaliation when attacked, and
  pressure against indefinitely staring into its eyes.
- Eye brightening is conditional on a cheap, reliable renderer/model hook; it
  is not promised until verified.

## Maturity at day end

The batch was compile-oriented and proof-heavy. It did not establish that every
feature survived death, disconnect, wipe, map transition, repeated activation,
or interaction with the other new systems. That missing integration discipline
became Day 4's central problem.
