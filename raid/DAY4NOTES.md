# Q2Raid Day 4 Notes — Integration Failure and Recovery Ledger

Date: 2026-08-27  
Status: canonical record of the current local state at `41c5428`.

## Executive summary

Day 4 integrated the terminal, lifecycle corrections, Phase 9 sanity maps,
core progression proof, HUD corrections, corpse correction, and one-button
grenades. It also exposed severe process and architecture drift.

Build #76 compiled successfully but is **rejected for testing** because the
terminal crashes immediately. Green CI did not prove runtime correctness.
Several later corrections exist only in local commits and have not been built
or runtime-tested.

## Hard workflow rule

Only the exact phrase **`go go gadget`** authorizes a compile or push.

- “Next build,” “queue this,” “please,” “sanity pass,” or discussion of a build
  is not authorization.
- Work remains local until the exact phrase is given.
- Before an authorized build, report the included commit range and unresolved
  P0 items.
- A build must never silently replace a requested review or sanity pass.

## Build and commit ledger

| Item | Status | Meaning |
| --- | --- | --- |
| Build #76 / remote `f4458e7` | Rejected | CI passed; terminal crashes immediately; do not continue testing it. |
| `d19b595` | Local lineage | Director-first doctrine and shared first-person weapon restoration. |
| `45d1891` | Local lineage | Earlier shield restoration and sanity map; later presentation superseded. |
| `4f92ad6` | Local, unbuilt | Downed players cannot pick up raid items. |
| `c500b28` | Local, unbuilt | Core phase JSON and preloaded-core proof files. |
| `fa5e500` | Local, unbuilt | Latest accepted rank colours, shield overlay, and ordinary corpse correction. |
| `41c5428` | Local HEAD, unbuilt | One-button `raid_grenade` implementation. |

## Latest accepted fixes

### Terminal

Observed behaviour: interaction activates and movement locks, but presentation
either fails or the pushed DLL crashes.

Confirmed current defect: `cg_screen.cpp` unconditionally draws three external
1224x1285 PNG layers. Build #76 did not package those assets, and there is no
availability guard or renderer-safe fallback.

Later source verification found an additional, more fundamental P0: the fixed
64-entry network stat array was indexed at 64 by `STAT_RAID_HAT_RANK`. Terminal
cursor Y borrowed that invalid slot. The checked-out chassis/master PNGs are
also corrupt, while screen/controls are valid. Local post-review work relocates
rank and dedicated terminal state into valid unused CTF HUD slots and adds a
safe composite fallback; this remains unbuilt.

Latest accepted remediation, not yet implemented:

1. Flatten the approved terminal artwork into one lower-resolution runtime
   texture.
2. Check availability/renderer constraints before drawing it.
3. Fall back to safe rectangle/text presentation if unavailable.
4. Package required runtime assets beside the DLL.
5. Give terminal presentation dedicated ownership instead of borrowing Raid Hat
   stat slots.
6. Generic interaction emits an event; JSON issues `open_terminal`. Direct
   `trigger_raid_terminal` is deprecated proof code, not the target design.

The approved controls artwork is `terminal_controls.png`. Rejected or duplicate
variants must not silently become canonical.

### Raid Hat name, health, and shield

Latest accepted presentation is in `fa5e500`, unbuilt:

- Name tag retains independent rank colours: white, red, blue, purple.
- Shield is a separate cyan font batch of 15 periods.
- It is drawn directly over/in front of the health bar, not above it.
- Empty capacity is dim cyan; current shield is bright cyan.
- Shield colour must never recolour the name tag.

Earlier tiny, malformed, name-coloured, or name-attached shield variants are
rejected.

### Quick grenade

Latest accepted implementation is `41c5428`, unbuilt:

- Client command: `raid_grenade`.
- Recommended binding: `bind g raid_grenade`.
- Throws immediately without changing the equipped weapon.
- Consumes one grenade unless infinite ammo is enabled.
- Blocked while dead, spectating, downed, carrying a weapon-blocking heavy item,
  or already priming/recovering a hand grenade.
- Current constants: 125 base damage, 800 speed, 2.5-second fuse, one-second
  recovery.

This was a confirmed Destiny-like checklist requirement. Earlier statements
that the entire list was implemented while it was absent were false.

### Downed, bleedout, corpse, and respawn

Accepted corrections currently in local lineage:

- Downed players cannot collect raid items.
- Bleedout retains downed ownership until ordinary death cleanup runs.
- Bleedout uses a small lethal hit, not `100000` gib damage.
- `PMF_DUCKED` is cleared before lethal damage so `player_die` selects a normal
  standing death sequence rather than the crouched corpse sequence.
- Third-person replacement state and view weapon must be restored on every exit.
- Downed audio should rotate appropriate pain/help sounds and exclude the
  counter-intuitive “kill me” line.

Still requiring runtime proof: repeated bleedout/respawn, death-pose variation,
model restoration, first-person weapon restoration, and non-gib graveyard
corpses.

### Third person

`RestoreFirstPersonWeapon` is now shared by manual toggle, carry exit, and
forced-presentation exit. It remains locally corrected but requires repeated
runtime tests across weapons, carry, downed, death, respawn, and map change.

### Core, beam, socket, and phase proof

Accepted ordinary semantics:

- A power core intrinsically begins uncharged unless explicitly authored
  otherwise.
- A charging beam charges a carried power core.
- Only a charged core receives Volatile by default.
- A socket intrinsically accepts a charged power core.
- The socket emits `deposit`; JSON decides phase progression.
- Group/team/filter strings exist only for genuine ambiguity or unusual
  overrides.
- Invalid use must print a semantic reason instead of silently doing nothing.

`core_phase_progression_test.json` proves the intended state sequence on paper:
three deposits advance phases, phase 3 applies Doomsday, and completion clears
it. `preloaded_core_test.map` is a physical proof asset, but initial socket
occupancy is not yet a formally registered runtime relationship.

The current interface remains malformed: charge defaults are encoded through an
inverted spawnflag, legacy fields carry semantic data, diagnostics are weak,
and obsolete FGDs can hide required properties. Do not treat the current clean
entity entries as proof of clean runtime semantics.

## Repeated failures and prevention

| Repeat failure | Actual prevention |
| --- | --- |
| “Whole list done” after a green compile | Canonical feature ledger with maturity states; cite every retained item. |
| Unrequested compile/push | Exact `go go gadget` authorization gate. |
| FGD fixed repeatedly in different files | `fgd/q2raid.fgd` is the sole canonical source; generate distributions from it and mark others obsolete. |
| Fixing presentation breaks unrelated HUD | One explicit owner per HUD/presentation mode; no borrowed slots. |
| Exit path leaves model/camera/weapon state behind | One idempotent cleanup function per owning subsystem; all exits call it. |
| Mapper wiring grows despite JSON Director | Primitive emits event; JSON decides consequence; no outcome-specific trigger subtype. |
| Missing resources fail silently or crash | Validate paths and dimensions, report one clear error, provide safe fallback. |
| Build proves compilation but masks runtime regression | Separate compile-clean, proof-tested, and integration-tested statuses. |
| Old source patterns override project doctrine | Treat legacy code as engine reference only; apply the Director ownership test first. |

## Finite last structural DLL batch

Before Q2Raid moves primarily to JSON/maps/assets, the remaining structural DLL
work is:

1. Terminal-safe renderer, flattened asset, fallback, packaging, and cleanup.
2. Runtime proof of the accepted shield/name presentation.
3. Runtime proof of `raid_grenade`.
4. Explicit core charge defaults, charged-only Volatile, socket acceptance,
   initial occupancy, semantic diagnostics, and Director events/operations.
5. Downed audio, ordinary bleedout corpse, varied death pose, pickup block, and
   complete respawn cleanup.
6. Third-person restoration across every exit path.
7. Canonical FGD consolidation and DLL/FGD mismatch validation.
8. Generic terminal interaction plus Director `open_terminal`; deprecate the
   direct-opening subtype.
9. Replace unsafe raw `edict_t` reset snapshots with typed restoration before
   calling reset alpha-safe.
10. Add behavioural assertions/dumps for repeat activation, wipe, disconnect,
    and map transition.

Weeping Strogg improvements and cinematic/demo camera work are retained feature
work, but they do not have to block the immediate corrective build unless the
canonical ledger promotes them into that batch.

## Immediate next build acceptance

The next build should not begin until terminal crash remediation is implemented
and the exact authorization phrase is supplied. Its minimum runtime checklist:

- terminal opens, renders or safely falls back, completes/cancels, and restores
  movement;
- shield periods overlay the health bar while name rank colour remains intact;
- `bind g raid_grenade` throws once, consumes ammo, and preserves the weapon;
- third-person toggle restores the weapon model;
- bleedout produces a normal corpse and clean respawn;
- downed players cannot collect raid items.

Passing this checklist would make those paths proof-tested, not automatically
feature-complete or alpha-ready.

## Integration and dependency-cleanup checkpoint

The local implementation line (`7e48698`) and remote/documentation line
(`da532ac`, containing remote `f4458e7`) were joined by integration commit
`f1f09eb`. The merge records both histories while retaining the reviewed local
tree byte-for-byte. Backup refs preserve all three pre-integration tips.

The follow-up cleanup established these rules in code and mapper interfaces:

- `trigger_raid_interaction` emits the generic `interact` event.
- Encounter JSON selects a terminal with validated `open_terminal`.
- `trigger_raid_terminal` remains only as a deprecated compatibility alias;
  direct opening requires the explicit `LEGACY_DIRECT_OPEN` flag.
- Ordinary power cores begin uncharged. `START_CHARGED` is the positive map
  override for deliberate precharged/preloaded configurations.
- Downed players cannot activate generic interaction volumes.
- `fgd/q2raid.fgd` remains the only hand-maintained Q2Raid FGD. The obsolete
  standalone `fgd/raid.fgd` snapshot moved to `raid/archive/fgd/`.

The initial encounter JSON guide is retained unchanged under `raid/archive/`.
The active guide and `IMPLEMENTATION_STYLE_GUIDE.md` now document authority,
terminology, dependency direction, compatibility exceptions, and lifecycle
pairing. Archived guidance is historical evidence, not an active requirement.
