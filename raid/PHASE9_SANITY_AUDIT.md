# Phase 9 Sanity Audit

Audit basis: local `director-scaffold` through `41c5428`. Build #76 is rejected
because its terminal path crashes at runtime; commits after that build remain
unbuilt. This is an evidence ledger, not a recovered wishlist. Historical
concepts are not classified as current missing features until canon explicitly
retains them.

## Executive finding

The integrated DLL is compile-clean at the last pushed build, but the current
work is an **integrated pre-alpha proof**, not feature-complete and not yet a
stable Phase 9 alpha.

The largest risk is not one broken mechanic. It is architectural drift:

- Physical primitives sometimes own encounter decisions.
- Dedicated entity subtypes bypass the Director.
- Several semantic mapper keys are stored in unrelated legacy `edict_t` fields.
- Three different FGD files describe three different public interfaces.
- Temporary player presentation has multiple exit paths with inconsistent
  restoration.
- HUD fields are shared between unrelated systems.
- Some failures are silent.

## Confirmed malformations

### P0 — third-person toggle loses the weapon view model

Evidence: `RaidThirdPerson_Update` sets `ps.gunindex = 0` while enabled.
`RaidThirdPerson_Toggle` destroyed the avatar when toggled off but did not restore
`gunindex`. Carry and forced-presentation exits contained separate restoration
code, so behaviour differed by exit path.

Correction applied locally: one `RestoreFirstPersonWeapon` helper is now used by
manual toggle, carry exit, and presentation exit.

Required verification: toggle on/off repeatedly with several weapons, after
carry, after downed recovery, after death, and after map change.

### P0 — bleedout bypasses downed presentation cleanup

Evidence: bleedout cleared `state.downed` before calling lethal damage. The
ordinary death callback then called `LeaveDowned`, which returned immediately
because the ownership flag was already false. The Insane replacement model and
third-person presentation survived into respawn.

Correction applied locally: leave ownership set until ordinary death cleanup;
zero the downed damage buffer and apply a small lethal hit.

### P0 — bleedout creates gibs instead of a graveyard corpse

Evidence: bleedout applied `100000` damage, guaranteeing a gib-class death and
defeating the intended persistent body-queue graveyard.

Correction applied locally: small lethal damage produces an ordinary corpse.

The same problem remains in the Director `kill_player` operation, which also
uses `100000` damage. `kill_player` needs an explicit death mode with ordinary
death as the default and gib/disintegrate only when requested.

### P0 — terminal activates but cannot display

Evidence: terminal input sets `STAT_RAID_HAT_NAME` to sentinel value `33`.
`G_SetStats` subsequently calls `RaidHats_UpdateHUD`, which clears all three Raid
Hat stats before the client renderer reads them. Movement locks because terminal
state is active, but no terminal is drawn.

Correction applied locally: Raid Hat HUD update is skipped while terminal state
owns those temporary slots.

This is only a compatibility repair. Permanent remediation is a dedicated HUD
presentation mode/fields rather than terminal state masquerading as Raid Hat
state.

### P0 — Raid Hat rank exceeded the fixed network stat array

Evidence: KEX exposes 64 player stats indexed `0` through `63`, but the appended
raid fields placed `STAT_RAID_HAT_RANK` at index `64`. Raid Hat shield/rank and
the terminal's borrowed cursor-Y channel therefore performed an out-of-bounds
read/write. This is a direct candidate cause for both HUD malformation and the
terminal crash.

Local correction: retain Raid Hat name/health in valid slots 62/63, explicitly
reuse otherwise inactive CTF HUD slots for rank and dedicated terminal mode,
cursor, and state fields, and assert every raid stat is below `MAX_STATS`.
Runtime verification remains required.

### P0 — terminal manifest and hardcoded asset disagree

Evidence:

- `terminal_layers.json` names `terminal_controls.png`.
- The last pushed DLL named `terminal_controls_puzzle_v2.png`.
- The latter was a rejected/misaligned artwork variant.

Correction applied locally: renderer uses `terminal_controls.png`. The temporary
asset package duplicates the approved pixels under both names only so the old
DLL can be tested.

Permanent remediation: load the manifest or accept Director-selected terminal
presentation data. Do not hardcode encounter artwork paths in `cg_screen.cpp`.

### P0 — terminal renderer has no asset safety boundary

Evidence: the current client renderer unconditionally draws three external
1224x1285 terminal PNGs. Build #76 packaged the DLL without those assets and
crashes immediately when the terminal is activated.

Asset verification also found that the checked-out `terminal_chassis.png` and
`terminal_master.png` are truncated/corrupt PNGs. The screen and controls alpha
layers are valid, and the intact saved original artwork was recovered.

Required correction: one flattened, renderer-safe runtime texture; an explicit
availability check; a safe rectangle/text fallback; and packaging validation.
Terminal failure must also release movement/input ownership.

### P0 — mapper interface is split across incompatible FGDs

Evidence:

- `fgd/raid.fgd` lacks `trigger_raid_item`, `trigger_raid_terminal`, core charge
  spawnflag, reconstruction options, bot goals, and charged VFX.
- `fgd/jack_merged/q2raid_merged.fgd` includes charging but lacks the terminal.
- `fgd/q2raid.fgd` contains the most complete current definitions.

This directly caused the core to default to charged/Volatile while the mapper
had no visible way to choose the intended uncharged state.

Remediation:

1. Declare one canonical source.
2. Generate merged/distribution FGDs from it.
3. Mark or remove obsolete mapper packs.
4. Add a CI comparison that fails when registered raid classes/keys are absent
   from the canonical delivered FGD.

Local unbuilt correction: `fgd/q2raid.fgd` is the sole hand-maintained Q2Raid
FGD. The obsolete standalone `fgd/raid.fgd` moved to `raid/archive/fgd/`, and
the old merged distribution is marked as a legacy generated snapshot. Automated
DLL/FGD comparison remains outstanding.

### P1 — terminal uses a dedicated direct-opening trigger

Evidence: `trigger_raid_terminal` resolves its `target` and directly calls the
terminal open routine. JSON receives `terminal_open` only after the decision has
already been made.

This is proof code that violates the Director-first target architecture.

Remediation: generic interaction/activation emits an event; JSON issues an
`open_terminal` operation against a terminal primitive. Retain the old class
only as a deprecated compatibility alias during map migration.

Local unbuilt correction: `trigger_raid_interaction` emits `interact`,
`open_terminal` is a validated Director operation, and `trigger_raid_terminal`
is now a compatibility alias whose direct-open path requires the explicit
`LEGACY_DIRECT_OPEN` flag.

### P1 — core state and compatibility are too implicit

Evidence:

- The rejected build encoded initial charge as the inverse of spawnflag 1.
- No explicit state is visible in normal mapper properties when using the stale
  FGD.
- Beam filtering, status application, socket acceptance, and VFX depend on
  several hidden runtime/legacy fields.
- Rejected beam/socket matches usually produce no reason at use time.

Required semantic default:

- Power core: uncharged.
- Beam: charges power cores.
- Core socket: accepts charged power cores.
- Director: decides what `charge_complete` and `deposit` mean for phase logic.

Group names and filters remain optional disambiguation, never baseline setup.

Local unbuilt correction: power cores now begin uncharged and the positive
`START_CHARGED` override authors deliberate precharged/preloaded variants.
Charged-only Volatile application remains enforced by the runtime primitive.

### P1 — semantic configuration is stored in legacy fields

Examples from `g_spawn.cpp`:

- `item_type` and `gadget_type` -> `message`
- `required_state` and relic `weapon` -> `pathtarget`
- `carry_status` -> `deathtarget`
- `held_model` and `spectator_camera` -> `combattarget`
- `status_policy` -> `healthtarget`
- boolean options -> `sounds`
- monster-door counts and modes -> `health`, `dmg`, `style`, `mass`, etc.

Aliases can be tolerated as a transitional parser implementation, but no system
may depend on users understanding those aliases. Typed runtime configuration is
the long-term fix. Validation must operate on semantic names.

### P1 — raw `edict_t` snapshot restoration is high risk

Evidence: reset stores `sizeof(edict_t)` bytes and may restore them with
`memcpy` when an original entity slot no longer exists. `edict_t` contains
pointers, callbacks, ownership links, chain links, and engine lifecycle state.
Even with selected fields restored explicitly for surviving entities, the raw
baseline path can resurrect stale cross-entity references.

Remediation: replace raw snapshots with explicit typed snapshots per supported
entity family, or respawn map-authored primitives from retained spawn data and
then apply semantic state. Add repeat-wipe and map-transition stress tests.

### P1 — wipe timing is hardcoded outside JSON

Evidence: `RaidDirector_OnPartyWipe` fixes reset at 2500 ms. Encounter JSON may
enter a wipe state, but does not own the actual reset delay.

Remediation: JSON/config controls reset timing and presentation; the DLL owns
safe reset execution only.

### P1 — no automated behavioural regression tests

The repo contains proof maps and encounter JSON but no automated test harness
for state ownership or reset invariants. Compiler success did not catch any of
the terminal, bleedout, gib, FGD, or weapon restoration defects.

At minimum, add debug assertions/dump checks for:

- terminal open/close restores movement and HUD ownership;
- third-person enter/leave restores view weapon;
- downed bleedout reaches ordinary death and clears presentation;
- core charge/deposit emits expected events;
- two consecutive wipes restore the same baseline;
- map transition does not retain previous encounter runtime.

## Current subsystem status

| Subsystem | Evidence-based status | Notes |
| --- | --- | --- |
| Director load/state/events/operations | Implemented, partially integration-tested | Central architecture exists and must regain authority over coordination. |
| Monster doors | Implemented proof | Complex runtime; needs repeated deployment/reset tests and mapper simplification review. |
| Raid hats and observation | Implemented proof | Additional behaviour ideas remain design work, not automatically missing commitments. |
| Carry/core/charge/deposit | Implemented but malformed at interface | Defaults, FGD, diagnostics, and Director ownership need correction. |
| Reconstruction | Implemented proof | Needs multiplayer/reset verification. |
| Downed | Implemented proof with local critical fixes | Teammate revival status is not classified here until canonical ledger review. |
| Third-person | Implemented proof with local critical fix | State exit tests required. |
| Terminal | Internal interaction proof; presentation broken in pushed DLL | Local HUD fix requires compile and runtime proof. |
| Quick grenade | Implemented locally, unbuilt | `41c5428` adds `raid_grenade`; requires compile and runtime proof. |
| Historical design-note concepts | Unclassified | Must be marked retained, superseded by Director, deferred, or rejected before counting. |

## Redundant-dependency policy

The following are presumptively redundant unless physical ambiguity proves
otherwise:

- Mandatory target/team strings for a single obvious compatible object.
- Dedicated trigger subclasses whose only difference is the JSON action they
  should cause.
- Map-authored relay/counter/chain logic reproducible by Director states/events.
- Per-encounter C++ branches.
- Duplicate state stored in both a primitive and Director without an explicit
  ownership boundary.
- Hardcoded presentation paths that already have a manifest/JSON home.

## Required next pass

1. Implement the terminal crash guard/fallback, then compile and runtime-test
   the local P0 corrections when explicitly authorized.
2. Establish a canonical feature ledger from post-Director decisions only.
3. Mark every historical note: `retained`, `superseded`, `deferred`, or
   `rejected`.
4. Consolidate the FGD before further mapper testing.
5. Introduce `open_terminal` and semantic item/socket operations in Director;
   deprecate direct terminal trigger wiring.
6. Add map-load diagnostics and extend `raid_dump`.
7. Replace raw reset snapshots before calling the reset system alpha-safe.
8. Proof-test the locally implemented quick-grenade requirement.

## Completion rule

No future answer may say "the whole list is done" without pointing to the
canonical ledger and showing every retained item at its actual maturity level.
A green build proves compilation only.
