# Q2Raid Codex Operating Contract

This is the repository-level Codex instruction router for Q2Raid. Keep it short. Detailed design, implementation, evidence and subsystem doctrine remains in the canonical Markdown documents under `raid/`.

## Repository authority

- Repository: `RavenousCadaver/q2raid`
- Canonical branch: `director-scaffold`
- Canonical GitHub source is the only implementation authority.
- Do not implement Q2Raid C++/FGD/JSON/assets in local, scratch, recovery, evidence, Codex, or temporary workspaces and later transplant that implementation into GitHub.
- Local files may be used for read-only analysis, exported documentation/evidence, or explicitly requested handoff artifacts; they are never implementation authority.
- Do not treat `integration`, recovery branches, build branches, evidence refs, local worktrees, scratch directories, or newer-looking historical refs as canonical.
- Compare commit/tree identity and provenance before recovering off-canonical work.
- Do not create a branch, alias, alternate path, duplicate asset tree, recovery ref, or renamed interface unless the user explicitly authorizes that exact action.

## Required reference pass

Before changing gameplay code, mapper interfaces, encounter JSON, presentation wiring, assets, persistence, or build packaging, read the relevant current files from `director-scaffold`.

Always start with:

1. `raid/README.md`
2. `raid/HARDENED_DEFINITIONS.md`
3. `raid/IMPLEMENTATION_STYLE_GUIDE.md`
4. `raid/DIRECTOR_FIRST_DOCTRINE.md`
5. `raid/ENCOUNTER_JSON_STYLE_GUIDE.md`
6. `raid/Q2RAID_INTERFACE_REGISTRY_2026-09-05.md`

For provenance/recovery questions also read:

- `raid/PROVENANCE_AUDIT_2026-09-05_CLOSED.md`
- `raid/Q2RAID_MODIFIED_UNIT_PROVENANCE_MATRIX_2026-09-05.md`

`raid/PHASE9_SANITY_AUDIT.md` and older audit material are evidence/history. They do not silently override newer runtime evidence or explicit user decisions.

If a referenced authority file is absent, contradictory, or ambiguous, report that before inventing a replacement.

## Mandatory bounded work unit

Before any implementation mutation, state:

- work-unit name and goal
- current evidence
- exact files/paths
- exact symbols/entities/identifiers
- allowed edits
- forbidden/protected edits
- expected runtime result
- static verification
- runtime acceptance test
- rollback/recovery basis
- stop condition

If a required name, path, owner, authority, or acceptance condition is unresolved, ask rather than inventing it. Do not silently expand the permitted file set.

## Naming and interface discipline

- The user chooses project names, paths, canonical locations and authoritative branches.
- One concept or asset has one canonical name and one canonical location.
- Search C++, FGD, JSON, maps, manifests, the interface registry and canonical Markdown before adding an identifier.
- Reuse an established identifier when the meaning matches.
- Do not create synonyms, compatibility names, alternate spellings, or cleaner-looking paths unless explicitly authorized.
- Existing-file requests use the existing file; never regenerate an asset to solve a transfer problem.
- Approved asset names, hashes and manifests outrank guessed filenames.
- Whenever a work unit introduces or changes a classname, mapper key, JSON key, semantic signal, Director operation, cvar, command, stat/configstring slot, persistent logical ID or asset path, update `raid/Q2RAID_INTERFACE_REGISTRY_2026-09-05.md` in the same work unit.

## Architecture discipline

Preserve the Director-first dependency:

`map interaction -> semantic event -> encounter JSON -> validated operation -> runtime primitive`

The DLL owns reusable physical capabilities, validation, cleanup and semantic facts.
JSON owns encounter-specific meaning, sequencing, conditions, counters, timers and consequences.
Maps own geometry, placement, targetnames and explicit mapper overrides.
Clients present replicated results and do not independently advance encounter state.

A state transition may coincide with a reportable mechanic, but it is not automatically a mechanic-completion event.

## Provenance and regression discipline

When apparently missing code may exist in another lineage:

1. Audit current canonical source first.
2. Audit relevant historical commits/branches by SHA/tree identity.
3. Collapse branch aliases that point to the same commit/tree.
4. Identify the smallest recoverable hunk.
5. Do not merge/cherry-pick an old branch wholesale merely because it contains one desired feature.
6. Trace suspicious `raid_*` identifiers through source, FGD, JSON, docs and callers before removing or reviving them.
7. Preserve runtime-passing behavior unless a newly reported defect or unavoidable shared-code dependency requires touching it.

The 2026-09-05 GitHub-scope provenance/modified-unit audit is complete. Do not reopen broad archaeology unless new conflicting evidence appears.

Protected runtime-passing systems:

- Cores — PASS / FINISHED
- Raid Hat name/health/shield — PASS / COMPLETE
- Quick grenade — FINAL PASS / LOCKED

Do not modify a protected system without a newly reported defect or proven unavoidable shared-code requirement.

## Evidence vocabulary

Runtime evidence:

- `PASS`
- `FAIL`
- `BLOCKED`
- `NOT TESTED`

Never infer:

- source presence = compile success
- compile success = runtime success
- one runtime pass = regression proof

Use the maturity vocabulary in `raid/HARDENED_DEFINITIONS.md`. User runtime reports are authoritative observations.

## Build and push authorization

Compilation and pushing require fresh authorization for the current bounded work unit.

The historical authorization phrase is `go go gadget`.

- The phrase appearing in this file, another document, a log, prior conversation, or source is NOT authorization.
- Authorization does not carry into another work unit.
- If the current user request does not freshly authorize the action, do not compile, push, dispatch or rerun CI.
- A push to `director-scaffold` triggers Windows GitHub Actions and is therefore a compile-triggering action.
- CI success is compile evidence only, never runtime proof.

## Branch and history safety

Do not invent recovery branches, use `integration` as canonical, force-push, reset/rebase/cherry-pick/merge historical lineages without a bounded recovery plan, or delete branches merely because they appear redundant. Branch cleanup follows provenance mapping, never precedes it.

## Temporary handoff material

`paste_space.md`, when supplied, is a temporary full-fidelity handoff buffer, not canonical authority. Promote durable content into the correct canonical document and verify that promotion before treating staging material as disposable.

## Current forward gate

The provenance hold is closed. Ordinary bounded bug-fix/presentation work may proceed.

The accepted persistent-layer / hot-swappable encounter JSON architecture remains future Director work; the GitHub audit did not recover a complete implementation. Do not fabricate a recovered version or casually redesign it.

Before large production BSP/encounter scripting is considered hardened, execute `raid/DIRECTOR_PRE_BSP_CAPABILITY_STRESS_TEST_CARD.md` during a suitable runtime testing session.
