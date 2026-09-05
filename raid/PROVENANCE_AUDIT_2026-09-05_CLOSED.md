# Q2Raid Provenance Audit — Closed

**Date:** 2026-09-05  
**Canonical repository:** `RavenousCadaver/q2raid`  
**Canonical branch:** `director-scaffold`  
**Audit basis HEAD:** `faae89f0baad042a348338660f8d2143e407120e`  
**Status:** `STATIC AUDIT COMPLETE — GITHUB SCOPE`

## Purpose

Establish which Q2Raid implementations, interfaces and historical lineages are actually authoritative before further Director/vertical-slice work. This audit was read-only and compared commit/tree identity rather than trusting branch names.

## Branch reality

The audit found 30 remote branch labels but only 17 distinct branch-tip SHAs. Several scary-looking branch sets were aliases for the same exact code state.

Important alias groups:

- `IMAFUCKINGRETARDCUNTPUTERDONOTFUCKINGOPEN`, `review/terminal-corpse-runtime-candidate`, and `work-unit-1-terminal-bleedout-base` -> the same tip `88502974...`.
- `director-build-70eb159` and `recovery/original-director-scaffold-542e444` -> the same tip `ce1d9589...`.
- `director-build-542e444` and `evidence/presentation-complete-current` -> the same tip `f0e564f4...`.
- `grenade-corpse-review` and `recovery/wu3-event-source-snapshot` -> the same tip `e795d0ce...`.
- `integration` and `recovery/local-integration-8459060` -> the same tip `bd3f4a3e...`.
- Eight differently named `recovery/wu3-*` labels -> the same tip `6969076d...`.

The eight `6969076d...` aliases are particularly misleading: that commit is the pre-event `RaidUI`-compatible interaction bridge, not eight distinct qualified-event implementations.

## Canonical authority

`director-scaffold` remains canonical. `integration`, recovery/evidence refs, build refs and local/scratch work are evidence/recovery material only unless the user explicitly promotes them.

The audit confirmed that branch names are not reliable provenance evidence. Commit/tree identity and runtime evidence outrank labels.

## Protected systems

Static identity checks support preserving these runtime-proven systems:

- **Cores — PASS / FINISHED**
- **Raid Hat name/health/shield — PASS / COMPLETE**
- **Quick grenade — FINAL PASS / LOCKED**

The grenade owning units matched the runtime-pass evidence lineage, and the protected Raid Hat presentation owners matched the presentation-complete evidence lineage. No provenance finding justifies changing them.

## Base Director event bridge

Canonical `director-scaffold` already contains a real DLL -> Director/JSON event bridge.

Current routing is:

`source targetname + signal + optional from_state -> operations -> optional set_state`

`g_trigger.cpp` publishes `activate` before native targets fire. Raid items, terminals, monster doors, Raid Hats, hover markers, reconstruction and helper bots also publish semantic facts through the same bridge.

Therefore the base wireless/JSON path is not missing.

## Qualified event routing

The later WU3 qualified-event work is intentional historical implementation, but it is off-canonical.

Recovered concepts include:

- wildcard/global source subscription;
- `source_tag`;
- `subject` / `subject_tag`;
- `monster_alerted`;
- `monster_arrived`;
- alert/arrival metadata;
- mapper listeners;
- count/window/cooldown/once/consume/dedupe qualification;
- explicit interaction qualification.

The strongest gameplay/source lineage culminates at `bf70733e...`; `work-unit-3-qualified-event-bridge` at `9a8b2a73...` is the same gameplay implementation plus a documentation-only descendant.

WU3 is not a clean isolated feature branch: it also carries terminal/downed/Raid Hat/UI changes. Future recovery must be reviewed hunk-by-hunk, not merged/cherry-picked wholesale.

## `g_ai.cpp`

Current canonical `g_ai.cpp` contains no Q2Raid Director publication hook. Its branch-side differences arrived through the broad `ce08c559...` integration snapshot.

Classification:

- not evidence that `monster_arrived` is canonical;
- not automatically bad code merely because no `raid_*` call is visible;
- do not patch it until the qualified-event work unit is active.

## Optional movement technology

The double-jump / coyote-time / crouch-long-jump block also arrived through a broad integration snapshot, but has explicit design provenance and is intentionally default-off.

It is retained. Poor commit provenance does not make it erroneous implementation.

## FGD and mapper interface

`fgd/q2raid.fgd` is the sole canonical hand-maintained mapper contract.

`fgd/jack_merged/q2raid_merged.fgd` is a generated/distribution derivative and is not independent authority.

The canonical FGD does not currently expose the stranded WU3 qualified-event fields, which means the active mapper contract is not simultaneously publishing two competing qualified-event APIs.

`trigger_raid_interaction` is the canonical generic interaction primitive. `trigger_raid_terminal` is a deprecated compatibility wrapper only.

## Semantic field aliases

`g_spawn.cpp` deliberately maps Q2Raid semantic mapper names onto existing `edict_t` storage. This is intentional transitional architecture, not permission to add synonyms.

The risk is backing-member collision if unrelated semantic keys are mixed on the same entity family. The canonical interface registry records the public key -> backing-member relationship so new fields can be checked before admission.

## Carnage Report

Carnage is not an encounter. It is a wipe/interlude presentation used to obscure the encounter reset, show encounter-authored scoreboard information/hints, and eventually expose useful encounter/boss effectiveness data.

The audit found a bad historical coupling:

`arbitrary Director state transition -> mechanic_progress++`

That is not a valid universal mechanic-completion signal. The post-audit rectification removes this coupling and the fixed `MECHANIC PROGRESS` row while preserving wipe snapshot/display/reset-survival behavior.

Future intended ownership remains:

`engine/encounter facts -> encounter JSON accounting -> generic Carnage rows -> wipe interlude display`

The Director remains authoritative for wipe/reset while Carnage still independently observes all-dead for presentation; this duplicate truth is recorded for later cleanup, not treated as a current runtime failure by itself.

## Terminal ownership conflict

`raid/terminals/entrance_terminal.json` already defines the onboarding prompt, answer `ALPHA`, available keys and submit/reset behavior while runtime/shared UI source also hard-codes that puzzle.

This is a real JSON/runtime ownership duplication. It is recorded as a bounded future terminal-content architecture issue and is not solved by the post-audit rectification.

## Bleedout

The current bleedout correction routes bleedout into ordinary player death ownership and has compiled, but has not yet been runtime-tested.

Status remains:

`COMPILED / NOT RUNTIME TESTED`

Do not rewrite it again before testing.

## Persistent/checkpoint/live Director architecture

Accepted architecture remains:

- persistent-layer JSON;
- hot-swappable encounter JSON;
- distinct live encounter / checkpoint / persistent ownership;
- stable logical IDs;
- deterministic recovery;
- staged BSP registration / `world_ready`;
- no duplicated waves/rewards/events after restore.

The current canonical Director is still one loaded encounter document with one current state plus in-place baseline reset.

A final GitHub-scope search and branch-tip inspection did not recover a complete implementation of the accepted persistent/checkpoint/live split.

Final classification:

- accepted architecture: **ACCEPTED DESIGN / REQUIRED**;
- current canonical implementation of full split: **ABSENT**;
- recoverable GitHub implementation of full split: **NOT FOUND**;
- historical `raid_reset` activity-refresh observation: **PARTIAL_RUNTIME_PASS / SOURCE UNKNOWN**.

This closes GitHub archaeology without claiming an uncommitted historical local build could never have existed.

## Audit closure

Completed:

- branch/commit lineage map;
- same-SHA alias collapse;
- protected-feature checks;
- finite modified code/FGD/JSON inventory;
- per-unit authority classification;
- current mapper/JSON/DLL interface registry;
- current vs off-canonical qualified-event distinction;
- WU3 recovery chain;
- Carnage accounting conflict;
- terminal JSON/runtime duplication;
- optional movement provenance;
- persistence architecture provenance classification.

Remaining work is implementation/runtime testing, not archaeology.

**GitHub mutation during audit:** NONE  
**Branch creation/deletion during audit:** NONE  
**Compile/workflow during audit:** NOT RUN
