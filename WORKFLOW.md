# Q2Raid Integration Workflow

## Permanent repository model

There is one product line: `integration`.

- Only `integration` may be compiled, packaged, uploaded, or runtime-tested.
- A feature branch is temporary isolation for one bounded change. It is never a product line and is never a build source.
- A restore point is an immutable tag created only from a runtime-tested integration build.
- Source presence and compilation are not runtime proof.

## Feature procedure

1. Start from the current `integration` commit.
2. Isolate one feature or defect correction in a temporary worktree or patch.
3. Record the permitted files before editing.
4. Review its complete diff against `integration`.
5. Reject any unrelated file or behavior change.
6. Apply the reviewed change to `integration`.
7. Recheck the complete integration diff and protected-feature ledger.
8. Compile only `integration` after explicit authorization.
9. Runtime-test the integration artifact.
10. After runtime success, create a descriptive immutable restore tag.
11. Remove the temporary feature branch/worktree after its change and evidence are integrated.

## Protected-feature rule

A runtime-complete feature is protected. Work on another feature does not authorize changing its source, project integration, assets, presentation, commands, or runtime behavior.

Before compiling, compare protected files and behavior against the last applicable runtime-passing evidence. Any unexplained difference blocks compilation.

## Restore points

Use descriptive annotated tags:

`restore/YYYY-MM-DD-playtest-NNN`

Each restore tag must identify the exact tested artifact and runtime ledger. A commit that proves only one feature is evidence for that feature, not a whole-project restore point.

## Artifact names

Use:

`Q2Raid_Playtest_NNN_CANDIDATE.zip`

After testing, record `KNOWN_GOOD` or `REJECTED` in the build manifest. Commit hashes belong in the manifest, not the filename.

## Current recovery restriction

The present history contains mixed, incompletely tested corrections. Until reconstructed and runtime-tested:

- do not declare a whole-project last-known-good restore point;
- preserve the grenade runtime-pass commit as feature evidence;
- preserve the presentation-complete commit as feature evidence;
- do not compile directly from either evidence ref;
- reconstruct and review the next candidate on `integration`.
