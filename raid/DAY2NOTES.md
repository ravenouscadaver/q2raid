# Q2Raid Day 2 Notes — Physical Systems

Date: 2026-08-25  
Status: reconstructed draft from repository history and retained decisions.

## Outcome

Day 2 expanded the Director scaffold into physical raid-system proofs: carry,
cores, sockets, deployment, wipe handling, Raid Hats, and the first downed
state. These were working proofs, not finished public interfaces.

## Implemented proofs

- Isolated third-person presentation and camera tuning.
- Carryable power cores with mapper-selected carried models.
- Ordered sockets and deposit lifecycle.
- Weapon relic carry mode.
- Monster deployment, roster ownership, replenishment, and collision waits.
- Encounter reset and wipe lifecycle fixes.
- Reusable Raid Hat wrappers and per-monster selectors.
- Prototype cooperative downed/crawl state.
- Recorded Shield Drone and held-open circuit encounter concepts.

## Significant stabilisation

- Monster rosters were rebuilt without retaining dead runtime members.
- Encounter pickups and combat debris were restored/cleared more consistently.
- Deployment collisions stopped forcing invalid placements.
- The attack that caused a down could no longer immediately finish the player
  through the same damage event.

## Architecture warning

Several systems were implemented quickly using legacy `edict_t` fields and
direct target relationships. That was acceptable for isolated proof work, but
it created the later interface drift: mapper-facing concepts were not yet clean
semantic objects and multiple FGDs began to disagree.

## Maturity at day end

- Director: implemented scaffold.
- Carry/core/socket: implemented proof.
- Monster doors/rosters: implemented proof requiring reset stress tests.
- Raid Hats: implemented proof.
- Downed: early prototype.
- None of the above was feature-complete merely because it compiled.
