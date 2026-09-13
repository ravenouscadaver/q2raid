# Q2Raid Encounter JSON Style Guide

**Status:** Canonical authoring convention
**Scope:** JSON authored for the Q2Raid encounter director
**Purpose:** Keep encounter scripts readable, testable, and maintainable as their complexity grows.

This document governs structure and authoring style. The runtime schema and hardened entity definitions remain the authority on which fields and operations are valid.

The untouched initial convention is archived at `archive/ENCOUNTER_JSON_STYLE_GUIDE_INITIAL.md`. Cross-layer dependency and terminology rules live in `IMPLEMENTATION_STYLE_GUIDE.md`.

## 1. Core principles

1. **JSON describes encounter logic; the map owns physical layout.**
   JSON should coordinate named entities, counters, timers, phases, audio, and effects. Brush dimensions, origins, paths, and ordinary entity wiring belong in the map unless the runtime explicitly requires otherwise.

2. **Prefer explicit state over inferred state.**
   Record important encounter facts as named flags, counters, or phase values. Do not make later behaviour depend on reconstructing what probably happened from several unrelated entities.

3. **One authoritative phase state.**
   An encounter should have a single phase value controlled by the director. Supporting flags may describe mechanics, but must not create competing definitions of the current phase.

4. **Make transitions idempotent.**
   Repeating a phase-entry or reset operation should leave the encounter in the same valid state. Avoid toggles for authoritative state; use explicit `set`, `enable`, `disable`, `start`, `stop`, or `reset` operations.

5. **Keep authored JSON deterministic.**
   Random behaviour must use a declared random-choice operation with named outcomes. Never rely on object ordering, incidental target ordering, or undocumented engine behaviour.

6. **Prefer composition over giant sequences.**
   Reusable actions should become named sequences. Phase handlers should read as an overview of intent, not a dump of every low-level operation.

## 2. File organization

Use one encounter definition per file unless a small shared resource is explicitly supported by the loader.

Recommended layout:

```text
raid/<raid_name>/
  encounters/
    <encounter_id>.json
```

Use lowercase `snake_case` filenames:

```text
power_regulator.json
teleport_chamber.json
```

Do not encode version numbers in filenames. Use source control and a schema/version field where required.

## 3. Top-level order

Keep top-level sections in a consistent conceptual order, even though JSON objects are not semantically ordered:

1. schema/version metadata
2. encounter identity and description
3. defaults or configuration
4. declared state
5. named sequences
6. events, conditions, or transitions
7. reset/wipe behaviour
8. debug metadata

Exact keys must follow the implemented schema. Do not invent structural keys merely to imitate this list.

## 4. Naming conventions

Use lowercase `snake_case` for all authored identifiers.

| Kind | Convention | Example |
| --- | --- | --- |
| Encounter ID | concise noun phrase | `power_regulator` |
| Phase ID | ordered and descriptive | `phase_2_irregular_supply` |
| Event | past-tense fact or clear signal | `core_deposited` |
| Flag | positive state | `critical_environment_active` |
| Counter | plural/countable noun | `cores_deposited` |
| Timer | purpose plus `_timer` | `enrage_timer` |
| Sequence | verb plus object | `begin_critical_environment` |
| Map target | system then role | `regulator_alarm_critical` |
| Random outcome | descriptive result | `spawn_left_heavy_wave` |

Avoid:

- vague names such as `thing1`, `trigger2`, `next`, or `temp`;
- negated flags such as `door_not_locked`;
- names whose meaning depends on comments;
- reusing one identifier for different concepts.

## 5. State ownership

Every important value must have one clear owner.

- The director owns encounter phase, encounter counters, encounter timers, and persistent encounter flags.
- Map entities own their physical or engine-local state.
- A named sequence may change state, but it does not own that state.
- Clients display replicated results; they do not independently advance encounter logic.

Prefer positive booleans:

```json
{
  "critical_environment_active": true
}
```

Instead of:

```json
{
  "critical_environment_disabled": false
}
```

Declare units in field names when the schema does not fix them:

```json
{
  "wipe_cooldown_seconds": 5,
  "arrival_radius_units": 192
}
```

## 6. Phases and transitions

Phase transitions must be easy to audit.

Each phase should have, where applicable:

- a unique ID;
- a single entry condition;
- a named entry sequence;
- an explicit completion condition;
- an explicit next phase;
- defined wipe/reset behaviour.

Do not distribute one phase transition across multiple unrelated event handlers. Funnel multiple valid causes into one named transition sequence.

Use monotonic progression for ordinary raid phases:

```text
setup -> phase_1 -> phase_2 -> phase_3 -> complete
```

If an encounter can regress, loop, or branch, make that exceptional behaviour explicit and document its recovery path.

## 7. Operations

Use one operation object per action. Keep the operation name first, followed by its target, then parameters.

Conceptual example:

```json
{
  "op": "fire_target",
  "target": "regulator_alarm_critical"
}
```

Rules:

- Use only registered operation names.
- Use explicit target names; no wildcard targeting unless formally supported and tightly scoped.
- Never overload one field with several meanings based on value type.
- Do not use numeric magic values where a named enum is supported.
- Keep side effects narrow. If an operation conceptually performs several actions, call a named sequence.
- Prefer semantic operations such as `open_terminal` over firing a map relay that happens to open a terminal.
- An operation target names the primitive that performs the action; it does not encode the encounter consequence in C++.

Current canonical interaction example:

```json
{
  "source": "entrance_terminal_interaction",
  "signal": "interact",
  "do": [
    {
      "op": "open_terminal",
      "target": "entrance_terminal"
    }
  ]
}
```

The map emits `interact`; JSON selects the terminal; the terminal primitive owns input capture and cleanup.

## 8. Named sequences

Create a named sequence when an action group:

- contains more than a few operations;
- is called from more than one place;
- represents a meaningful encounter beat;
- needs an inverse reset/cleanup sequence;
- would obscure a phase transition if written inline.

Good sequence names describe intent:

```text
enter_phase_2
begin_enrage
stop_critical_environment
reset_core_mechanics
```

Avoid sequences that merely hide one trivial operation unless the indirection provides a stable public hook for future expansion.

## 9. Conditions and events

Conditions should test declared state or explicit event data.

Prefer:

```text
when cores_deposited reaches 2
```

Over a compound inference such as:

```text
when left_socket_active and right_socket_active and alarm_inactive
```

Rules:

- Keep conditions side-effect free.
- Use equality for discrete states and threshold comparisons for counters.
- Treat one-shot events and persistent conditions as different concepts.
- State whether a handler may run once, once per phase, or repeatedly.
- Guard repeatable handlers against re-entry while their sequence is already running.

## 10. Timers

Every timer must define:

- what starts it;
- what stops or cancels it;
- what happens on expiry;
- what wipe/reset does to it;
- whether it persists across phase or map transitions.

Use named timers for gameplay-critical timing. Anonymous inline delays are acceptable only for short presentation sequencing where cancellation and persistence do not matter.

Do not build long gameplay timers from chains of delays.

## 11. Randomization

Random choices must be bounded, named, and observable in debug output.

Each random choice should declare:

- its candidate outcomes;
- any weights;
- whether immediate repetition is allowed;
- whether the result must be replicated or persisted;
- its fallback if no candidate is valid.

Randomization chooses between valid authored outcomes. It must not determine whether the encounter enters a valid state.

## 12. Map targets

Treat map target names as an API between the BSP and the encounter script.

- One target name should have one stable purpose.
- Renaming a target requires updating and validating every reference.
- Avoid firing broad relay trees whose consequences cannot be understood from the JSON name.
- Use mapper-facing prefixes consistently within an encounter.
- Targets shared across encounters must be explicitly identified as global.
- Treat `trigger_raid_terminal` as legacy proof wiring. New encounters use `trigger_raid_interaction` plus an `open_terminal` operation.

When possible, validate authored targets against the map's entity list before loading or packaging the raid.

## 13. Reset and wipe safety

Reset is not an afterthought. Every mechanic introduced by a phase must have a defined cleanup path.

A full encounter reset should explicitly restore:

- phase and counters;
- flags and locks;
- timers and queued sequences;
- spawned or parented monsters;
- encounter items and sockets;
- doors, blockers, and shootables;
- lights, fog, audio loops, and effects;
- player-affecting states;
- randomizer history where appropriate.

Reset operations should be safe to run from any phase, including during another sequence or timer callback.

Persistent raid-layer rewards and secrets must be deliberately excluded from encounter reset rather than surviving accidentally.

## 14. Formatting

- UTF-8 without unusual control characters.
- Two-space indentation.
- One key/value pair per line except for genuinely tiny leaf objects.
- No trailing commas.
- Use integers where fractional precision is unnecessary.
- Keep arrays vertically formatted once they contain multiple operations or objects.
- Do not depend on comments: standard JSON has none. Put design rationale in adjacent Markdown documentation or supported metadata fields.
- Run formatting and schema validation before committing.

## 15. Complexity limits

These are review thresholds, not runtime guarantees:

- More than **5 inline operations**: consider a named sequence.
- More than **3 nested logical levels**: extract or simplify the condition.
- More than **1 owner** changing the same state value: redesign ownership.
- More than **1 screenful** for a phase transition: extract presentation and cleanup details.
- Any sequence with both phase advancement and delayed callbacks: document cancellation and re-entry behaviour.
- Any duplicated operation block: extract it before adding a third copy.

## 16. Validation requirements

An encounter file should fail validation before runtime when it contains:

- unknown operations;
- unknown or duplicate authored IDs;
- references to undeclared sequences, counters, flags, or timers;
- invalid enum values;
- missing required fields;
- impossible phase destinations;
- unreachable phases where detectable;
- recursive sequence calls;
- ambiguous state ownership;
- targets absent from the BSP entity list, when map validation is available.

Validation errors should identify the file, JSON path, offending value, and expected form.

## 17. Review checklist

Before an encounter script is accepted:

- [ ] The file passes JSON parsing and schema validation.
- [ ] All identifiers follow the naming convention.
- [ ] The happy-path phase flow can be read from top to bottom.
- [ ] Important state has one owner.
- [ ] Repeated actions are extracted into named sequences.
- [ ] Timers define cancellation and reset behaviour.
- [ ] Random choices have bounded outcomes and fallbacks.
- [ ] Every map target reference is valid.
- [ ] Every phase can be safely wiped and reset.
- [ ] Persistent rewards/secrets are intentionally preserved or cleared.
- [ ] Debug output can identify the active phase, important counters, timers, and last transition.

## 18. Evolution rule

When a new encounter requires breaking one of these conventions, do not silently work around the guide. Record the exceptional requirement, decide whether the schema or this guide should change, and update the guide alongside the implementation.

The goal is not merely valid JSON. The goal is encounter code that remains understandable when the raid contains many phases, mechanics, branches, failures, and persistent layers.
