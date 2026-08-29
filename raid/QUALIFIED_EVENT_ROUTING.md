# Qualified Raid event routing

## Implemented routes

- `bind mouse2 +raid_interact` registers paired KEX aliases and forwards press/release commands to the game DLL.
- `required_action` is optional on `trigger_raid_interaction`. Blank preserves overlap activation; `interact` requires the explicit press while overlapping.
- Existing Director event arrays are multi-listener collections: every matching JSON entry is evaluated in document order through the guarded dispatcher.
- `source: "*"` subscribes globally. Optional `source_tag`, `subject`, and `subject_tag` filters further qualify a match.
- `monster_alerted` is published on the first native player-awareness edge in `FoundTarget`.
- `monster_arrived` is published once when a monster with an assigned `raid_arrival_target` enters its configured radius.
- `monster_death` and `player_death` are published from their authoritative ordinary death paths.

## Optional monster fields

`raid_alert_emit`, `raid_alert_tag`, `raid_arrival_target`, `raid_arrival_tag`, `raid_arrival_radius`, and `raid_arrival_emit` are optional. Blank fields preserve native AI. JSON can subscribe globally to alert/death events without mapper fields. Arrival monitoring exists only when an objective is assigned.

## Dispatch safety

Events use entity number plus spawn count for deferred activators. Recursively emitted events are queued and processed only after the active callback returns. The existing 1024-event budget stops recursive loops and discards the remainder with one server diagnostic. Multiple matching JSON entries are preserved; no implicit deduplication or consumption occurs.

## Current limits

Numbered mapper listener groups, projectile-impact normalization, count/window/cooldown/consume/dedupe controls, and dynamic JSON arrival assignment are not implemented by this slice. They must not be documented as supported. Native `usercmd_t` has no M2 bit; explicit interaction therefore uses the proven cgame alias-to-`cmd` bridge rather than inventing a button callback.
