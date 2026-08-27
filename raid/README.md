# Raid Director

Architecture and current maturity are documented in:

- `DIRECTOR_FIRST_DOCTRINE.md` — canonical ownership and authoring rules.
- `PHASE9_SANITY_AUDIT.md` — evidence-based defects, risks, and next gates.

The raid Director is a single server-authoritative runtime. Encounter JSON is
loaded only by the game DLL; clients receive ordinary replicated gameplay
results rather than executing their own Director.

## Current console commands

Commands are issued through the Quake II server command entry point:

```text
sv raid_load <path>
sv raid_reload
sv raid_dump
sv raid_set_state <state>
```

The server cvars are:

```text
raid_script       Encounter JSON path; empty by default
raid_script_root  Directory prepended to relative paths; `.` by default
raid_autoload     Load `raid_script` after map entities spawn; enabled by default
```

Example dedicated-server configuration:

```text
+set raid_script_root "C:/quake2/raidmod"
+set raid_script "raid/encounters/director_bridge_test.json"
```

The Director validates encounter documents, changes state, executes supported
state-entry operations, receives gameplay events, and manages encounter reset.
That is an implemented integration proof, not a claim of feature completeness;
consult the Phase 9 audit for known gaps and unverified paths.
