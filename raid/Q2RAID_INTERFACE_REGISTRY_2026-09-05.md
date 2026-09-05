# Q2Raid Interface / String Registry

**Seeded:** 2026-09-05 provenance audit  
**Canonical branch:** `director-scaffold`  
**Purpose:** living drift-control and cross-reference registry.

New classnames, mapper keys, JSON keys, signals, operations, cvars, commands, stat/configstring slots, persistent logical IDs and asset paths must be checked here and recorded in the same work unit that changes them.

## Mapper-facing classnames

| Exact classname | Owner | Meaning | Status |
|---|---|---|---|
| `raid_item` | `raid_items.cpp::SP_raid_item` | Carryable raid item | CANONICAL; core path protected |
| `raid_gadget` | `raid_items.cpp::SP_raid_gadget` | Reusable raid anchor: socket/reconstruction/terminal | CANONICAL |
| `trigger_raid_deposit` | `raid_items.cpp` | Deposit carried raid item | CANONICAL; core path protected |
| `trigger_raid_item` | `raid_items.cpp` | Carried-item interaction/charging field | CANONICAL; core path protected |
| `raid_hovertext` | `raid_items.cpp` | Crosshair hover message/event marker | CANONICAL |
| `raid_bot_goal` | `raid_bots.cpp` | Bot navigation/operation marker | CANONICAL / EXPERIMENTAL |
| `trigger_raid_interaction` | `raid_terminal.cpp` | Generic Director-first interaction volume | **CANONICAL interaction primitive** |
| `trigger_raid_terminal` | `raid_terminal.cpp` | Legacy direct-open terminal wrapper | **DEPRECATED COMPATIBILITY** |
| `raid_monster_door` | `raid_monsters.cpp` | Roster/deployment controller | CANONICAL |
| `raid_hat` | `raid_hats.cpp` | Monster metadata/presentation/behavior wrapper | CANONICAL; presentation protected |

Runtime-only internal classnames:

- `raid_hat_attachment`
- `raid_leash_goal`

Do not add those to the FGD.

## Semantic mapper keys -> backing `edict_t` member

The mapper key is the public interface. The backing member is an implementation detail, not a second public name.

| Public key | Backing member | Intended family |
|---|---|---|
| `text` | `message` | `raid_hovertext` |
| `item_type` | `message` | `raid_item` |
| `gadget_type` | `message` | `raid_gadget` |
| `carry_mode` | `style` | `raid_item` |
| `weapon` | `pathtarget` | `raid_item` relic |
| `encumber_scale` | `speed` | `raid_item` |
| `carry_status` | `deathtarget` | `raid_item` |
| `held_model` | `combattarget` | `raid_item` |
| `status_duration` | `delay` | `raid_item` |
| `status_policy` | `healthtarget` | `raid_item` |
| `clear_status_on_drop` | `sounds` | `raid_item` |
| `hover_radius` | `dmg_radius` | `raid_hovertext` |
| `hover_distance` | `speed` | `raid_hovertext` |
| `require_los` | `sounds` | `raid_hovertext` |
| `deploy_interval` | `wait` | `raid_monster_door` |
| `initial_count` | `count` | `raid_monster_door` |
| `min_active` | `radius_dmg` | `raid_monster_door` |
| `max_active` | `health` | `raid_monster_door` |
| `wave_size` | `dmg` | `raid_monster_door` |
| `replenish_delay` | `delay` | `raid_monster_door` |
| `replenish_mode` | `style` | `raid_monster_door` |
| `leash_radius` | `dmg_radius` | `raid_monster_door` |
| `leash_return_radius` | `accel` | `raid_monster_door` |
| `leash_grace` | `speed` | `raid_monster_door` |
| `wake_mode` | `mass` | `raid_monster_door` |
| `raid_health_multiplier` | `speed` | `raid_hat` |
| `raid_monster_scale` | `accel` | `raid_hat` |
| `raid_ai_mode` | `mass` | `raid_hat` |
| `display_distance` | `dmg_radius` | `raid_hat` |
| `start_pose` | `dmg` | `raid_hat` |
| `freeze_delay` | `delay` | `raid_hat` observer mode |
| `release_delay` | `wait` | `raid_hat` observer mode |
| `minimum_move_time` | `decel` | `raid_hat` observer mode |
| `observer_inverted` | `sounds` | `raid_hat` observer mode |
| `twitch_chance` | `random` | `raid_hat` observer mode |
| `raid_hat` | `map` | editor-placed `monster_*` template |
| `rank` | `style` | `raid_hat` |
| `attachment_offset` | `move_origin` | `raid_hat` |
| `attachment_angles` | `move_angles` | `raid_hat` |
| `accepts` | `target` | gadget/item interaction |
| `required_state` | `pathtarget` | gadget/deposit/item interaction |
| `set_state` | `combattarget` | `trigger_raid_item` item-state result |
| `charge_time` | `delay` | `trigger_raid_item` |
| `charged_vfx` | `itemtarget` | raid item/charger presentation |
| `reconstruct_spawn` | `deathtarget` | reconstruction gadget |
| `spectator_camera` | `combattarget` | reconstruction gadget |
| `order` | `count` | `raid_gadget` |

Backing-field collision hotspots include `message`, `pathtarget`, `combattarget`, `speed`, `sounds`, `dmg_radius`, `delay`, `style`, `count`, `mass`, `accel` and `dmg`.

**Rule:** do not replace semantic keys with raw backing-member names. Prevent synonyms and cross-family misuse instead.

## Current Director JSON document contract

Root keys:

- `encounter`
- `initial_state` — required
- `states` — required
- `reset`
- `statuses`
- `events`

State keys:

- `enter`
- `exit`

Event keys:

- `source` — required exact source `targetname`
- `signal` — exact semantic signal; runtime default `activate`
- `from_state` — optional state filter
- `do` — operation array
- `set_state` — optional transition

Current event matching is:

`exact source targetname + exact signal + optional from_state`

`RaidDirector_NotifyEntityEvent()` requires a source with `targetname`. The dispatch recursion budget is 1024 events.

## Current Director operations

Accepted operation names:

- `fire_target`
- `disable_entity`
- `set_field`
- `post_message`
- `post_encounter_message`
- `apply_status`
- `clear_status`
- `damage_player`
- `kill_player`
- `screen_shake`
- `screen_flash`
- `play_sound`
- `color_cycle`
- `set_monster_door`
- `bot_move_to`
- `bot_follow_activator`
- `bot_operate_gadget`
- `open_terminal`

Current `set_field` whitelist:

- `wait`
- `hover_distance` on `raid_hovertext`
- `hover_radius` on `raid_hovertext`
- `require_los` on `raid_hovertext`
- `observer_inverted` on `raid_hat`

Do not invent JSON spellings for accepted-design capabilities that are not yet current operations, including generic named counters/flags/timers, checkpoint operations, persistent-layer writes, `world_ready` registration operations, or generic Carnage-row writes.

## Current semantic event publishers

Generic engine trigger:

- `activate`

Raid item/carry:

- `pickup`
- `drop`
- `charge_begin`
- `charge_cancelled`
- `charge_complete`

`trigger_raid_item`:

- `charge_begin`
- `charge_complete`

Socket/reconstruction gadget:

- `deposit`
- `player_assigned`
- `reconstructed`

Hover:

- `hover_enter`
- `hover`
- `hover_exit`

Generic interaction / terminal:

- `interact`
- `terminal_open`
- `terminal_complete`

Current terminal cancellation intentionally does not invent a new semantic event in the post-audit rectification. Unsolved/manual close uses an internal per-player 500 ms re-entry lockout only.

Monster door:

- `activated`
- `deactivated`
- `deploy`
- `replenish`
- `leash_return`

Door-local `deploy` is not the generic `monster_arrived` contract.

Raid Hat:

- `applied`
- `activated`
- `watched`
- `unwatched`
- `movement_released`
- `movement_frozen`
- `twitch`
- `observation_inverted`
- `observation_normal`
- `monster_killed`

`monster_killed` here is hat-scoped, not a generic every-monster death signal.

Helper bots:

- `bot_assigned`
- `bot_goal_reached`
- `bot_operate_begin`
- `bot_operate_complete`
- `bot_failed`

## Commands

Server commands:

- `raid_load`
- `raid_reload`
- `raid_reset`
- `raid_dump`
- `raid_monster_dump`
- `raid_set_state`
- `raid_bot_add`
- `raid_bot_remove_all`
- `raid_test_flash`
- `raid_test_dark`

Client/dev commands:

- `raid_thirdperson`
- `raid_downed_test`
- `raid_grenade_press`
- `raid_grenade_release`

KEX protected aliases:

- `+raid_grenade` -> `cmd raid_grenade_press`
- `-raid_grenade` -> `cmd raid_grenade_release`

## Intentional optional movement cvars

- `raid_double_jump`
- `raid_double_jump_velocity`
- `raid_coyote_time`
- `raid_coyote_window`
- `raid_long_jump`
- `raid_long_jump_min_speed`
- `raid_long_jump_speed`
- `raid_long_jump_vertical`

These are intentional/default-off and are not cleanup candidates.

## Downed cvars

- `raid_downed_damage_buffer`
- `raid_downed_bleedout`
- `raid_downed_crawl_scale`

Latest corpse correction remains `COMPILED / NOT RUNTIME TESTED` until user runtime proof.

## Director cvars

- `raid_script`
- `raid_script_root`
- `raid_autoload`
- native `game` cvar used for encounter-path resolution

## Off-canonical WU3 vocabulary

Intentional historical recovery candidates, not current canonical promises:

Filters/identity:

- wildcard `source: "*"`
- `source_tag`
- `subject`
- `subject_tag`

Monster metadata:

- `raid_alert_emit`
- `raid_alert_tag`
- `raid_arrival_target`
- `raid_arrival_tag`
- `raid_arrival_radius`
- `raid_arrival_emit`

Mapper listener fields:

- `listen_1_*`
- `listen_2_*`
- `listen_3_*`
- `listen_4_*`
- `required_action`

Historical generic signals:

- `monster_alerted`
- `monster_arrived`
- `monster_death`
- `player_death`

Classification: `RECOVERABLE OFF-CANONICAL`. Do not author production JSON against these names until that bridge is deliberately recovered.

## Deprecated / superseded names

| Name | Classification |
|---|---|
| `trigger_raid_terminal` | Current deprecated compatibility wrapper; migration proof required before removal |
| `raid_player_pose.*` | Historical/superseded; not in current build |
| `raid_terminal_shared.h` | Historical/superseded; not in current build |
| `integration` as canonical branch | Superseded workflow authority |
| `pics/raid/...` | Invented/rejected asset location |
| `terminal_controls_puzzle_v2.png` as active layer | Rejected; historical original only |
| `terminal_controls.png` as approved final controls | Superseded for the next graphics work unit by `terminal_controls_clean.png` |

## Current ownership conflicts / follow-ups

### Carnage

Post-audit rectification removes state-transition-derived `mechanic_progress` and the fixed `MECHANIC PROGRESS` renderer row.

Current report remains a wipe interlude with hostiles eliminated, marine deaths and wipe count while the generic encounter-authored row contract is intentionally left unnamed/unimplemented.

Future ownership:

`engine/encounter facts -> encounter JSON accounting -> generic Carnage rows -> wipe interlude display`

Director wipe/reset and Carnage all-dead presentation observation remain duplicate truth to reconcile later.

### Terminal

Manual/unsolved terminal exit now receives an internal per-player 500 ms re-entry lockout. No mapper key, JSON key, signal or classname was added.

`raid/terminals/entrance_terminal.json` still duplicates onboarding content currently hard-coded in `raid_ui_shared.h`, `raid_ui.cpp` and `cg_raid_ui.cpp`. Resolve that JSON/runtime boundary in a future bounded terminal-content work unit.

## Admission rule

Before adding any new interface string:

1. Search this registry.
2. Search `fgd/q2raid.fgd`.
3. Search `rerelease/g_spawn.cpp`.
4. Search the owning `raid_*.cpp/.h`.
5. Search current encounter/terminal JSON.
6. Search canonical doctrine/style guides.
7. Search historical recovery lineage if the concept sounds familiar.
8. Reuse an existing name when meaning matches.
9. If genuinely new, add exactly one name and document its owner in the same work unit.
10. Never add a second spelling merely to make one implementation convenient.
