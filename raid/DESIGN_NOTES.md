# Q2Raid Design Notes

## First-door terminal onboarding

- Place the first functional terminal immediately before the first locked door,
  outside combat pressure.
- Use it to teach cursor movement, keyboard interaction, pressed/dim feedback,
  submit, and cancel before terminals appear during encounters.
- Make the first task short and forgiving: reconstruct a simple callsign such as
  `ALPHA`; success emits a semantic action and encounter JSON opens the door.
- This establishes the mod's thesis immediately: the machinery is recognisably
  Quake II, but the expected play vocabulary is not ordinary Quake II.
- Full terminal shell, puzzle permutations, art contract, and camera composition
  are defined in `TERMINAL_SYSTEM.md`.

## Powered emissive fixtures

- Pair a dull permanent base face with a slightly offset toggleable emissive
  `func_wall` overlay and its actual light sources.
- Existing Director `fire_target` can prove synchronized shutdown immediately; later a
  generic `set_powered` group can coordinate visibility, light, sound, and VFX.
- Do not require an overlap trigger merely to discover ordinary authored
  fixtures. See `POWERED_SURFACES.md`.

## Modular BSP kit generator

- Author simple reusable tile brushes in a source `.map`, enclosed by bounds
  volumes and marked with generator-only connector textures/metadata.
- An offline constraint solver assembles an editable `.map` blockout, preserving
  its seed and stripping generator markers.
- Generated geometry is an inspiration/cleanup substrate for JACK, not runtime
  procedural generation. See `MODULAR_BSP_GENERATOR.md`.

## Shield-drone laser-field proof encounter

- Build a traversal room blocked by a named laser field.
- Attach a `raid_hat` to the flying laser-firing support monster (exact stock classname to be confirmed from source/wiki).
- Treat it as a **Shield Drone**: killing the hatted monster emits `monster_killed` to JSON.
- JSON disables the laser field and advances the encounter presentation.
- Purpose: prove the missing enemy-owned immunity/shield-gate component needed for the opening encounter emulation.

## Held-open circuit bypass

- Present a malfunctioning powered door with its access panel visibly torn away and broken wiring exposed.
- Implement the interaction with a small invisible hold volume at normal use distance; the mapper-facing fiction is repairing the circuit, not standing on a floor plate.
- While a player maintains the interaction, force the existing third-person unarmed presentation and suppress weapon use. Releasing, displacement, downing, death, or wipe must immediately end the interaction and restore normal presentation.
- Fire intermittent sparks from the exposed panel and show a development message such as `DOOR CIRCUIT TEMPORARILY REPAIRED` as immediate feedback. Final presentation should communicate the same state without relying on debug text.
- Keep the door open only while the interaction remains valid, creating a committed operator role while teammates transport an objective through it.
- Pair it with a Volatile power core whose socket cannot normally be reached within its 15-second carry window. Holding the bypass opens the shorter viable route.
- Combine the transport pressure with Shield Drones, laser fields, Volatile consequences, and monster-door population so the opening encounter demands coordination rather than becoming an uncontested carry exercise.
- Generalize the logic as a JSON-controlled hold interaction so it can later represent jammed shutters, exposed relays, coolant valves, manual pumps, or other sustained repairs.

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

## Optional crouch long-jump

- Add an opt-in Half-Life-style movement permutation: a deliberate crouch-and-jump input while grounded and already moving launches the player in a long jump.
- Derive the horizontal launch vector from the player's current planar movement direction, not merely their view angle. Require a configurable minimum input/velocity so it cannot launch from rest.
- Keep stock Quake II movement unchanged by default. Expose the feature and its forward speed, vertical impulse, and optional cooldown as mapper/server configuration.
- Require a fresh jump press and a valid grounded/coyote-grounded state; merely holding crouch in the air must never retrigger it.
- Consume the jump cleanly so it cannot accidentally compound with the optional double jump unless an encounter explicitly permits that combination.
- Preserve ordinary player collision and knockback during launch and landing. Teammates impatiently crowding a traversal lane can therefore bump one another off course as emergent cooperative chaos.
- Provide clear launch/landing feedback and deterministic server-authoritative behaviour so clients agree on the arc.
- Initially allow direct player/server enablement for testing; later present it as **Strogg leg actuators** granted through the general implant/status system.

## Strogg implants as a player-facing mechanic family

- Treat **Strogg Implant** as a reusable lore layer rather than the permanent name of one narrowly defined buff.
- Individual implants may grant long-jump actuators, safe passage through disintegration fields, movement changes, environmental tolerances, sensory changes, weapon interactions, or other encounter permissions.
- Let JSON define the implant's mechanical capabilities, duration, acquisition, loss rules, HUD label, and presentation while the DLL supplies reusable hooks.
- Prefer specific player-facing names where clarity matters, with `Strogg implant` retained as the shared category/source. This avoids making unrelated mechanics feel like arbitrary game permissions without forcing every implant to behave identically.

## Raid Hat HUD presentation status

- Current monster name and red health-bar presentation is accepted as implemented: scale, readability, and targeting behaviour are suitable.
- Segmented Destiny-style bars and the earlier text-formatted bar remain optional presentation polish, not blockers for the Raid Hat system.

## Observer-Locked Raid Hat controls

- Preserve the proven default relationship: a watched monster freezes and an unobserved monster may move.
- Add a live targetable/JSON-controlled inversion switch. While inverted, the monster may move while watched and freezes when every player looks away, forcing the fireteam to deliberately play "look away."
- Expose separate observation timing controls: delay before movement is released after the relevant observation change, delay before freezing resumes, and an optional minimum active movement window.
- Evaluate observation across all living players: the current rule or its inverse must use the combined fireteam view state, not only the nearest player.
- Emit a clear event when inversion changes so JSON can synchronize lights, sound, messaging, and encounter state with the rule swap.

## Deferred custom-model production

Status: CONCEPT / deferred — not an active day goal.
Date: 2026-09-05.

- Possible custom model for Dr Oliver; role, appearance and required animation set remain undecided.
- Consider a couple of bespoke Raid Hat enemy models where their presentation materially matters to gameplay.
- The user has hard-surface modelling experience in Blender; rigging and animation are the main production bottleneck. This constraint currently favours passive implant abilities and feedback using available particles, beams and other existing presentation.
- Contractor lead to revisit: [Iammr R / iammrromeo on Fiverr](https://www.fiverr.com/iammrromeo/create-3d-game-character-and-game-mod-with-custom-animation-for-skyrim-unity-ue). The listing advertises low-poly assets, rigging, custom animation and game-mod work. This is an unverified lead, not a vetted hire or evidence of MD2 expertise; no contact or commission has been made.
- Proposed qualification gate, if this work is activated: review actual animation examples and agree source-file delivery, then commission a small Blender animation test before a complete character. Test fixed topology, named frame ranges, MD2 export and playback inside Q2Raid.
- Keep editable Blender source separate from the final exported MD2. Exporter/Blender compatibility and the complete conversion pipeline remain NOT TESTED; do not treat a listing or available exporter as proof.
- No contractor spend, asset production or implementation is authorized by recording this note.
