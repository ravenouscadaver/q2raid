# Q2Raid Implementation Style Guide

**Status:** Canonical implementation convention
**Scope:** Game DLL, mapper entities, encounter JSON, test maps, and generated mapper distributions

This guide prevents the runtime, map, and encounter layers from acquiring competing meanings for the same mechanic. `HARDENED_DEFINITIONS.md` remains the authority on game design; this file governs how that design crosses implementation boundaries.

## 1. Authority order

When two layers could make the same decision, use this order:

1. The game DLL provides reusable primitives and validates their local invariants.
2. Encounter JSON owns encounter meaning, sequence, phase, consequences, and cross-system coordination.
3. The map owns geometry, placement, targetnames, and explicit mapper overrides.
4. Clients present replicated results and never advance encounter state independently.

A map override may select an intrinsic starting condition such as `START_CHARGED`. It must not silently become a second encounter director.

## 2. Primitive, event, operation

Keep these terms distinct:

| Term | Meaning | Example |
| --- | --- | --- |
| Primitive | Reusable local capability implemented by the DLL | terminal input capture, chargeable core |
| Event | A semantic fact emitted to the Director | `interact`, `charge_complete`, `deposit` |
| Operation | A validated Director command | `open_terminal`, `apply_status`, `set_monster_door` |
| Targetname | Stable map address consumed by JSON or another primitive | `entrance_terminal` |
| Override | Explicit map-authored exception to a primitive default | `START_CHARGED` |

Do not call events “triggers,” operations “events,” or mapper targetnames “states.”

## 3. Director-first dependency rule

Cross-system behaviour follows this dependency direction:

```text
map interaction -> semantic event -> encounter JSON -> validated operation -> runtime primitive
```

Runtime primitives must not hardwire encounter consequences. For example, an interaction volume emits `interact`; JSON decides whether that opens a terminal, unlocks a door, starts an alarm, or does nothing.

Compatibility wrappers may bypass this route only behind an explicit legacy flag. They must be labelled deprecated in the canonical FGD and excluded from new examples.

## 4. Strings and identifiers

Some mapper keys are stored in generic Quake II edict fields for SDK compatibility. That storage detail is not permission to proliferate synonyms.

- Use the exact identifiers documented in `HARDENED_DEFINITIONS.md` and `ENCOUNTER_JSON_STYLE_GUIDE.md`.
- JSON should select named targets and consequences wherever possible.
- Map string fields configure local primitive variants or deliberate overrides.
- C++ defaults cover intrinsic primitive behaviour only.
- Do not add a second spelling for an existing item type, gadget type, state, event, or operation.
- Unknown strings must produce a useful validation or runtime diagnostic; never silently fall through to a different meaning.

## 5. Defaults and overrides

Defaults must describe the ordinary fiction and mechanic:

- A power core begins uncharged.
- A charging field changes it to charged.
- Only a charged power core receives Volatile by default.
- A socket accepts the state its primitive contract declares.
- A terminal does not decide what its completion changes in the encounter.

Exceptional initial conditions use positive, explicit names such as `START_CHARGED`. Avoid inverted flags such as `DOES_NOT_REQUIRE_CHARGE`.

## 6. Entity policy

- `trigger_raid_interaction` is the canonical generic interaction volume.
- `trigger_raid_terminal` is a deprecated compatibility alias.
- `raid_gadget` may currently host several primitive roles, but each role must have a hardened identifier and narrow behaviour.
- `fgd/q2raid.fgd` is the only hand-maintained mapper definition.
- Other FGDs are upstream inputs, generated distributions, or archived snapshots. Never repair a Q2Raid entity in more than one FGD by hand.

## 7. Lifecycle pairing

Every operation that acquires state must have a safe inverse:

| Acquire | Required inverse |
| --- | --- |
| Open terminal | close/cancel, restore player, clear HUD ownership |
| Pick up item | deposit/drop/return, restore weapon and presentation |
| Apply status | expire/clear, including wipe and disconnect |
| Spawn or activate encounter entity | reset/free/restore baseline |
| Start timer or presentation | cancel and clear queued callbacks |

Cleanup must be idempotent and safe during death, disconnect, wipe, map transition, and partial activation.

## 8. Terminology review

Before adding a new identifier:

1. Search C++, JSON, FGD, maps, and Markdown for an existing term.
2. Decide whether the concept is a primitive, event, operation, state, or targetname.
3. Reuse the canonical term if the meaning matches.
4. If the meaning differs, document the distinction in hardened definitions before implementing it.
5. Add validation and one narrow proof asset.

## 9. Commit and test discipline

- Integration commits reconcile history without mixing behavioural cleanup.
- Behavioural changes and documentation must agree in the same reviewed batch.
- Compile-clean is evidence only of compilation.
- Runtime claims require testing the exact built commit.
- Rejected builds remain rejected even if later commits appear related.

## 10. Archived guidance

Superseded documents are retained under `raid/archive/` with names that identify them as historical. Archived files are evidence, not active requirements.
