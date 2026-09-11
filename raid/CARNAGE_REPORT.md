# Q2Raid Carnage Report

**Status:** IMPLEMENTED / UNBUILT / NOT RUNTIME TESTED  
**Date:** 2026-09-04  
**Authority:** `director-scaffold`

## Recovery note

The previously recorded Carnage Report candidate existed only in an ephemeral local workspace and was never pushed to GitHub. Its exact source is no longer recoverable. This implementation is therefore a reconstruction from the preserved day-goal and design requirements; it must not be described as the lost candidate.

All implementation work for this reconstruction is committed directly to GitHub. No local code copy is an implementation authority.

## Contract

The Carnage Report is encounter-scoped wipe presentation.

For each live attempt it tracks:

- hostiles eliminated, derived from the encounter attempt's `level.killed_monsters` delta;
- marine deaths, derived from living-player to dead-player edges;
- mechanic progress, currently represented by non-wipe Director state transitions during the attempt;
- encounter wipe count.

When every non-spectating raid participant is dead, the tracker freezes a wipe snapshot and exposes the report independently to every player.

The report survives the Director's in-place encounter reset. Starting the restored attempt resets the live counters but does not dismiss the frozen report. Each player dismisses their own report with a new FIRE, USE or JUMP input after a short arming delay, preventing the final lethal input from immediately closing it.

## Presentation

The cgame renderer draws:

`THE DARKNESS CONSUMES YOU`

followed by the Carnage Report totals.

The report currently has an engine-primitive fallback presentation because no Carnage Report PNG is present on GitHub `director-scaffold`. The user's prepared image can replace or decorate the fallback once that exact asset is promoted into the repository; code must not invent an asset filename or depend on a local-only file.

## Network state

No player-state protocol slots were added. While `SCREEN_CARNAGE_REPORT` is active, the existing isolated `STAT_RAID_UI_*` slots carry the frozen report:

- `STAT_RAID_UI_CURSOR_X`: hostile kills;
- `STAT_RAID_UI_CURSOR_Y`: marine deaths;
- `STAT_RAID_UI_STATE` low 8 bits: mechanic progress;
- `STAT_RAID_UI_STATE` high 7 bits: wipe count.

Terminal rendering remains screen `1`; Carnage Report is screen `2`. Terminal session state has priority if both are nominally present, preventing simultaneous interpretation of the same stat fields.

## Reset/lifecycle rules

- Director not loaded: Carnage state clears.
- Director newly loaded: new encounter tracking begins.
- Team wipe: snapshot report, increment wipe count.
- In-place wipe recovery: reset live attempt counters when the team becomes alive again; preserve report snapshot until each player dismisses it.
- Disconnect: clear that player's report visibility.
- Map/Director unload: clear all Carnage state.

## Evidence status

Source presence is not runtime evidence.

This implementation is **NOT COMPILED** and **NOT RUNTIME TESTED** until an exact authorized build is produced and tested.
