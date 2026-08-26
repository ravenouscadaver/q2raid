# Q2Raid Design Notes

## Shield-drone laser-field proof encounter

- Build a traversal room blocked by a named laser field.
- Attach a `raid_hat` to the flying laser-firing support monster (exact stock classname to be confirmed from source/wiki).
- Treat it as a **Shield Drone**: killing the hatted monster emits `monster_killed` to JSON.
- JSON disables the laser field and advances the encounter presentation.
- Purpose: prove the missing enemy-owned immunity/shield-gate component needed for the opening encounter emulation.

## Power-core dunk presentation

- Preserve the current satisfying deposit sound as the baseline.
- On successful dunk, briefly force a third-person jump/impact presentation that lands into a short crouch, then returns cleanly to first person.
- Keep input lockout short and deterministic; never leave the player stranded in third person.
- Sound direction: layered *twinkle -> mechanical confirmation -> restrained impact*, rewarding but not intrusive or slot-machine excessive.
- Presentation should be JSON-selectable so ordinary deposits can remain lightweight and major deposits can receive the full animation/sound treatment.

## Gib-gobbling escalation monster

- Prototype a monster or `raid_hat` behaviour that detects nearby physical gibs, moves to them, and consumes them.
- Each consumed gib can grant configurable health, armour, size, damage, speed, a stack counter, or unlock a stronger attack.
- Its threat is legible and interruptible: players see it feeding and can kill, stagger, or displace it before it snowballs.
- Candidate encounter role: an anti-farming pressure unit deployed when a room accumulates excessive remains.
- Gib consumption must distinguish disposable gore from objectives, corpses required by another mechanic, and reset-owned entities.
- Director-facing signals could include `gib_consumed`, `feed_threshold`, and `fully_fed`; JSON decides the consequences.

## Shadow-seeking enemy behaviour

- Explore a reusable `raid_hat` AI preference that scores navigation destinations by darkness and attempts to remain outside brightly lit player sightlines.
- Strong candidate: the tentacled dog-like enemy, used as a lurking ambusher that retreats or relocates through shadow.
- Audit engine-accessible light sampling first; do not infer darkness only from mapper labels if reliable world-light information already exists.
- Allow mapper-authored fallback shadow anchors for deterministic encounter composition and engines/maps where useful light sampling is unavailable.
- Distinguish a soft preference from a hard rule: combat, leash, path safety, and valid navigation must remain authoritative.
- Bright flashes or activated room lighting could temporarily flush these enemies from cover and become encounter counterplay.

## Limited downed and revival state

- Cooperative PvE may grant a marine one downed state before full death and matter reconstruction.
- A downed player can crawl slowly toward cover but cannot fight normally; avoid an extended helpless-state punishment.
- Teammates can perform a committed revive interaction within a configurable window.
- Successful revival keeps the fireteam together but returns the marine in a vulnerable, configurable condition.
- Expiry, severe finishing damage, environmental hazards, or a full-team down advances to actual death/wipe handling.
- Matter reconstruction remains station-based and is the lore-consistent fallback; marines do not possess unlimited personal resurrection devices.
- Encounters can disable downing, modify the timer, limit revives, or apply revive consequences through JSON.
- Preserve clean accounting across downed, revived, dead, spectator, wipe, and reconstruction states.

### Prototype damage gate

- A normally fatal hit that would leave the marine between `0` and `-24` enters the downed state instead of death.
- A hit that crosses directly to `-25` or below causes normal death.
- Any valid damage taken while already downed causes normal death, even if it is only one point.
- Gib/disintegration-class outcomes always cause immediate death regardless of the numerical threshold.
- Internally, keep a separate `downed` flag and safe engine health value so stock `health <= 0` checks do not prematurely classify the player as dead or trigger a team wipe; the HUD may still present zero health.
- Make the `25`-damage overkill allowance configurable after the prototype proves reliable.
