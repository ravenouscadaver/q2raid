# Q2Raid Day 1 Notes — Director Foundation

Date: 2026-08-24  
Status: reconstructed draft from repository history and retained decisions.

## Outcome

Day 1 established the server-authoritative JSON Director and proved that an
encounter document could control ordinary Quake II runtime objects. It also
established the Windows DLL build workflow and the first standalone JACK FGD.

## Implemented proofs

- JSON encounter loading, validation, state selection, event dispatch, and
  state-entry operations.
- Server commands for loading, reloading, dumping, and forcing Director state.
- Entity-to-Director event bridge.
- Director-controlled runtime lights and door-hold proof.
- Player status application/removal and encounter HUD messages.
- Encounter reset validation and status cleanup.
- Windows GitHub Actions DLL build path with JsonCpp/vcpkg integration.
- Initial JACK-compatible `fgd/q2raid.fgd` interface.

## Canon established

- The host/server alone executes encounter JSON.
- The DLL supplies reusable physical capabilities.
- Maps supply geometry and named physical objects.
- JSON owns phase logic, timing, coordination, failure, and outcomes.
- Clients receive replicated gameplay results; they do not run duplicate
  encounter scripts.

## Known limitations at day end

- Compiler success proved only that the scaffold linked.
- Reset was not yet safe enough to describe as alpha-ready.
- Mapper definitions and runtime semantics were still changing rapidly.
- `raid_reload` did not reconstruct BSP entities or all runtime state; disabling
  triggers was cleanup-only.

## Historical boundary

Traditional relay/counter/target-string encounter designs predating the
Director became reference material, not automatic requirements. Any retained
idea had to be restated as a primitive, event, and Director operation.
