# Powered Emissive Surfaces

Status: mapper pattern and future generic operation; no dedicated trigger
subclass required.

## Goal

When a core, beam, generator, or encounter power source is removed, every
associated glowing face and emitted light can fail dramatically and in sync.
The physical presentation must be addressable by JSON without requiring the
Director to understand brush geometry or discover objects through overlap.

## Ordinary mapper pattern

Build one powered fixture from:

1. A permanent base brush using the dull/unpowered material.
2. A thin toggleable `func_wall` overlay using the emissive/powered material.
3. One or more named dynamic or switchable lights that provide the actual cast
   illumination.
4. Optional sparks, sound, particles, or a relay used only as presentation
   targets.

The emissive overlay sits slightly proud of the base rather than occupying the
same plane. This prevents z-fighting when it is visible. When hidden, the dull
face beneath remains.

The ordinary case uses one shared semantic identity, for example:

```text
power_group = regulator_beam_power
```

Mapper-facing tools may expand that group into named objects, but authors should
not manually reproduce multi-target wiring for each fixture.

## Director contract

Primitive: powered presentation group.  
Events: optional `power_enabled`, `power_disabled`.  
Proposed future semantic operation (not currently registered):

```json
{"op":"set_powered", "target":"regulator_beam_power", "value":false}
```

Once implemented, the operation would atomically coordinate the registered members:

- emissive overlay visibility;
- cast/dynamic light enablement or fade;
- loop sound;
- one-shot shutdown sound;
- sparks or failure VFX;
- optional screen/room presentation cue.

The encounter event that causes power loss remains separate:

```json
{
  "source":"beam_power_core",
  "signal":"pickup",
  "do":[{"op":"fire_target", "target":"regulator_beam_power_off"}]
}
```

## Implementation priority

### Zero/new-DLL-cost proof

- Name the emissive `func_wall` and matching light/relay targets.
- Let existing Director `fire_target` operations toggle them together.
- Author a reusable mapper prefab so the wiring is created once, not rebuilt for
  every room.

### Hardened vocabulary

Add generic `set_powered` registration/operation only when it materially reduces
authoring noise. It remains a presentation primitive, not a core-specific C++
branch.

## Dramatic failure permutations

JSON can select sequences without changing the prefab:

- instant hard blackout;
- lights fail down the corridor in order;
- emissive faces extinguish first, cast light decays afterwards;
- intermittent brownout/flicker before permanent failure;
- emergency red group powers up after normal lighting fails;
- one damaged fixture refuses to extinguish cleanly and strobes/sparks;
- restored core reverses the sequence.

## Rejected baseline

Putting every glow face inside a trigger merely so runtime code can search the
overlap is unnecessary for normal authored fixtures. Spatial discovery is
reserved for genuinely dynamic or procedural arrangements. A named semantic
group or generated prefab is clearer, deterministic, and easier to validate.
