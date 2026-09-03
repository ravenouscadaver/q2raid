# Qualified Raid event routing

Status: IMPLEMENTED / STATICALLY AUDITED / UNBUILT
Date: 2026-09-04

Current review target: `work-unit-3-qualified-event-bridge`.

## Implemented

- Generic Director event dispatch uses the existing queued dispatcher.
- `source: "*"` provides wildcard/global subscription.
- Optional `source_tag`, `subject`, and `subject_tag` filters qualify JSON event matches.
- `monster_alerted` is published on the first native player-awareness edge in `FoundTarget`; the hook does not alter native AI behaviour.
- `monster_arrived` is monitored only for entities with an assigned `raid_arrival_target`, uses the configured radius (default 64), and emits once until encounter reset.
- `monster_death` is published from `G_MonsterKilled` before the existing score/debug accounting; no death/drop/gib behaviour is changed by the hook.
- `player_death` is published from the ordinary `player_die` cleanup path through `RaidDowned_OnDeath`.
- `bind mouse2 +raid_interact` uses paired KEX aliases (`+raid_interact` / `-raid_interact`) routed to the game DLL.
- `required_action` is optional on `trigger_raid_interaction`; blank preserves overlap activation and `interact` requires an explicit press while overlapping.
- Terminal interaction delegates to the current `RaidUI` implementation; Work Unit 1 remains the terminal state/input owner.
- Recursive events are queued while dispatch is active. The dispatcher retains its 1024-event budget.
- Multiple matching JSON event entries are evaluated in document order.
- Deferred activators use entity number plus spawn count internally for lifetime-safe resolution.

## Mapper/entity registration

Optional monster fields:

`raid_alert_emit`, `raid_alert_tag`, `raid_arrival_target`, `raid_arrival_tag`, `raid_arrival_radius`, `raid_arrival_emit`.

Four mapper listener slots are registered through `listen_1_*` to `listen_4_*`. The recovered source supports `event`/`action`, `source`, `subject`, `tag`, `radius`, `edge`, `count`, `window`, `cooldown`, `once`, `consume`, `dedupe`, and `emit`.

These controls are source-level implementation only until compile/runtime evidence exists.

## Protected reset/presentation note

The Work Unit 3 branch does not change Raid Hat HUD name/health/shield drawing. Encounter reset retains the protected shield-reset behaviour by restoring monsters with configured power armour to `max_power_armor_power` and their `initial_power_armor_type` during `RaidHats_Reset`.

## Not implemented / deferred

- `player_death` attacker/kill-source context is **not currently preserved**: the present `RaidDowned_OnDeath` hook emits `player_death` with a null event subject. Moving the emit directly into authoritative `player_die`, where `attacker` is already available, is the minimal source fix, but that protected Work Unit 1 file was not rewritten during this static-only pass.
- Projectile-impact normalization is not implemented.
- Explosion context normalization is not implemented.
- BFG-hit context normalization is not implemented.
- Dynamic JSON arrival assignment is not implemented.
- No additional projectile type, explosion, damage, or death-context fields should be documented as available until they exist in the dispatcher contract.

## Dispatch safety

Events retain entity number plus spawn count for deferred activators. Recursively emitted events remain queued until the active callback returns. A 1024-event budget prevents an unbounded recursive loop. JSON event entries do not implicitly consume one another; mapper-listener `consume` applies only to mapper-output routing.

No compile or runtime claim is made by this document.
