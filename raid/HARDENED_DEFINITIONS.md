# Q2Raid Hardened Definitions

Status: canonical vocabulary and decision rules.  
Relationship: concise operational companion to `DIRECTOR_FIRST_DOCTRINE.md`.

## Authority

| Layer | Owns | Must not own |
| --- | --- | --- |
| DLL | Physical engine capabilities, validation, cleanup, semantic events and operations | Encounter-specific sequencing or outcomes |
| Map | Geometry, placed physical primitives, intrinsic variants, optional names | Relay spaghetti or phase programs |
| JSON Director | State, conditions, timers, permutations, coordination, win/fail/wipe consequences | Low-level physics or renderer implementation |
| FGD | Accurate mapper-facing vocabulary and defaults | Legacy storage aliases or imaginary keys |
| Presentation data | Models, textures, sounds, colours, timing, terminal skins | Encounter authority |

## Mandatory feature shape

Every new mechanic is defined as:

1. **Primitive** — reusable physical capability.
2. **Event** — semantic fact emitted by that primitive.
3. **Operation** — semantic command the Director may issue.
4. **Owner** — subsystem responsible for all temporary state and cleanup.

If a proposal cannot identify those four things, it is not ready for code.

## Canonical primitives

| Primitive | Intrinsic ordinary behaviour | Events | Director operations |
| --- | --- | --- | --- |
| Power core | Begins uncharged; can be carried; charged state is intrinsic | `pickup`, `drop`, `charge_complete`, `deposit` | set/reset state, return/eject |
| Charging beam | Charges a carried power core | `charge_begin`, `charge_cancelled`, `charge_complete` | enable, disable |
| Core socket | Accepts a charged core if empty | `deposit`, `deposit_rejected` | enable, disable, eject/reset |
| Terminal/gadget | Presents an interaction owned by the terminal system | `terminal_open`, `terminal_complete`, `terminal_cancel` | open, close, choose puzzle/presentation |
| Monster door | Deploys its physical roster | `activated`, `deploy`, `replenish`, `deactivated` | enable, disable |
| Raid Hat | Wraps a stock monster with raid presentation/behaviour | `monster_killed`, observation-state events | set wrapper/observation state |
| Downed state | Temporary cooperative pre-death state | `downed`, `revived`, `bleedout` | enable/disable policy, revive or finish where allowed |

## Core theology

- The core says: **I am a power core; I begin uncharged.**
- The beam says: **I am a charging beam; I charge power cores.**
- The socket says: **I am a socket; I accept charged power cores.**
- The Director receives `charge_complete` and `deposit` and controls progression.
- Additional strings exist only to disambiguate unusual layouts.
- Only charged cores are Volatile by default.
- A pre-slotted core must register occupancy and state, not merely overlap the
  socket visually.

## Interaction theology

- Different consequences do not justify different trigger subclasses.
- A generic physical interaction emits an event.
- JSON decides which terminal, door, state, reward, or failure follows.
- `trigger_raid_terminal` is deprecated proof wiring; the canonical flow is
  generic interaction -> event -> Director `open_terminal`.

## State ownership

| Owner | Must restore |
| --- | --- |
| Third person | Camera, avatar visibility/model, held model, weapon view model |
| Terminal | Input/movement capture and terminal HUD/presentation mode |
| Downed | Replacement model, crawl restrictions, audio, timers, death handoff |
| Carry | Carried item reference, world visibility, status, weapon suppression |

All success, cancel, damage, death, disconnect, wipe, map-change, and toggle-off
paths converge on the owner's idempotent cleanup routine. A caller never clears
the ownership flag before cleanup runs.

## Presentation definitions

- Raid Hat name colour communicates rank: white, red, blue, or purple.
- Shield is a separate cyan text batch of 15 periods drawn over the health bar.
- Terminal presentation uses validated data and a safe fallback; missing art may
  not crash or leave movement locked.
- Presentation filenames are selected by a manifest/JSON or one documented safe
  default, never conflicting hardcoded paths.

## Validation definitions

Doing nothing silently is a defect. Diagnostics name the source object, field,
value, and semantic rejection reason. Expected reasons include wrong type,
wrong state, disabled, occupied, ambiguous, unresolved target, missing asset,
and invalid JSON state/event/operation index.

`raid_dump` should report active Director state, registered primitives,
unresolved links, occupied sockets, carried objects, and current player-state
owners.

## Canonical mapper interface

- `fgd/q2raid.fgd` is the single source of truth until generation replaces it.
- Merged or packaged FGDs are generated outputs, never independently edited
  authorities.
- Semantic keys must match the DLL and documentation in the same batch.
- Legacy storage fields such as `message`, `pathtarget`, `deathtarget`,
  `combattarget`, `healthtarget`, `sounds`, and `style` are implementation
  details and must not appear as the conceptual API.

## Maturity vocabulary

| Term | Exact meaning |
| --- | --- |
| Documented | Requirement or concept recorded |
| Scaffolded | Interface or placeholder exists |
| Implemented | A code path exists |
| Compile-clean | CI/compiler builds and links it |
| Proof-tested | Small map demonstrates its happy path |
| Integration-tested | Interacts and resets correctly across relevant exits |
| Feature-complete | All current canonical acceptance criteria pass |
| Alpha-ready | Full intended slice repeats without known blocking failures |

These terms are never interchangeable. “The whole list is done” requires a
canonical ledger showing every retained item and its evidence.

## Build authorization

Only the exact phrase **`go go gadget`** authorizes compilation or pushing.
Everything else queues local work. Before acting, state the commit range and
known blockers; afterwards, report the actual build/runtime status without
upgrading it.

## Last-DLL rule

After the finite structural batch in `DAY4NOTES.md`, a proposed DLL change must
show that the capability cannot be represented through existing primitives,
events, operations, JSON, map placement, or presentation data. Encounter rules
default to JSON. Legacy engine-family code is evidence about mechanics, not
authority over Q2Raid architecture.
