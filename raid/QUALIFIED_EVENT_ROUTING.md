# Qualified Raid event routing

Status: IMPLEMENTED / UNBUILT
Date: 2026-09-04

Recovered from `9dbe2a00b16bb21e9d777e488c36f583ef99b2b3` plus numeric parsing correction `e795d0ce7f8202acd6698126658ddb98cd44a976`, reconciled onto `rectify/terminal-state-director`.

## Statically implemented routes

- `bind mouse2 +raid_interact` registers paired KEX aliases and forwards press/release commands to the game DLL.
- `required_action` is optional on `trigger_raid_interaction`. Blank preserves overlap activation; `interact` requires an explicit press while overlapping.
- Existing Director event arrays remain multi-listener collections: every matching JSON entry is evaluated in document order through the guarded dispatcher.
- `source: "*"` subscribes globally. Optional `source_tag`, `subject`, and `subject_tag` qualify matches.
- `monster_alerted` is published on the first native player-awareness edge in `FoundTarget`.
- `monster_arrived` is published once when a monster with an assigned `raid_arrival_target` enters its configured radius.
- `monster_death` is published from `G_MonsterKilled`.
- `player_death` is published from the ordinary `player_die` cleanup path through `RaidDowned_OnDeath`; the recovered safe hook does not currently attach killer/attacker as the event subject.

## Optional monster fields

`raid_alert_emit`, `raid_alert_tag`, `raid_arrival_target`, `raid_arrival_tag`, `raid_arrival_radius`, and `raid_arrival_emit` are optional. Blank fields preserve native AI. Arrival monitoring exists only when an objective is assigned.

## Mapper listener fields

Four listener slots are statically registered through `listen_1_*` to `listen_4_*`. The recovered dispatcher contains support for `event`/`action`, `source`, `subject`, `tag`, `radius`, `edge`, `count`, `window`, `cooldown`, `once`, `consume`, `dedupe`, and `emit`.

These controls are recovered source behavior, not runtime-proofed behavior. Do not promote them to PLAYTESTED until a build and runtime artifact exists.

## Dispatch safety

Events retain entity number plus spawn count for deferred activators. Recursively emitted events are queued until the active callback returns. A 1024-event budget prevents an unbounded recursive loop. JSON event entries do not implicitly consume one another; every matching entry is evaluated unless mapper-listener `consume` behavior terminates mapper-output routing.

## Current limits

Projectile-impact normalization, explosion context, BFG-hit context, and dynamic JSON arrival assignment remain deferred. They should use this event contract rather than an encounter-specific hard-coded path. Native `usercmd_t` has no M2 bit, so explicit interaction continues to use the cgame alias-to-`cmd` bridge.

No compile or runtime claim is made by this document.