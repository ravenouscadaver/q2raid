# Qualified raid event routing recovery

Status: IMPLEMENTATION READY / UNBUILT
Date: 2026-09-04

Recovery source: `9dbe2a00b16bb21e9d777e488c36f583ef99b2b3` with numeric parsing correction `e795d0ce7f8202acd6698126658ddb98cd44a976`.

This branch is recovering the existing qualified Director event bridge onto the current `rectify/terminal-state-director` line without making the stale `integration` branch canonical.

Required recovered routes: `monster_alerted`, `monster_arrived`, `monster_death`, `player_death`, explicit `interact`, generic Director notifications, guarded queued dispatch, multiple matching JSON listeners, wildcard source matching, and source/subject/tag qualification.

Current protected systems remain authoritative: cores, name/health/shield presentation, grenade, and the current RaidUI terminal/bleedout implementation. Historical terminal presentation is not part of this recovery.

Projectile/explosion/BFG event normalization is intentionally deferred until the recovered bridge is statically reconciled. No encounter-specific BFG behavior belongs in this work unit.

No compile or runtime claim is made by this note.