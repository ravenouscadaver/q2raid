# Q2Raid — Director Pre-BSP Capability Stress-Test Card

**Purpose:** future runtime testing-session checklist  
**Status:** `DEFERRED UNTIL CURRENT BUGS ARE KNOCKED OUT AND THE TEST SLICE IS READY`  
**Rule:** this card does not block present bounded bug/presentation work.

## Goal

Before committing to extensive production BSP and encounter scripting, stress-test whether the Director/JSON layer can express, reset, persist, diagnose and recover the kinds of encounter logic Q2Raid intends to use.

## Capability matrix

| Capability | Question to prove | Evidence |
|---|---|---|
| Event ingress | What engine/game facts can JSON actually receive? | NOT TESTED |
| Event qualification | Can events be qualified without bespoke encounter code? | NOT TESTED |
| Encounter state | Is live state explicit and deterministic? | NOT TESTED |
| Counters / flags | Can mechanics maintain explicit state without abusing phase transitions/entities? | NOT TESTED |
| Timers | Can named timers start/cancel/reset/restore safely? | NOT TESTED |
| Operations | What generic consequences can JSON cause reliably? | NOT TESTED |
| Randomisation | Can bounded/random branches behave deterministically enough for encounter use? | NOT TESTED |
| Wipe reset | Does wipe fully restore without duplicate entities/rewards/events? | NOT TESTED |
| Checkpoints | Can committed checkpoints resume without replaying completed work? | NOT TESTED |
| Persistent raid state | Can implants/unlocks/secrets survive encounter/map boundaries independently of live state? | NOT TESTED |
| Hot swap / reload | Can encounter JSON reload without corrupting persistent/checkpoint state? | NOT TESTED |
| BSP registration | Are mapper interfaces validated before encounter start / `world_ready`? | NOT TESTED |
| Save/load | What state actually survives native game/level save paths? | NOT TESTED |
| Diagnostics | Can phase/state, last event, timers/loops, missing IDs and reset confirmation be inspected? | NOT TESTED |
| Presentation data | Can JSON expose Carnage rows/hints/encounter-specific presentation explicitly? | NOT TESTED |
| Failure semantics | What happens when targets/events/states are missing or an operation fails? | NOT TESTED |

Allowed evidence labels:

- `PROVEN`
- `IMPLEMENTED-NOT-PROVEN`
- `PARTIAL`
- `DOCUMENTED-ONLY`
- `MISSING`
- `CONFLICTING`

## Suggested synthetic stress encounter

When the vertical slice is stable enough, exercise several systems together:

- explicit phase/state progression;
- concurrent timer;
- random branch;
- qualified monster/event path;
- wipe during active logic;
- full in-place restoration;
- checkpoint commit/recovery;
- one persistent value surviving wipe/encounter boundary;
- encounter JSON reload/hot-swap;
- one encounter-authored Carnage field;
- one missing/invalid target test;
- proof no wave/reward/event/pickup/mechanic duplicates after restore.

## Carnage note

Carnage Report is not an encounter and must not infer reportable mechanics from arbitrary Director state transitions.

Target relationship:

`engine/encounter facts -> encounter JSON accounting -> generic Carnage rows -> wipe interlude display`

## Completion gate

Do not call the Director/JSON layer hardened for production BSP scripting until this card has recorded runtime evidence and architectural blockers have been resolved.
