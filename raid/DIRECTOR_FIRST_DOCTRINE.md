# Q2Raid Director-First Design Doctrine

Status: **canonical architecture contract**
Applies to: DLL code, encounter JSON, mapper entities, FGD definitions, test maps, and future coding batches.

## Purpose

Q2Raid deliberately departs from traditional Quake encounter construction.
Stock Quake entity wiring is a useful engine substrate, but it is not the
authoring model for raid logic. The Director exists specifically to prevent a
raid from becoming a hidden network of target strings, relay subclasses,
counters, teams, and entity-specific trigger variants.

The intended split is:

- **The DLL supplies reusable physical capabilities.**
- **The map supplies geometry and named physical objects.**
- **JSON owns encounter rules, sequencing, conditions, and coordination.**
- **The FGD exposes mapper concepts, not implementation accidents.**

If a change makes the mapper reconstruct program logic in JACK, the change is
moving in the wrong direction.

## The authority boundary

### DLL responsibilities

The DLL may:

- Implement physical facts that require engine access: carrying, collision,
  damage, movement, cameras, HUD rendering, monster deployment, observation,
  item overlap, and input capture.
- Expose a small reusable vocabulary of capabilities.
- Emit semantic events such as `pickup`, `charge_begin`, `charge_complete`,
  `deposit`, `terminal_open`, `terminal_complete`, `monster_killed`, `downed`,
  `revived`, and `bleedout`.
- Accept Director operations such as opening a terminal, enabling a monster
  door, changing an object's state, applying a status, or playing presentation.
- Validate mapper and JSON configuration loudly.
- Restore all state it temporarily owns.

The DLL must not silently become the encounter script. Phase progression,
conditional outcomes, timing sequences, target coordination, permutations,
and encounter-specific consequences belong to JSON.

### Map responsibilities

The map may:

- Contain geometry, ordinary world entities, physical interaction volumes,
  spawn locations, cameras, lights, items, sockets, beams, terminals, and
  monster roster templates.
- Give a physical object a `targetname` when JSON or another object genuinely
  needs to address that exact instance.
- Select intrinsic presentation or physical variants.

The map must not require chains of bespoke logic entities to describe encounter
rules. A mapper should not need to know which legacy `edict_t` field stores a
raid concept.

### JSON responsibilities

JSON owns:

- Encounter and phase state.
- Event-to-action relationships.
- Win, failure, wipe, and recovery conditions.
- Timers and escalation.
- Random selection and permutations.
- Activation and deactivation of named physical capabilities.
- Status consequences and encounter-specific presentation.
- Coordination between otherwise independent map objects.

## Primitive, event, operation

Every new feature must be expressible as this three-part contract:

1. **Primitive:** a reusable physical capability in the DLL.
2. **Event:** a semantic fact emitted when something happens.
3. **Operation:** a semantic command JSON can issue when required.

Examples:

| Primitive | Emits | Director may command |
| --- | --- | --- |
| Power core | `pickup`, `drop`, `charge_complete`, `deposit` | set/reset state, return to origin |
| Charging beam | `charge_begin`, `charge_cancelled`, `charge_complete` | enable/disable |
| Socket | `deposit`, `deposit_rejected` | enable/disable, eject/reset |
| Terminal | `terminal_open`, `terminal_complete`, `terminal_cancel` | open/close, choose presentation/puzzle |
| Monster door | `activated`, `deploy`, `replenish`, `deactivated` | enable/disable |
| Raid hat | `monster_killed`, observation-state events | set wrapper/observation state |

The event reports what happened. JSON decides what it means for this encounter.

## Defaults must produce the ordinary case

The common mapper path must require the fewest fields.

- A power core begins uncharged unless explicitly authored otherwise.
- A charging beam charges a carried power core.
- A core socket accepts a charged power core.
- A terminal is a terminal.
- A monster door owns deployment capability, not encounter phase logic.

Special identifiers and filters are allowed only for ambiguity or deliberate
override. They must never be mandatory ceremony for the ordinary case.

For example, socket-group strings are appropriate when several physically
overlapping socket groups need disambiguation. They are not required for one
core, one beam, and one socket.

## No dedicated trigger subtype for every outcome

A dedicated trigger is justified only when its physical detection semantics are
different. A new trigger classname is not justified merely because its event
causes a different encounter action.

Preferred flow:

1. A generic activation/interaction primitive detects the player.
2. It emits a semantic event.
3. JSON chooses the operation, target, and consequence.

`trigger_raid_terminal` directly opening one hardwired gadget is therefore a
proof implementation, not the target architecture. The target design is a
generic interaction event plus an `open_terminal` Director operation.

## State has one owner

Every temporary state must have one explicit owner and one symmetric exit path.

Examples:

- Third-person owns camera offset, owner-only avatar visibility, held model,
  weapon view-model suppression, and restoration.
- Terminal owns movement/input capture and its HUD presentation mode.
- Downed owns the replacement model, crawl restrictions, bleedout, and cleanup
  before ordinary death proceeds.
- Carry owns the carried item reference and releases it through one finish path.

Rules:

- Enter and leave operations must be paired.
- Toggle-off, completion, cancellation, damage, death, disconnect, map change,
  and wipe must converge on the same cleanup routine.
- A caller must not clear an ownership flag before the owner performs cleanup.
- Subsystems must not communicate by borrowing another subsystem's HUD marker.
- Cleanup must be idempotent: calling it twice must be safe.

## Presentation is data, not a hardcoded filename

Terminal layers, sounds, models, colours, timings, and puzzle presentations are
data. Code may provide a safe default, but encounter packs must be able to
select their resources through a manifest or JSON.

Rules:

- A manifest is authoritative when one exists.
- Code and manifests may not name different assets.
- Missing assets produce one clear console error naming the unresolved path.
- Renderer constraints are checked before promising presentation features.
- Physical behaviour remains testable with placeholder presentation.

## FGD is a generated public interface

There must be one canonical source for mapper-facing definitions.

- `fgd/q2raid.fgd` is the intended canonical complete FGD until a generator is
  introduced.
- Other FGD files and mapper packs must be generated copies or explicitly
  labelled obsolete.
- A build is not mapper-ready if the DLL accepts keys absent from the delivered
  FGD.
- New semantic keys require corresponding validation and documentation.
- Internal storage aliases such as `message`, `pathtarget`, `deathtarget`,
  `sounds`, and `style` must not leak into mapper instructions.

Longer term, raid configuration should be parsed into typed per-system runtime
structures instead of treating general-purpose `edict_t` fields as the domain
model.

## Validation must replace silent failure

Invalid configuration must be visible at map load or first use.

At minimum:

- An unresolved named target prints the source classname, source targetname,
  field, and missing value.
- An item rejected by a beam or socket reports the semantic reason in developer
  output: wrong type, wrong state, disabled, occupied, or ambiguous target.
- Duplicate objects that make automatic selection ambiguous produce a warning.
- JSON validation identifies the state/event/operation index.
- `raid_dump` reports active state, registered primitives, unresolved links,
  and current temporary player-state owners.

Doing nothing without explanation is a bug.

## Legacy source is reference, not doctrine

Nightdive, id, Rogue, Xatrix, CTF, and old Q2 mod code provide engine knowledge.
Their architectural patterns are not automatically appropriate for Q2Raid.

Before copying a legacy pattern, ask:

1. Is this physical engine capability or encounter logic?
2. Would JSON make the relationship explicit and easier to vary?
3. Does this introduce mapper wiring that the Director was created to remove?
4. Does it create a second owner for player, HUD, item, or encounter state?
5. Can the ordinary case work without a mapper-authored identifier?

If the pattern fails those questions, use it only as low-level implementation
reference and design a Director-facing semantic interface around it.

## Feature-completion language

The following claims are distinct and may not be substituted for one another:

- **Documented:** concept recorded.
- **Scaffolded:** interfaces or placeholders exist.
- **Implemented:** code path exists.
- **Compile-clean:** CI builds and links it.
- **Proof-tested:** isolated map demonstrates the happy path.
- **Integration-tested:** interacts correctly with other systems and resets.
- **Feature-complete:** current canonical acceptance criteria are satisfied.
- **Alpha-ready:** full intended slice is playable repeatedly without known
  blocking failures.

A completed commit batch or green compiler run never proves feature completion.

## Required gate for every future batch

Before coding:

- Identify the canonical requirement and whether old notes are obsolete.
- State primitive, event, operation, and state owner.
- Confirm what belongs in DLL, map, JSON, and presentation data.

Before calling it complete:

- Compile and link.
- Run the smallest proof map.
- Exercise enter, success, cancel, death, disconnect, wipe, and repeat paths as
  applicable.
- Confirm FGD and JSON examples match the DLL.
- Record known gaps without upgrading their status.

This doctrine exists to preserve the thing Q2Raid is trying to become: a raid
authoring layer that makes sophisticated encounters clearer and faster to build,
not a larger pile of traditional Quake wiring with raid-flavoured names.
